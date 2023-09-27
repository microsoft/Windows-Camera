// Copyright (c) Microsoft. All rights reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.Media.Capture.Frames;
using Windows.Media.Capture;
using Windows.Media.Playback;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Data;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Navigation;
using System.Threading.Tasks;
using Windows.Devices.Enumeration;
using Windows.UI.Core;
using Windows.Media.Devices;
using Windows.Media.MediaProperties;
using Windows.Media.Core;
using System.Diagnostics;
using Windows.Graphics.Imaging;

// The Blank Page item template is documented at https://go.microsoft.com/fwlink/?LinkId=402352&clcid=0x409

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
        private List<MediaFrameSourceGroup> m_mediaFrameSourceGroupList;
        private MediaFrameSourceGroup m_selectedMediaFrameSourceGroup;
        private MediaFrameSource m_selectedMediaFrameSource;
        private MediaFrameFormat m_selectedFormat;
        private const string DEVPKEY_DeviceInterface_IsWindowsCameraEffectAvailable = "{6EDC630D-C2E3-43B7-B2D1-20525A1AF120} 4";

        public MainPage()
        {
            this.InitializeComponent();
        }

        private async void Page_Loaded(object sender, RoutedEventArgs e)
        {
            try
            {
                // Initialized MediaCapture and start streaming
                await InitializeMediaCaptureAsync();
                RefreshUI();
            }
            catch (Exception ex)
            {
                NotifyUser(ex.Message, NotifyType.ErrorMessage);
            }
        }

        private void RefreshUI()
        {
            var bytePayload = KsHelper.ToBytes(new KspNode(new KsProperty(
                KsHelper.KSPROPERTYSETID_ExtendedCameraControl,
                (uint)KsHelper.ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION,
                (uint)KsProperty.KsPropertyKind.KSPROPERTY_TYPE_GET), 15));

            string propString = $"{KsHelper.KSPROPERTYSETID_ExtendedCameraControl} {(uint)KsHelper.ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION}";
            var resultPayload = m_mediaCapture.VideoDeviceController.GetDevicePropertyById(
                propString, 
                null);
            if(resultPayload != null) 
            {
                if (resultPayload.Status == VideoDeviceControllerGetDevicePropertyStatus.Success)
                {
                    byte[] byteResultPayload = (byte[])resultPayload.Value;
                    KsBasicCameraExtendedPropPayload payload = KsHelper.FromBytes<KsBasicCameraExtendedPropPayload>(byteResultPayload);
                    var currentFlag = payload.header.Flags;
                    NotifyUser($"current flag value for KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION: {currentFlag}", NotifyType.StatusMessage);
                }
            }
        }

        /// <summary>
        /// Initialize MediaCapture
        /// </summary>
        /// <returns></returns>
        private async Task InitializeMediaCaptureAsync()
        {
            // Retrieve the list of cameras available on the system with the additional properties
            // that specifies if the system is compatible with Windows Studio
            DeviceInformationCollection deviceInfoCollection = await DeviceInformation.FindAllAsync(
                MediaDevice.GetVideoCaptureSelector(), 
                new List<string>() { DEVPKEY_DeviceInterface_IsWindowsCameraEffectAvailable } );

            // if there are no camera devices returned, it means we don't support Windows Studio on this system
            if(deviceInfoCollection.Count == 0)
            {
                throw new InvalidOperationException("Windows Studio not supported");
            }

            // Windows Studio is currently only supported on front facing cameras.
            // Look at the panel information to identify which camera to use
            DeviceInformation selectedDeviceInfo = deviceInfoCollection.FirstOrDefault(x => x.EnclosureLocation.Panel == Windows.Devices.Enumeration.Panel.Front);
            if(selectedDeviceInfo == null)
            {
                throw new InvalidOperationException("Could not find a front facing camera");
            }

            // Find the source associated with the device we identified earlier that streams in color.
            // (It may be redundant to check for SourceKind as Windows Studio only works on cameras exposing a color stream)
            var allGroups = await MediaFrameSourceGroup.FindAllAsync();

            m_selectedMediaFrameSourceGroup = allGroups.FirstOrDefault(group => group.SourceInfos.Any(
                sourceInfo => sourceInfo.DeviceInformation.Id == selectedDeviceInfo.Id
                              && sourceInfo.SourceKind == MediaFrameSourceKind.Color
                              && (sourceInfo.MediaStreamType == MediaStreamType.VideoPreview
                                  || sourceInfo.MediaStreamType == MediaStreamType.VideoRecord)));
            
            if(m_selectedMediaFrameSourceGroup == null) 
            {
                throw new Exception($"Unexpectedly could not find a MediaFrameSourceGroup associated with the device id {selectedDeviceInfo.Id}");
            }

            // Initialize MediaCapture
            m_mediaCapture = new MediaCapture();
            await m_mediaCapture.InitializeAsync(
                new MediaCaptureInitializationSettings()
                {
                    SourceGroup = m_selectedMediaFrameSourceGroup,
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
        }

        private void UIPreviewElement_SizeChanged(object sender, SizeChangedEventArgs e)
        {

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

        private void DebugOutput(string message,
            [System.Runtime.CompilerServices.CallerMemberName] string memberName = "")
        {
            Debug.WriteLine($"{memberName} : {message}");
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
