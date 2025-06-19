//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//

using CameraKsPropertyHelper;
using ControlMonitorHelperWinRT;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Windows.ApplicationModel;
using Windows.Media.Capture;
using Windows.Media.Capture.Frames;
using Windows.Media.Core;
using Windows.Media.Playback;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Navigation;

namespace OutboundSettingsAppTest
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class MainPage : Page
    {
        string m_cameraId = null;
        private MediaCapture m_mediaCapture = null;
        private MediaPlayer m_mediaPlayer = null;

        private DefaultControlHelper.DefaultControlManager m_controlManager = null;
        private DefaultControlHelper.DefaultController m_contrastController = null;
        private DefaultControlHelper.DefaultController m_brightnessController = null;
        private DefaultControlHelper.DefaultController m_backgroundBlurController = null;
        private DefaultControlHelper.DefaultController m_hdrController = null;
        private DefaultControlHelper.DefaultController m_evCompController = null;
        private ControlMonitorHelperWinRT.ControlMonitorManager m_controlMonitorManager = null;

        private SemaphoreSlim m_lock = new SemaphoreSlim(1);
        private bool m_isInitialized = false;
        private List<string> m_cameraIdList = new List<string>();

        public MainPage()
        {
            InitializeComponent();

            // register some lifecycle handling callbacks
            Application.Current.EnteredBackground += Current_EnteredBackground;
            Application.Current.LeavingBackground += Current_LeavingBackground;
        }

        /// <summary>
        /// Upon leaving background, uninitialize
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private async void Current_LeavingBackground(object sender, LeavingBackgroundEventArgs e)
        {
            if (m_cameraId != null)
            {
                await InitializeAsync();
            }
        }

        /// <summary>
        /// Upon leaving foreground, initialize
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void Current_EnteredBackground(object sender, EnteredBackgroundEventArgs e)
        {
            m_lock.Wait();
            if (!m_isInitialized)
            {
                m_lock.Release();
                return;
            }

            try
            {
                if (m_mediaPlayer != null)
                {
                    m_mediaPlayer.Dispose();
                    m_mediaPlayer = null;
                }

                if (m_mediaCapture != null)
                {
                    m_mediaCapture.Dispose();
                    m_mediaCapture = null;
                }

                m_controlMonitorManager = null;

                m_isInitialized = false;
            }
            finally
            {
                m_lock.Release();
            }
        }

        /// <summary>
        /// Upon lauching the app and navigating to this page.
        /// </summary>
        /// <param name="e"></param>
        protected override async void OnNavigatedTo(NavigationEventArgs e)
        {
            // This app is expecting to be launched via Camera Settings page association
            // which will include the camera symbolic link name in the navigation arguments
            string launchArgs = e.Parameter?.ToString();
            if (launchArgs != null || launchArgs.Length != 0)
            {
                var argsArr = launchArgs.Split(' ');
                int argIndex = 0;

                foreach (var arg in argsArr)
                {
                    if (String.Compare(arg, "/cameraId", StringComparison.InvariantCultureIgnoreCase) == 0)
                    {
                        // "/cameraId" argument found
                        m_cameraId = argsArr[argIndex + 1];
                        break;
                    }
                    argIndex++;
                }

                // if the app was launched without the /cameraId parameter, then offer the choice to select
                // a camera that would be configured properly in registry with this app set as companion app
                if (m_cameraId == null)
                {
                    try
                    {
                        var frameSourceGroups = await MediaFrameSourceGroup.FindAllAsync();
                        var videoCaptureDevices = await Windows.Devices.Enumeration.DeviceInformation.FindAllAsync(Windows.Media.Devices.MediaDevice.GetVideoCaptureSelector());

                        frameSourceGroups = frameSourceGroups.Where(group => group.SourceInfos.Any(sourceInfo => (sourceInfo.SourceKind == MediaFrameSourceKind.Color)
                                                                                                                 && (sourceInfo.MediaStreamType == MediaStreamType.VideoPreview
                                                                                                                     || sourceInfo.MediaStreamType == MediaStreamType.VideoRecord))
                                                                             && videoCaptureDevices.Any(x => x.Id == group.Id)).ToList();
                        // if there are no camera found
                        if(!frameSourceGroups.Any())
                        {
                            throw (new Exception("no valid camera found"));
                        }

                        foreach (var group in frameSourceGroups)
                        {
                            m_cameraIdList.Add(group.Id);
                            UICameraSelectionList.Items.Add($"{group.DisplayName} - ID: {group.Id}");
                        }

                        UITextOutput.Text = $"This app's PFN: {Package.Current.Id.FamilyName}";
                    }
                    catch (Exception ex)
                    {
                        UITextOutput.Text = $"error: {ex.Message}";
                    }
                }
                else
                {
                    UICameraSelectionList.Visibility = Visibility.Collapsed;
                }
            }

            base.OnNavigatedTo(e);
        }

        /// <summary>
        /// Initialize camera and UI
        /// </summary>
        /// <returns></returns>
        private async Task InitializeAsync()
        {
            m_lock.Wait();
            if(m_isInitialized)
            {
                m_lock.Release();
                return;
            }

            try
            {
                SelectedCameraTB.Text = m_cameraId;

                m_mediaCapture = new MediaCapture();
                m_mediaPlayer = new MediaPlayer();

                // We initialize the MediaCapture instance with the provided camera ID
                var initSettings = new MediaCaptureInitializationSettings()
                {
                    SharingMode = MediaCaptureSharingMode.ExclusiveControl,
                    VideoDeviceId = m_cameraId,
                    StreamingCaptureMode = StreamingCaptureMode.Video
                };

                await m_mediaCapture.InitializeAsync(initSettings);

                // Retrieve the source associated with the video preview stream.
                // On 1-pin camera, this may be the VideoRecord MediaStreamType as opposed to VideoPreview on multi-pin camera
                var frameSource = m_mediaCapture.FrameSources.FirstOrDefault(source => source.Value.Info.MediaStreamType == MediaStreamType.VideoPreview
                                                                                  && source.Value.Info.SourceKind == MediaFrameSourceKind.Color).Value;
                if (frameSource == null)
                {
                    frameSource = m_mediaCapture.FrameSources.FirstOrDefault(source => source.Value.Info.MediaStreamType == MediaStreamType.VideoRecord
                                                                                      && source.Value.Info.SourceKind == MediaFrameSourceKind.Color).Value;
                }

                // if no preview stream is available, bail
                if (frameSource == null)
                {
                    return;
                }

                // Setup MediaPlayer with the preview source
                m_mediaPlayer.RealTimePlayback = true;
                m_mediaPlayer.AutoPlay = true;
                m_mediaPlayer.Source = MediaSource.CreateFromMediaFrameSource(frameSource);
                UIMediaPlayerElement.SetMediaPlayer(m_mediaPlayer);

                // Creating a default contol manager and controls for Contrast, Brightness, and BackgroundSegmentation/Blur if the camera supports these.
                m_controlManager = new DefaultControlHelper.DefaultControlManager(m_cameraId);

                // Updating UI elements and default value controller for each supported control
                // Contrast
                if (m_mediaCapture.VideoDeviceController.Contrast.Capabilities.Supported)
                {
                    // retrieve default control manager to inspect and alter any default value
                    m_contrastController = m_controlManager.CreateController(
                        DefaultControlHelper.DefaultControllerType.VideoProcAmp,
                        (uint)CameraKsPropertyHelper.VidCapVideoProcAmpKind.KSPROPERTY_VIDEOPROCAMP_CONTRAST);

                    await UpdateContrastValueUI();
                }

                // Brightness
                if (m_mediaCapture.VideoDeviceController.Brightness.Capabilities.Supported)
                {
                    // retrieve default control manager to inspect and alter any default value
                    m_brightnessController = m_controlManager.CreateController(
                        DefaultControlHelper.DefaultControllerType.VideoProcAmp,
                        (uint)CameraKsPropertyHelper.VidCapVideoProcAmpKind.KSPROPERTY_VIDEOPROCAMP_BRIGHTNESS);

                    await UpdateBrightnessValueUI();
                }

                // EVComp
                if (m_mediaCapture.VideoDeviceController.ExposureCompensationControl.Supported)
                {
                    // retrieve default control manager to inspect and alter any default value
                    m_evCompController = m_controlManager.CreateController(
                        DefaultControlHelper.DefaultControllerType.ExtendedCameraControl,
                        (uint)CameraKsPropertyHelper.ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_EVCOMPENSATION);

                    await UpdateEVCompValueUI();
                }

                // Blur
                bool isBlurControlSupported = false;
                IExtendedPropertyHeader getPayload = null;

                getPayload = PropertyInquiry.GetExtendedControl(m_mediaCapture.VideoDeviceController, ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION);
                isBlurControlSupported = (getPayload != null);
                if (isBlurControlSupported && (((ulong)BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR & ~getPayload.Capability) == 0))
                {
                    // retrieve default control manager to inspect and alter any default value
                    m_backgroundBlurController = m_controlManager.CreateController(
                        DefaultControlHelper.DefaultControllerType.ExtendedCameraControl,
                        (uint)ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION);

                    await UpdateBackgroundSegmentationValueUI();
                }

                //hdr
                bool isHdrControlSupported = m_mediaCapture.VideoDeviceController.HdrVideoControl.Supported;
                if (isHdrControlSupported) 
                {
                    // retrieve default control manager to inspect and alter any default value
                    m_hdrController = m_controlManager.CreateController(
                        DefaultControlHelper.DefaultControllerType.ExtendedCameraControl,
                        (uint)ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_VIDEOHDR);

                    await UpdateHdrValueUI();
                }

                // create a CameraControlMonitor to be alerted when a control value is changed from outside our app
                // while we are using it (i.e. via the Windows Settings)
                List<ControlData> controlDataToMonitor = new List<ControlData>
                {
                    new ControlData()
                    {
                        controlKind = ControlKind.All,
                        controlId = 0
                    }
                };
                m_controlMonitorManager = ControlMonitorManager.CreateCameraControlMonitor(
                m_mediaCapture.MediaCaptureSettings.VideoDeviceId,
                controlDataToMonitor.ToArray());
                m_controlMonitorManager.CameraControlChanged += CameraControlMonitor_ControlChanged;

                UILaunchSettingsButton.Visibility = Visibility.Visible;

                m_isInitialized = true;
            }
            catch (Exception ex)
            {
                UITextOutput.Text = $"error: {ex.Message}";
            }
            finally
            {
                m_lock.Release();
            }
        }

        /// <summary>
        /// callback for when a camera control value is changed. This is used to get notified if user changes 
        /// a control value outside of this app that we want to take action on such as refreshing our UI.
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="control"></param>
        /// <exception cref="Exception"></exception>
        private void CameraControlMonitor_ControlChanged(object sender, ControlData control)
        {
            Debug.WriteLine($"controlKind={control.controlKind} : controlId={(uint)control.controlId} changed externally");
            if (control.controlKind == ControlKind.VidCapVideoProcAmpKind)
            {
                switch (control.controlId)
                {
                    case (uint)VidCapVideoProcAmpKind.KSPROPERTY_VIDEOPROCAMP_BRIGHTNESS:
                        var t = UpdateBrightnessValueUI();
                        break;

                    case (uint)VidCapVideoProcAmpKind.KSPROPERTY_VIDEOPROCAMP_CONTRAST:
                        var t2 = UpdateContrastValueUI();
                        break;

                    default:
                        throw new Exception("unhandled control change, implement or allow through at your convenience");
                }
            }
            else if (control.controlKind == ControlKind.ExtendedControlKind)
            {
                switch (control.controlId)
                {
                    case (uint)ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_EVCOMPENSATION:
                        var t = UpdateEVCompValueUI();
                        break;

                    case (uint)ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION:
                        var t2 = UpdateBackgroundSegmentationValueUI();
                        break;
                    //Stop the video preview first to receive this control change.
                    //https://learn.microsoft.com/en-us/windows-hardware/drivers/stream/ksproperty-cameracontrol-extended-videohdr
                    case (uint)ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_VIDEOHDR:
                        var t3 = UpdateHdrValueUI();
                        break;
                    default:
                        throw new Exception("unhandled ExtendedControlKind change, implement or allow through at your convenience");
                }
            }
        }


        /// <summary>
        /// Update background segmentation UI given default value stored.
        /// </summary>
        /// <returns></returns>
        private async Task UpdateBackgroundSegmentationValueUI()
        {
            await Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
            {
                try
                {
                    if (m_backgroundBlurController != null)
                    {
                        DefaultBlurToggle.Toggled -= DefaultBlurToggle_Toggled;
                        DefaultBlurToggle.Visibility = Visibility.Visible;
                        
                        // query current default value
                        var defaultValue = m_backgroundBlurController.TryGetStoredDefaultValue();
                        if(defaultValue != null)
                        {
                            DefaultBlurToggle.IsOn = (defaultValue != 0);
                        }

                        DefaultBlurToggle.Toggled += DefaultBlurToggle_Toggled;
                    }
                }
                catch (Exception ex)
                {
                    UITextOutput.Text = $"error: {ex.Message}";
                }
            });
        }

        /// <summary>
        /// Update Video Hdr UI given default value stored.
        /// </summary>
        /// <returns></returns>
        private async Task UpdateHdrValueUI()
        {
            await Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
            {
                try
                {
                    if (m_hdrController != null)
                    {
                        DefaultHdrToggle.Toggled -= DefaultHdrToggle_Toggled;
                        DefaultHdrToggle.Visibility = Visibility.Visible;

                        // query current default value
                        var defaultValue = m_hdrController.TryGetStoredDefaultValue();
                        if (defaultValue != null)
                        {
                            DefaultHdrToggle.IsOn = (defaultValue != 0);
                        }

                        DefaultHdrToggle.Toggled += DefaultHdrToggle_Toggled;
                    }
                }
                catch (Exception ex)
                {
                    UITextOutput.Text = $"error: {ex.Message}";
                }
            });
        }

        /// <summary>
        /// Update exposure compensation UI given default value stored.
        /// </summary>
        /// <returns></returns>
        private async Task UpdateEVCompValueUI()
        {
            await Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
            {
                try
                {
                    if (m_evCompController != null)
                    {
                        // Setup the UI slider with min and max values as well as reflect the current default value applied for this control
                        DefaultEVCompSlider.ValueChanged -= DefaultEVCompSlider_ValueChanged;
                        DefaultEVCompSlider.Minimum = m_mediaCapture.VideoDeviceController.ExposureCompensationControl.Min;
                        DefaultEVCompSlider.Maximum = m_mediaCapture.VideoDeviceController.ExposureCompensationControl.Max;
                        DefaultEVCompSlider.StepFrequency = m_mediaCapture.VideoDeviceController.ExposureCompensationControl.Step;
                        DefaultEVCompSlider.Visibility = Visibility.Visible;
                        var defaultValue = m_evCompController.TryGetStoredDefaultValue();
                        if (defaultValue != null)
                        {
                            DefaultEVCompSlider.Value = (double)defaultValue;
                        }
                        else
                        {
                            DefaultEVCompSlider.Value = m_mediaCapture.VideoDeviceController.ExposureCompensationControl.Value;
                        }
                        DefaultEVCompSlider.ValueChanged += DefaultEVCompSlider_ValueChanged;
                    }
                }
                catch (Exception ex)
                {
                    UITextOutput.Text = $"error: {ex.Message}";
                }
            });
        }


        /// <summary>
        /// Update brigthness UI given default value stored.
        /// </summary>
        /// <returns></returns>
        private async Task UpdateBrightnessValueUI()
        {
            await Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
            {
                try
                {
                    if (m_brightnessController != null)
                    {
                        // Setup the UI slider with min and max values as well as reflect the current default value applied for this control
                        DefaultBrightnessSlider.ValueChanged -= DefaultBrightnessSlider_ValueChanged;
                        DefaultBrightnessSlider.Minimum = m_mediaCapture.VideoDeviceController.Brightness.Capabilities.Min;
                        DefaultBrightnessSlider.Maximum = m_mediaCapture.VideoDeviceController.Brightness.Capabilities.Max;
                        DefaultBrightnessSlider.StepFrequency = m_mediaCapture.VideoDeviceController.Brightness.Capabilities.Step;
                        DefaultBrightnessSlider.Visibility = Visibility.Visible;

                        var defaultValue = m_brightnessController.TryGetStoredDefaultValue();
                        if (defaultValue != null)
                        {
                            DefaultBrightnessSlider.Value = (double)defaultValue;
                        }
                        else
                        {
                            double value = 0;
                            if (m_mediaCapture.VideoDeviceController.Contrast.TryGetValue(out value))
                            {
                                DefaultBrightnessSlider.Value = value;
                            }
                            else
                            {
                                DefaultBrightnessSlider.IsEnabled = false;
                            }
                        }
                        DefaultBrightnessSlider.ValueChanged += DefaultBrightnessSlider_ValueChanged;
                    }
                }
                catch (Exception ex)
                {
                    UITextOutput.Text = $"error: {ex.Message}";
                }
            });
        }

        /// <summary>
        /// Update contrast UI given default value stored.
        /// </summary>
        /// <returns></returns>
        private async Task UpdateContrastValueUI()
        {
            await Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
            {
                try
                {
                    // Setup the UI slider with min and max values as well as reflect the current default value applied for this control
                    if (m_contrastController != null)
                    {
                        DefaultContrastSlider.ValueChanged -= DefaultContrastSlider_ValueChanged;
                        DefaultContrastSlider.Minimum = m_mediaCapture.VideoDeviceController.Contrast.Capabilities.Min;
                        DefaultContrastSlider.Maximum = m_mediaCapture.VideoDeviceController.Contrast.Capabilities.Max;
                        DefaultContrastSlider.StepFrequency = m_mediaCapture.VideoDeviceController.Contrast.Capabilities.Step;
                        DefaultContrastSlider.Visibility = Visibility.Visible;

                        var defaultValue = m_contrastController.TryGetStoredDefaultValue();
                        if (defaultValue != null)
                        {
                            DefaultContrastSlider.Value = (double)defaultValue;
                        }
                        else
                        {
                            double value = 0;
                            if (m_mediaCapture.VideoDeviceController.Contrast.TryGetValue(out value))
                            {
                                DefaultContrastSlider.Value = value;
                            }
                            else
                            {
                                DefaultContrastSlider.IsEnabled = false;
                            }
                        }
                        DefaultContrastSlider.ValueChanged += DefaultContrastSlider_ValueChanged;
                    }
                }
                catch (Exception ex)
                {
                    UITextOutput.Text = $"error: {ex.Message}";
                }
            });
        }


