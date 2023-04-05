//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//

using CameraKsPropertyHelper;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Linq;
using System.Threading.Tasks;
using Windows.Devices.Enumeration;
using Windows.Media.Capture;
using Windows.Media.Capture.Frames;
using Windows.Media.Core;
using Windows.Media.Devices;
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
        private List<DeviceInformation> m_cameraDeviceList;
        string cameraId = null;
        private MediaCapture m_mediaCapture = null;
        private MediaPlayer m_mediaPlayer = null;

        private DefaultControlHelper.DefaultControlManager m_controlManager = null;
        private DefaultControlHelper.DefaultController m_contrastController = null;
        private DefaultControlHelper.DefaultController m_brightnessController = null;
        private DefaultControlHelper.DefaultController m_backgroundBlurController = null;
        private bool m_blurDefault = false;
        private DefaultControlHelper.DefaultController m_evCompController = null;
        private DefaultControlHelper.DefaultController m_autoFramingController = null;

        public MainPage()
        {
            InitializeComponent();
        }

        protected override async void OnNavigatedTo(NavigationEventArgs e)
        {
            // This app is expecting to be launched via Camera Settings page association
            // which will include the camera symbolic link name in the navigation arguments
            string launchArgs = e.Parameter?.ToString();
            if (launchArgs == null || launchArgs.Length == 0)
            {
                SelectedCameraTB.Text = "No launch args";

                var cameraList = await DeviceInformation.FindAllAsync(DeviceClass.VideoCapture);
                m_cameraDeviceList = cameraList.ToList();
                await Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
                {

                    // re-enable and refresh UI
                    UICameraList.ItemsSource = m_cameraDeviceList.Select(x => x.Name);
                    UICameraList.SelectedIndex = 0;
                    UICameraList.IsEnabled = true;
                    UICameraList.Visibility = Visibility.Visible;
                });

            }
            else
            {
                var argsArr = launchArgs.Split(' ');
                int argIndex = 0;

                foreach (var arg in argsArr)
                {
                    if (arg == "/cameraId")
                    {
                        // "/cameraId" argument found
                        cameraId = argsArr[argIndex + 1];
                        break;
                    }
                    argIndex++;
                }

                SelectedCameraTB.Text = cameraId;

                if (cameraId != null)
                {
                    await InitializeAsync();
                }
            }

            base.OnNavigatedTo(e);
        }
        private async Task InitializeAsync()
        {
            try
            {
                m_mediaCapture = new MediaCapture();
                m_mediaPlayer = new MediaPlayer();

                // We initialize the MediaCapture instance with the virtual camera in sharing mode
                // to preview its stream without blocking other app from using it
                var initSettings = new MediaCaptureInitializationSettings()
                {
                    SharingMode = MediaCaptureSharingMode.ExclusiveControl,
                    VideoDeviceId = cameraId,
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
                m_controlManager = new DefaultControlHelper.DefaultControlManager(cameraId);

                // Contrast
                if (m_mediaCapture.VideoDeviceController.Contrast.Capabilities.Supported)
                {
                    m_contrastController = m_controlManager.CreateController(DefaultControlHelper.DefaultControllerType.VideoProcAmp, (uint)CameraKsPropertyHelper.VidCapVideoProcAmpKind.KSPROPERTY_VIDEOPROCAMP_CONTRAST, 0 /*flags*/);
                }

                // Brightness
                if (m_mediaCapture.VideoDeviceController.Brightness.Capabilities.Supported)
                {
                    m_brightnessController = m_controlManager.CreateController(DefaultControlHelper.DefaultControllerType.VideoProcAmp, (uint)CameraKsPropertyHelper.VidCapVideoProcAmpKind.KSPROPERTY_VIDEOPROCAMP_BRIGHTNESS, 0 /*flags*/);
                }

                // EVComp
                if (m_mediaCapture.VideoDeviceController.ExposureCompensationControl.Supported)
                {
                    var evStep = m_mediaCapture.VideoDeviceController.ExposureCompensationControl.Step;
                    var flagtmp = (uint)((1 / evStep) + 0.5f);

                    uint flag = 0;

                    // See ksmedia.h for KSCAMERA_EXTENDEDPROP_EVCOMP_SIXTHSTEP and other step flags
                    switch (flagtmp)
                    {
                        case 6:
                            flag = 0x1;
                            break;
                        case 4:
                            flag = 0x2;
                            break;
                        case 3:
                            flag = 0x4;
                            break;
                        case 2:
                            flag = 0x8;
                            break;
                        case 1:
                            flag = 0x10;
                            break;
                        default:
                            flag = 0x0;
                            break;
                    }
                    m_evCompController = m_controlManager.CreateController(DefaultControlHelper.DefaultControllerType.ExtendedCameraControl, (uint)CameraKsPropertyHelper.ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_EVCOMPENSATION, flag);
                }

                // Blur
                bool isBlurControlSupported = false;
                IExtendedPropertyPayload getPayload = null;

                getPayload = PropertyInquiry.GetExtendedControl(m_mediaCapture.VideoDeviceController, ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION);
                isBlurControlSupported = (getPayload != null);
                if (isBlurControlSupported && (((ulong)BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR & ~getPayload.Capability) == 0))
                {
                    m_backgroundBlurController = m_controlManager.CreateController(DefaultControlHelper.DefaultControllerType.ExtendedCameraControl, (uint)ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION, 0 /*flags*/);
                    // if the flag value is set to BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR
                    m_blurDefault = (getPayload.Flags % 2 == 1);
                }

                // DW w/ auto framing
                bool isAutoFramingSupported = false;
                getPayload = PropertyInquiry.GetExtendedControl(m_mediaCapture.VideoDeviceController, ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_DIGITALWINDOW);
                if (getPayload != null)
                {
                    isAutoFramingSupported = ((getPayload.Capability & (ulong)(DigitalWindowCapabilityKind.KSCAMERA_EXTENDEDPROP_DIGITALWINDOW_AUTOFACEFRAMING)) == (ulong)DigitalWindowCapabilityKind.KSCAMERA_EXTENDEDPROP_DIGITALWINDOW_AUTOFACEFRAMING);
                }
                if (isAutoFramingSupported)
                {
                    m_autoFramingController = m_controlManager.CreateController(DefaultControlHelper.DefaultControllerType.ExtendedCameraControl, (uint)ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_DIGITALWINDOW, 0 /*flags*/);
                }

                // Updating UI elements
                await Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, (Windows.UI.Core.DispatchedHandler)(() =>
                {
                    try
                    {
                        // The sliders is initialized to work with the values that driver provides as supported range and step
                        if (m_contrastController != null)
                        {
                            DefaultContrastSlider.ValueChanged -= DefaultContrastSlider_ValueChanged;
                            DefaultContrastSlider.Minimum = m_mediaCapture.VideoDeviceController.Contrast.Capabilities.Min;
                            DefaultContrastSlider.Maximum = m_mediaCapture.VideoDeviceController.Contrast.Capabilities.Max;
                            DefaultContrastSlider.StepFrequency = m_mediaCapture.VideoDeviceController.Contrast.Capabilities.Step;
                            DefaultContrastSlider.Visibility = Visibility.Visible;
                            if (m_contrastController.HasDefaultValueStored())
                            {
                                DefaultContrastSlider.Value = m_contrastController.DefaultValue;
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

                        if (m_brightnessController != null)
                        {
                            DefaultBrightnessSlider.ValueChanged -= DefaultBrightnessSlider_ValueChanged;
                            DefaultBrightnessSlider.Minimum = m_mediaCapture.VideoDeviceController.Brightness.Capabilities.Min;
                            DefaultBrightnessSlider.Maximum = m_mediaCapture.VideoDeviceController.Brightness.Capabilities.Max;
                            DefaultBrightnessSlider.StepFrequency = m_mediaCapture.VideoDeviceController.Brightness.Capabilities.Step;
                            DefaultBrightnessSlider.Visibility = Visibility.Visible;
                            if (m_brightnessController.HasDefaultValueStored())
                            {
                                DefaultBrightnessSlider.Value = m_brightnessController.DefaultValue;
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
                        if (m_evCompController != null)
                        {
                            DefaultEVCompSlider.ValueChanged -= DefaultEVCompSlider_ValueChanged;
                            DefaultEVCompSlider.Minimum = m_mediaCapture.VideoDeviceController.ExposureCompensationControl.Min;
                            DefaultEVCompSlider.Maximum = m_mediaCapture.VideoDeviceController.ExposureCompensationControl.Max;
                            DefaultEVCompSlider.StepFrequency = m_mediaCapture.VideoDeviceController.ExposureCompensationControl.Step;
                            DefaultEVCompSlider.Visibility = Visibility.Visible;
                            if (m_evCompController.HasDefaultValueStored())
                            {
                                DefaultEVCompSlider.Value = m_evCompController.DefaultValue;
                            }
                            else
                            {
                                DefaultEVCompSlider.Value = m_mediaCapture.VideoDeviceController.ExposureCompensationControl.Value;
                            }
                            DefaultEVCompSlider.ValueChanged += DefaultEVCompSlider_ValueChanged;
                        }

                        if (m_backgroundBlurController != null)
                        {
                            DefaultBlurToggle.Toggled -= DefaultBlurToggle_Toggled;
                            DefaultBlurToggle.Visibility = Visibility.Visible;
                            if (m_backgroundBlurController.HasDefaultValueStored())
                            {
                                DefaultBlurToggle.IsOn = (m_backgroundBlurController.DefaultValue != 0);
                            }
                            else
                            {
                                DefaultBlurToggle.IsOn = m_blurDefault;
                            }

                            DefaultBlurToggle.Toggled += DefaultBlurToggle_Toggled;
                        }

                        if (m_autoFramingController != null)
                        {
                            AutoFramingToggle.Toggled -= AutoFramingToggle_Toggled;
                            AutoFramingToggle.Visibility = Visibility.Visible;
                            if (m_autoFramingController.HasDefaultValueStored())
                            {
                                AutoFramingToggle.IsOn = (m_autoFramingController.DefaultValue != 0);
                            }
                            else
                            {
                                var res = (getPayload.Flags & (ulong)(DigitalWindowCapabilityKind.KSCAMERA_EXTENDEDPROP_DIGITALWINDOW_AUTOFACEFRAMING));
                                AutoFramingToggle.IsOn = (res != 0);
                            }
                            AutoFramingToggle.Toggled += AutoFramingToggle_Toggled;

                        }

                    }
                    catch (Exception ex)
                    {
                        UITextOutput.Text = $"error: {ex.Message}";
                    }
                }));
            }
            catch (Exception ex)
            {
                UITextOutput.Text = $"error: {ex.Message}";
            }
        }

        private void UninitMediaCapture()
        {
            if (m_controlManager != null)
            {
                m_controlManager = null;
            }
            if (m_mediaPlayer != null)
            {
                UIMediaPlayerElement.SetMediaPlayer(null);
                m_mediaPlayer.Pause();
                m_mediaPlayer.Source = null;
                m_mediaPlayer.Dispose();
                m_mediaPlayer = null;
            }

            if (m_mediaCapture != null)
            {
                m_mediaCapture.Dispose();
                m_mediaCapture = null;
            }
        }

        private void DefaultContrastSlider_ValueChanged(object sender, RangeBaseValueChangedEventArgs e)
        {
            try
            {
                m_contrastController.DefaultValue = (int)e.NewValue;
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
                m_brightnessController.DefaultValue = (int)e.NewValue;
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
                m_backgroundBlurController.DefaultValue = flags;
                PropertyInquiry.SetExtendedControlFlags(m_mediaCapture.VideoDeviceController, ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION, (uint)flags);
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
                m_evCompController.DefaultValue = (int)Math.Round(e.NewValue / DefaultEVCompSlider.StepFrequency);
                await m_mediaCapture.VideoDeviceController.ExposureCompensationControl.SetValueAsync((float)e.NewValue);
            }
            catch (Exception ex)
            {
                UITextOutput.Text = $"error: {ex.Message}";
            }
        }

        private void AutoFramingToggle_Toggled(object sender, RoutedEventArgs e)
        {
            try
            {
                int flags = (int)((AutoFramingToggle.IsOn == true) ? DigitalWindowCapabilityKind.KSCAMERA_EXTENDEDPROP_DIGITALWINDOW_AUTOFACEFRAMING : DigitalWindowCapabilityKind.KSCAMERA_EXTENDEDPROP_DIGITALWINDOW_MANUAL);
                m_autoFramingController.DefaultValue = flags;
                PropertyInquiry.SetExtendedControlFlags(m_mediaCapture.VideoDeviceController, ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_DIGITALWINDOW, (uint)flags);
            }
            catch (Exception ex)
            {
                UITextOutput.Text = $"error: {ex.Message}";
            }
        }

        private async void UICameraList_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            UninitMediaCapture();

            cameraId = m_cameraDeviceList[UICameraList.SelectedIndex].Id;
            await InitializeAsync();
        }
    }
}
