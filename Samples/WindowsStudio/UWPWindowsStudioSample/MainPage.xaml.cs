// Copyright (c) Microsoft. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using Windows.Media.Capture.Frames;
using Windows.Media.Capture;
using Windows.Media.Playback;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Media;
using System.Threading.Tasks;
using Windows.Devices.Enumeration;
using Windows.UI.Core;
using Windows.Media.Devices;
using Windows.Media.MediaProperties;
using Windows.Media.Core;
using System.Diagnostics;
using ControlMonitorHelper;

namespace UWPWindowsStudioSample
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class MainPage : Page
    {
        // Camera related
        private MediaCapture m_mediaCapture;
        private MediaPlayer m_mediaPlayer;
        private MediaFrameSource m_selectedMediaFrameSource;
        private MediaFrameFormat m_selectedFormat;
        private ControlMonitorHelper.ControlMonitorManager m_controlManager = null;

        // this DevPropKey signifies if the system is capable of exposing a Windows Studio camera
        private const string DEVPKEY_DeviceInterface_IsWindowsCameraEffectAvailable = "{6EDC630D-C2E3-43B7-B2D1-20525A1AF120} 4";

        readonly Dictionary<string, ulong> m_possibleBackgroundSegmentationFlagValues = new Dictionary<string, ulong>()
        {
            {
                "off",
                (ulong)KsHelper.BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_OFF
            },
            {
                "blur",
                (ulong)KsHelper.BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR
            },
            {
                "mask metadata",
                (ulong)KsHelper.BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_MASK
            },
            {
                "shallow focus blur",
                (ulong)KsHelper.BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR
                    + (ulong)KsHelper.BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_SHALLOWFOCUS
            },
            {
                "blur and mask metadata",
                (ulong)KsHelper.BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR
                    + (ulong)KsHelper.BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_MASK
            },
            {
                "shallow focus blur and mask metadata",
                (ulong)KsHelper.BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR
                    + (ulong)KsHelper.BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_MASK
                    + (ulong)KsHelper.BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_SHALLOWFOCUS
            }
        };

        readonly Dictionary<string, ulong> m_possibleEyeGazeCorrectionFlagValues = new Dictionary<string, ulong>()
        {
            {
                "off",
                (ulong)KsHelper.EyeGazeCorrectionCapabilityKind.KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_OFF
            },
            {
                "on",
                (ulong)KsHelper.EyeGazeCorrectionCapabilityKind.KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_ON
            },
            {
                "enhanced",
                (ulong)KsHelper.EyeGazeCorrectionCapabilityKind.KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_ON
                    + (ulong)KsHelper.EyeGazeCorrectionCapabilityKind.KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_STARE
            }
        };

        public MainPage()
        {
            this.InitializeComponent();
        }

        private async void Page_Loaded(object sender, RoutedEventArgs e)
        {
            try
            {
                // Attempt to find a Windows Studio camera then initialize a MediaCapture instance
                // for it and start streaming
                await InitializeWindowsStudioMediaCaptureAsync();

                // Refresh UI element associated with Windows Studio effects
                RefreshEyeGazeUI();
                RefreshBackgroundSegmentationUI();
                RefreshAutomaticFramingUI();
            }
            catch (Exception ex)
            {
                NotifyUser(ex.Message, NotifyType.ErrorMessage);
            }
        }

        /// <summary>
        /// Initialize a MediaCapture instance for a Windows Studio camera if available on the system
        /// </summary>
        /// <returns></returns>
        private async Task InitializeWindowsStudioMediaCaptureAsync()
        {
            // Retrieve the list of cameras available on the system with the additional properties
            // that specifies if the system is compatible with Windows Studio
            DeviceInformationCollection deviceInfoCollection = await DeviceInformation.FindAllAsync(
                MediaDevice.GetVideoCaptureSelector(),
                new List<string>() { DEVPKEY_DeviceInterface_IsWindowsCameraEffectAvailable });

            // if there are no camera devices returned, it means we don't support Windows Studio on this system
            if (deviceInfoCollection.Count == 0)
            {
                throw new InvalidOperationException("Windows Studio not supported");
            }

            // Windows Studio is currently only supported on front facing cameras.
            // Look at the panel information to identify which camera is front facing
            DeviceInformation selectedDeviceInfo = deviceInfoCollection.FirstOrDefault(x => x.EnclosureLocation.Panel == Windows.Devices.Enumeration.Panel.Front);
            if (selectedDeviceInfo == null)
            {
                throw new InvalidOperationException("Could not find a front facing camera");
            }

            // Initialize MediaCapture with the Windows Studio camera device id
            m_mediaCapture = new MediaCapture();
            await m_mediaCapture.InitializeAsync(
                new MediaCaptureInitializationSettings()
                {
                    VideoDeviceId = selectedDeviceInfo.Id,
                    MemoryPreference = MediaCaptureMemoryPreference.Cpu,
                    StreamingCaptureMode = StreamingCaptureMode.Video,
                    SharingMode = MediaCaptureSharingMode.ExclusiveControl
                });

            // find the frame source, i.e. the stream or pin we want to use to preview from the Windows Studio camera
            m_selectedMediaFrameSource = m_mediaCapture.FrameSources.FirstOrDefault(source => source.Value.Info.MediaStreamType == MediaStreamType.VideoPreview
                                                                                  && source.Value.Info.SourceKind == MediaFrameSourceKind.Color).Value;
            if (m_selectedMediaFrameSource == null)
            {
                m_selectedMediaFrameSource = m_mediaCapture.FrameSources.FirstOrDefault(source => source.Value.Info.MediaStreamType == MediaStreamType.VideoRecord
                                                                                      && source.Value.Info.SourceKind == MediaFrameSourceKind.Color).Value;
            }

            // if no color preview stream is available, bail
            if (m_selectedMediaFrameSource == null)
            {
                throw new Exception("Could not find a valid color preview frame source");
            }

            // Filter MediaType given resolution and framerate preference, and filter out non-compatible subtypes
            // in this case we are looking for a MediaType closest to 1080p @ above 15fps
            const uint targetFrameRate = 15;
            const uint targetWidth = 1920;
            const uint targetHeight = 1080;
            bool IsCompatibleSubtype(string subtype)
            {
                return string.Compare(subtype, MediaEncodingSubtypes.Nv12, true) == 0
                       || string.Compare(subtype, MediaEncodingSubtypes.Bgra8, true) == 0
                       || string.Compare(subtype, MediaEncodingSubtypes.Yuy2, true) == 0
                       || string.Compare(subtype, MediaEncodingSubtypes.Rgb32, true) == 0;
            }

            var formats = m_selectedMediaFrameSource.SupportedFormats.Where(
                            format => (format.FrameRate.Numerator / format.FrameRate.Denominator) > targetFrameRate
                                      && IsCompatibleSubtype(format.Subtype))?.OrderBy(
                                          format => Math.Abs((int)(format.VideoFormat.Width * format.VideoFormat.Height) - (targetWidth * targetHeight)));

            m_selectedFormat = formats.FirstOrDefault();

            if (m_selectedFormat == null)
            {
                throw new Exception($"Could not find a valid frame source format to stream with");
            }
            await m_selectedMediaFrameSource.SetFormatAsync(m_selectedFormat);

            // Connect the camera to the UI element for previewing the camera
            m_mediaPlayer = new MediaPlayer();
            m_mediaPlayer.RealTimePlayback = true;
            m_mediaPlayer.AutoPlay = true;
            var mediaSource = MediaSource.CreateFromMediaFrameSource(m_selectedMediaFrameSource);
            m_mediaPlayer.Source = mediaSource;
            m_mediaPlayer.CommandManager.IsEnabled = false; // disable playback controls
            UIPreviewElement.SetMediaPlayer(m_mediaPlayer);

            NotifyUser($"streaming with {m_selectedMediaFrameSource.Info.DeviceInformation.Name} at " +
                $"{m_selectedMediaFrameSource.CurrentFormat.VideoFormat.Width}x{m_selectedMediaFrameSource.CurrentFormat.VideoFormat.Height}" +
                $"@{m_selectedMediaFrameSource.CurrentFormat.FrameRate.Numerator}/{m_selectedMediaFrameSource.CurrentFormat.FrameRate.Denominator}" +
                $"with subtype = {m_selectedMediaFrameSource.CurrentFormat.Subtype}",
                NotifyType.StatusMessage);

            // create a CameraControlMonitor to be alerted when a control value is changed from outside our app
            // while we are using it (i.e. via the Windows Settings)
            m_controlManager = ControlMonitorHelper.ControlMonitorManager.CreateCameraControlMonitor(
                m_mediaCapture.MediaCaptureSettings.VideoDeviceId,
                new ControlMonitorHelper.ControlData[]
                {
                    new ControlMonitorHelper.ControlData
                    {
                        controlKind = ControlMonitorHelper.ControlKind.ExtendedControlKind,
                        controlId = (uint)KsHelper.ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION
                    },
                    new ControlMonitorHelper.ControlData
                    {
                        controlKind = ControlMonitorHelper.ControlKind.ExtendedControlKind,
                        controlId = (uint)KsHelper.ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION
                    },
                    new ControlMonitorHelper.ControlData
                    {
                        controlKind = ControlMonitorHelper.ControlKind.ExtendedControlKind,
                        controlId = (uint)KsHelper.ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_DIGITALWINDOW
                    },

                });
            m_controlManager.CameraControlChanged += CameraControlMonitor_ControlChanged;
        }


        /// <summary>
        /// Callback when a monitored camera control changes so that we can refresh the UI accordingly
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="control"></param>
        /// <exception cref="Exception"></exception>
        private async void CameraControlMonitor_ControlChanged(object sender, ControlData control)
        {
            Debug.WriteLine($"{(KsHelper.ExtendedControlKind)((uint)control.controlId)} changed externally");
            if(control.controlKind == ControlKind.ExtendedControlKind)
            {
                switch(control.controlId)
                {
                    case (uint)KsHelper.ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION:
                        await Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () => RefreshEyeGazeUI());
                        break;

                    case (uint)KsHelper.ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION:
                        await Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () => RefreshBackgroundSegmentationUI());
                        break;

                    case (uint)KsHelper.ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_DIGITALWINDOW:
                        await Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () => RefreshAutomaticFramingUI());
                        break;

                    default:
                        throw new Exception("unhandled control change, implement or allow through at your convenience");
                }
            }
        }

        /// <summary>
        /// get the current value for eye gaze correction and update UI accordingly
        /// </summary>
        private void RefreshEyeGazeUI()
        {
            // send a GET call for the eye gaze DDI and retrieve the result payload
            byte[] byteResultPayload = KsHelper.GetExtendedControlPayload(
                m_mediaCapture.VideoDeviceController,
                KsHelper.ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION);

            // reinterpret the byte array as an extended property payload
            KsHelper.KsBasicCameraExtendedPropPayload payload = KsHelper.FromBytes<KsHelper.KsBasicCameraExtendedPropPayload>(byteResultPayload);

            // refresh the list of values displayed for this control
            UIEyeGazeCorrectionModes.SelectionChanged -= UIEyeGazeCorrectionModes_SelectionChanged;

            var eyeGazeCorrectionCapabilities = m_possibleEyeGazeCorrectionFlagValues.Where(x => (x.Value & payload.header.Capability) == x.Value).ToList();
            UIEyeGazeCorrectionModes.ItemsSource = eyeGazeCorrectionCapabilities.Select(x => x.Key);

            // reflect in UI what is the current value
            var currentFlag = payload.header.Flags;
            var currentIndexToSelect = eyeGazeCorrectionCapabilities.FindIndex(x => x.Value == currentFlag);
            UIEyeGazeCorrectionModes.SelectedIndex = currentIndexToSelect;

            UIEyeGazeCorrectionModes.IsEnabled = true;
            UIEyeGazeCorrectionModes.SelectionChanged += UIEyeGazeCorrectionModes_SelectionChanged;
        }

        /// <summary>
        /// get the current value for background segmentation and update UI accordingly
        /// </summary>
        private void RefreshBackgroundSegmentationUI()
        {
            // send a GET call for the eye gaze DDI and retrieve the result payload
            byte[] byteResultPayload = KsHelper.GetExtendedControlPayload(
                m_mediaCapture.VideoDeviceController,
                KsHelper.ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION);

            // reinterpret the byte array as an extended property payload
            KsHelper.KsBackgroundCameraExtendedPropPayload payload = KsHelper.FromBytes(byteResultPayload);

            // refresh the list of values displayed for this control
            UIBackgroundSegmentationModes.SelectionChanged -= UIBackgroundSegmentationModes_SelectionChanged;

            var backgroundSegmentationCapabilities = m_possibleBackgroundSegmentationFlagValues.Where(x => (x.Value & payload.header.Capability) == x.Value).ToList();
            UIBackgroundSegmentationModes.ItemsSource = backgroundSegmentationCapabilities.Select(x => x.Key);

            // reflect in UI what is the current value
            var currentFlag = payload.header.Flags;
            var currentIndexToSelect = backgroundSegmentationCapabilities.FindIndex(x => x.Value == currentFlag);
            UIBackgroundSegmentationModes.SelectedIndex = currentIndexToSelect;

            UIBackgroundSegmentationModes.IsEnabled = true;
            UIBackgroundSegmentationModes.SelectionChanged += UIBackgroundSegmentationModes_SelectionChanged;
        }

        /// <summary>
        /// get the current state of Automatic Framing and update UI accordingly
        /// </summary>
        private void RefreshAutomaticFramingUI()
        {
            // check if automatic framing is supported, in theory it should always be supported by Windows Studio
            if (m_mediaCapture.VideoDeviceController.DigitalWindowControl.SupportedModes.Contains(DigitalWindowMode.Auto))
            {
                // refresh the UI for this control
                UIAutomaticFramingSwitch.Toggled -= UIAutomaticFramingSwitch_Toggled;
                UIAutomaticFramingSwitch.IsOn = (m_mediaCapture.VideoDeviceController.DigitalWindowControl.CurrentMode == DigitalWindowMode.Auto);
                UIAutomaticFramingSwitch.IsEnabled = true;
                UIAutomaticFramingSwitch.Toggled += UIAutomaticFramingSwitch_Toggled;
            }
        }

        private void UIEyeGazeCorrectionModes_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            var selectedIndex = UIEyeGazeCorrectionModes.SelectedIndex;
            if (selectedIndex < 0)
            {
                return;
            }

            // find the flags value associated with the current mode selected
            string selection = UIEyeGazeCorrectionModes.SelectedItem.ToString();
            ulong flagToSet = m_possibleEyeGazeCorrectionFlagValues.FirstOrDefault(x => x.Key == selection).Value;

            // set the flags value for the corresponding extended control
            KsHelper.SetExtendedControlFlags(
                m_mediaCapture.VideoDeviceController,
                KsHelper.ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION,
                flagToSet);
        }

        private void UIBackgroundSegmentationModes_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            var selectedIndex = UIBackgroundSegmentationModes.SelectedIndex;
            if (selectedIndex < 0)
            {
                return;
            }

            // find the flags value associated with the current mode selected
            string selection = UIBackgroundSegmentationModes.SelectedItem.ToString();
            ulong flagToSet = m_possibleBackgroundSegmentationFlagValues.FirstOrDefault(x => x.Key == selection).Value;

            // set the flags value for the corresponding extended control
            KsHelper.SetExtendedControlFlags(
                m_mediaCapture.VideoDeviceController,
                KsHelper.ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION,
                flagToSet);
        }

        private void UIAutomaticFramingSwitch_Toggled(object sender, RoutedEventArgs e)
        {
            // set automatic framing On or Off given following the toggle switch state
            DigitalWindowMode modeToSet = UIAutomaticFramingSwitch.IsOn ? DigitalWindowMode.Auto : DigitalWindowMode.Off;
            m_mediaCapture.VideoDeviceController.DigitalWindowControl.Configure(modeToSet);
        }

        #region DebugMessageHelpers
        public enum NotifyType
        {
            StatusMessage,
            ErrorMessage
        };

        /// <summary>
        /// Display a message to the user.
        /// This method may be called from any thread.
        /// </summary>
        /// <param name="strMessage"></param>
        /// <param name="type"></param>
        public void NotifyUser(string strMessage, NotifyType type)
        {
            // If called from the UI thread, then update immediately.
            // Otherwise, schedule a task on the UI thread to perform the update.
            if (Dispatcher.HasThreadAccess)
            {
                UpdateStatus(strMessage, type);
            }
            else
            {
                var task = Dispatcher.RunAsync(CoreDispatcherPriority.Normal, () => UpdateStatus(strMessage, type));
                task.AsTask().Wait();
            }
        }

        /// <summary>
        /// Update the status message displayed on the UI
        /// </summary>
        /// <param name="strMessage"></param>
        /// <param name="type"></param>
        private void UpdateStatus(string strMessage, NotifyType type)
        {
            switch (type)
            {
                case NotifyType.StatusMessage:
                    UIStatusBorder.Background = new SolidColorBrush(Windows.UI.Colors.Green);
                    break;
                case NotifyType.ErrorMessage:
                    UIStatusBorder.Background = new SolidColorBrush(Windows.UI.Colors.Red);
                    break;
            }
            UIStatusBlock.Text = strMessage;
            Debug.WriteLine($"{type}: {strMessage}");

            // Collapse the StatusBlock if it has no text to conserve UI real estate.
            UIStatusBorder.Visibility = (!string.IsNullOrEmpty(UIStatusBlock.Text)) ? Visibility.Visible : Visibility.Collapsed;
        }
        #endregion DebugMessageHelpers
    }
}