#region UICallbacks
        private void DefaultContrastSlider_ValueChanged(object sender, RangeBaseValueChangedEventArgs e)
        {
            try
            {
                m_contrastController.SetDefaultValue((int)e.NewValue);
                m_mediaCapture.VideoDeviceController.Contrast.TrySetValue(e.NewValue);
            }
            catch (Exception ex)
            {
                UITextOutput.Text = $"error: {ex.Message}";
            }
        }

        private void DefaultBrightnessSlider_ValueChanged(object sender, RangeBaseValueChangedEventArgs e)
        {
            try
            {
                m_brightnessController.SetDefaultValue((int)e.NewValue);
                m_mediaCapture.VideoDeviceController.Brightness.TrySetValue(e.NewValue);
            }
            catch (Exception ex)
            {
                UITextOutput.Text = $"error: {ex.Message}";
            }
        }

        private void DefaultBlurToggle_Toggled(object sender, RoutedEventArgs e)
        {
            try
            {
                int flags = (int)((DefaultBlurToggle.IsOn == true) ? BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR : BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_OFF);
                m_backgroundBlurController.SetDefaultValue(flags);
                PropertyInquiry.SetExtendedControlFlags(m_mediaCapture.VideoDeviceController, ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION, (uint)flags);
            }
            catch (Exception ex)
            {
                UITextOutput.Text = $"error: {ex.Message}";
            }
        }

        private void DefaultHdrToggle_Toggled(object sender, RoutedEventArgs e)
        {
            try
            {
                Windows.Media.Devices.HdrVideoMode flags = (DefaultHdrToggle.IsOn) ? Windows.Media.Devices.HdrVideoMode.On : Windows.Media.Devices.HdrVideoMode.Off;
                m_hdrController.SetDefaultValue((int)flags);
                m_mediaCapture.VideoDeviceController.HdrVideoControl.Mode = flags;
            }
            catch (Exception ex)
            {
                UITextOutput.Text = $"error: {ex.Message}";
            }
        }

        private async void DefaultEVCompSlider_ValueChanged(object sender, RangeBaseValueChangedEventArgs e)
        {
            try
            {
                // For KSPROPERTY_CAMERACONTROL_EXTENDED_EVCOMPENSATION the floating point values need to be scaled
                // to matching int range with the step size.
                m_evCompController.SetDefaultValue((int) Math.Round(e.NewValue / DefaultEVCompSlider.StepFrequency));
                await m_mediaCapture.VideoDeviceController.ExposureCompensationControl.SetValueAsync((float)e.NewValue);
            }
            catch (Exception ex)
            {
                UITextOutput.Text = $"error: {ex.Message}";
            }
        }

        private void UILaunchSettingsButton_Click(object sender, RoutedEventArgs e)
        {
            // fire-and-forget launch UI
            var t = Windows.System.Launcher.LaunchUriAsync(new Uri("ms-settings:camera?cameraId=" + Uri.EscapeDataString(m_cameraId)));
        }

        private void UICameraSelectionList_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            var selectedIndex = UICameraSelectionList.SelectedIndex;
            if (selectedIndex >= 0)
            {
                m_cameraId = m_cameraIdList[selectedIndex];
                UICameraSelectionList.Visibility = Visibility.Collapsed;

                var t = InitializeAsync(); // fire-forget
            }
        }

#endregion UICallbacks
    }
}
