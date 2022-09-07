//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Windows.Devices.Enumeration;
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
        private MediaCapture m_mediaCapture = null;
        private MediaPlayer m_mediaPlayer = null;
        private List<DeviceInformation> m_cameraDeviceList;

        private ControlMonitorHelper.ControlMonitorManager m_controlManager = null;
        public MainPage()
        {
            this.InitializeComponent();
        }

        protected override async void OnNavigatedTo(NavigationEventArgs e)
        {
            //await InitializeAsync();
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
            base.OnNavigatedTo(e);
        }

        protected override void OnNavigatedFrom(NavigationEventArgs e)
        {
            UninitMediaCapture();
            base.OnNavigatedFrom(e);
        }

        private async Task InitializeAsync(int selectedIndex)
        {
            m_mediaCapture = new MediaCapture();
            m_mediaPlayer = new MediaPlayer();

            // We initialize the MediaCapture instance with the camera in sharing mode
            // to preview its stream without blocking other app from using it
            var initSettings = new MediaCaptureInitializationSettings()
            {
                // This app could define "SharingMode= MediaCaptureSharingMode.SharedReadOnly" here if
                // the app only wants to be informed of the changes and not have full control camera app.
                VideoDeviceId = m_cameraDeviceList[selectedIndex].Id,
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

            // Creating a controls data for listening changes on these controls.
            var controlContrast = new ControlMonitorHelper.ControlData()
            {
                controlKind = ControlMonitorHelper.ControlKind.VidCapVideoProcAmpKind,
                controlId = (UInt32)CameraKsPropertyHelper.VidCapVideoProcAmpKind.KSPROPERTY_VIDEOPROCAMP_CONTRAST
            };

            var controlBrightness = new ControlMonitorHelper.ControlData()
            {
                controlKind = ControlMonitorHelper.ControlKind.VidCapVideoProcAmpKind,
                controlId = (UInt32)CameraKsPropertyHelper.VidCapVideoProcAmpKind.KSPROPERTY_VIDEOPROCAMP_BRIGHTNESS
            };

            ControlMonitorHelper.ControlData[] controls = { controlContrast, controlBrightness };

            m_controlManager = ControlMonitorHelper.ControlMonitorManager.CreateCameraControlMonitor(m_mediaCapture.MediaCaptureSettings.VideoDeviceId, controls);
            m_controlManager.CameraControlChanged += CameraControlMonitor_VidCapCameraControlChanged;
            await Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
            {
                // The sliders is initialized to work with the values that driver provides as supported range and step
                ContrastSlider.Minimum = m_mediaCapture.VideoDeviceController.Contrast.Capabilities.Min;
                ContrastSlider.Maximum = m_mediaCapture.VideoDeviceController.Contrast.Capabilities.Max;
                ContrastSlider.StepFrequency = m_mediaCapture.VideoDeviceController.Contrast.Capabilities.Step;
                ContrastSlider.Visibility = Visibility.Visible;
                double value = 0;
                m_mediaCapture.VideoDeviceController.Contrast.TryGetValue(out value);
                ContrastSlider.Value = value;

                BrightnessSlider.Minimum = m_mediaCapture.VideoDeviceController.Brightness.Capabilities.Min;
                BrightnessSlider.Maximum = m_mediaCapture.VideoDeviceController.Brightness.Capabilities.Max;
                BrightnessSlider.StepFrequency = m_mediaCapture.VideoDeviceController.Brightness.Capabilities.Step;
                BrightnessSlider.Visibility = Visibility.Visible;
                m_mediaCapture.VideoDeviceController.Brightness.TryGetValue(out value);
                BrightnessSlider.Value = value;
            });
        }

        private void UninitMediaCapture()
        {
            if(m_controlManager != null)
            {
                m_controlManager.CameraControlChanged -= CameraControlMonitor_VidCapCameraControlChanged;
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
        private async void CameraControlMonitor_VidCapCameraControlChanged(object sender, ControlMonitorHelper.ControlData control)
        {
            try
            {

                if (control.controlKind == ControlMonitorHelper.ControlKind.VidCapVideoProcAmpKind && control.controlId == (UInt32)CameraKsPropertyHelper.VidCapVideoProcAmpKind.KSPROPERTY_VIDEOPROCAMP_CONTRAST)
                {
                    await Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
                    {
                        double value = 0;
                        m_mediaCapture.VideoDeviceController.Contrast.TryGetValue(out value);
                        ContrastSlider.Value = value;

                    });
                }
                else if(control.controlKind == ControlMonitorHelper.ControlKind.VidCapVideoProcAmpKind && control.controlId == (UInt32)CameraKsPropertyHelper.VidCapVideoProcAmpKind.KSPROPERTY_VIDEOPROCAMP_BRIGHTNESS)
                {
                    await Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
                    {
                        double value = 0;
                        m_mediaCapture.VideoDeviceController.Brightness.TryGetValue(out value);
                        BrightnessSlider.Value = value;

                    });
                }
            }
            catch (Exception ex)
            {
                Console.WriteLine($"CameraControlMonitor_VidCapControlChanged: {ex.Message} | hr={ex.HResult}");
            }
        }

        private void ContrastSlider_ValueChanged(object sender, RangeBaseValueChangedEventArgs e)
        {
            m_mediaCapture.VideoDeviceController.Contrast.TrySetValue(e.NewValue);
        }

        private void BrightnessSlider_ValueChanged(object sender, RangeBaseValueChangedEventArgs e)
        {
            m_mediaCapture.VideoDeviceController.Brightness.TrySetValue(e.NewValue);
        }

        private async void UICameraList_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            UninitMediaCapture();
            await InitializeAsync(UICameraList.SelectedIndex);
        }
    }
}
