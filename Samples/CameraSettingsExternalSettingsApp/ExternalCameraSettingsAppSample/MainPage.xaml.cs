//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//

using CameraKsPropertyHelper;
using System;
using System.ComponentModel;
using System.Linq;
using System.Threading.Tasks;
using Windows.Media.Capture;
using Windows.Media.Capture.Frames;
using Windows.Media.Core;
using Windows.Media.Playback;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Navigation;

// The Blank Page item template is documented at https://go.microsoft.com/fwlink/?LinkId=402352&clcid=0x409

namespace OutboundSettingsAppTest
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class MainPage : Page
    {
        string cameraId = null;
        private MediaCapture m_mediaCapture = null;
        private MediaPlayer m_mediaPlayer = null;

        private DefaultControlHelper.DefaultControlManager m_controlManager = null;
        private DefaultControlHelper.DefaultController m_contrastController = null;
        private DefaultControlHelper.DefaultController m_brightnessController = null;
        private DefaultControlHelper.DefaultController m_backgroundBlurController = null;

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

            m_controlManager = new DefaultControlHelper.DefaultControlManager(cameraId);
            if (m_mediaCapture.VideoDeviceController.Contrast.Capabilities.Supported)
            {
                m_contrastController = m_controlManager.CreateController(DefaultControlHelper.DefaultControllerType.VideoProcAmp, (uint) CameraKsPropertyHelper.VidCapVideoProcAmpKind.KSPROPERTY_VIDEOPROCAMP_CONTRAST);
            }

            if (m_mediaCapture.VideoDeviceController.Brightness.Capabilities.Supported)
            {
                m_brightnessController = m_controlManager.CreateController(DefaultControlHelper.DefaultControllerType.VideoProcAmp,(uint) CameraKsPropertyHelper.VidCapVideoProcAmpKind.KSPROPERTY_VIDEOPROCAMP_BRIGHTNESS);
            }

            bool isDeviceControlSupported = false;
            IExtendedPropertyPayload getPayload = null;

            getPayload = PropertyInquiry.GetExtendedControl(m_mediaCapture.VideoDeviceController, ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION);
            isDeviceControlSupported = (getPayload != null);
            if (isDeviceControlSupported && (((ulong)BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR & ~getPayload.Capability) == 0))
            {
                m_backgroundBlurController = m_controlManager.CreateController(DefaultControlHelper.DefaultControllerType.ExtendedCameraControl, (uint) ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION);
            }

            await Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
            {
                // The sliders is initialized to work with the values that driver provides as supported range and step
                if (m_contrastController != null)
                {
                    DefaultContrastSlider.Minimum = m_mediaCapture.VideoDeviceController.Contrast.Capabilities.Min;
                    DefaultContrastSlider.Maximum = m_mediaCapture.VideoDeviceController.Contrast.Capabilities.Max;
                    DefaultContrastSlider.StepFrequency = m_mediaCapture.VideoDeviceController.Contrast.Capabilities.Step;
                    DefaultContrastSlider.Visibility = Visibility.Visible;
                    DefaultContrastSlider.Value = m_contrastController.DefaultValue;
                }

                if (m_brightnessController != null)
                {
                    DefaultBrightnessSlider.Minimum = m_mediaCapture.VideoDeviceController.Brightness.Capabilities.Min;
                    DefaultBrightnessSlider.Maximum = m_mediaCapture.VideoDeviceController.Brightness.Capabilities.Max;
                    DefaultBrightnessSlider.StepFrequency = m_mediaCapture.VideoDeviceController.Brightness.Capabilities.Step;
                    DefaultBrightnessSlider.Visibility = Visibility.Visible;
                    DefaultBrightnessSlider.Value = m_brightnessController.DefaultValue;
                }

                if (m_backgroundBlurController != null)
                {
                    DefaultBlurToggle.Visibility = Visibility.Visible;
                    DefaultBlurToggle.IsOn = m_backgroundBlurController.DefaultValue != 0;
                }
            });
        }

        private void DefaultContrastSlider_ValueChanged(object sender, RangeBaseValueChangedEventArgs e)
        {
            m_contrastController.DefaultValue = (uint)e.NewValue;
            m_mediaCapture.VideoDeviceController.Contrast.TrySetValue(e.NewValue);
        }

        private void DefaultBrightnessSlider_ValueChanged(object sender, RangeBaseValueChangedEventArgs e)
        {
            m_brightnessController.DefaultValue = (uint)e.NewValue;
            m_mediaCapture.VideoDeviceController.Brightness.TrySetValue(e.NewValue);
        }

        private void DefaultBlurToggle_Toggled(object sender, RoutedEventArgs e)
        {
            uint flags = (DefaultBlurToggle.IsOn == true) ? (uint)BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR : (uint)BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_OFF;
            m_backgroundBlurController.DefaultValue = flags;
            PropertyInquiry.SetExtendedControlFlags(m_mediaCapture.VideoDeviceController, ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION, flags);
        }
    }
}
