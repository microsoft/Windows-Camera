// Copyright (C) Microsoft Corporation. All rights reserved.

using System;
using System.Linq;
using System.Threading.Tasks;
using VirtualCameraManager_WinRT;
using Windows.Media.Capture;
using Windows.Media.Capture.Frames;
using Windows.Media.Core;
using Windows.Media.Playback;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;

namespace VirtualCameraManager_App
{
    /// <summary>
    /// UI control to display and interact with a virtual camera
    /// </summary>
    public sealed partial class VirtualCameraControl : UserControl
    {
        // events and delegates
        public delegate void VirtualCameraControlFailedHandler(Exception ex);
        public event VirtualCameraControlFailedHandler VirtualCameraControlFailed;
        public delegate void VirtualCameraControlRemovedHandler(string symLink);
        public event VirtualCameraControlRemovedHandler VirtualCameraControlRemoved;

        // members
        public VirtualCameraProxy VirtualCameraProxyInst { get; private set; }
        private MediaCapture m_mediaCapture = null;
        private MediaPlayer m_mediaPlayer = null;
        private int m_colorMode = 0;
        private bool m_eyeGazeMode = false;

        /// <summary>
        /// Constructor for VirtualCameraControl
        /// </summary>
        /// <param name="virtualCameraProxy"></param>
        public VirtualCameraControl(VirtualCameraProxy virtualCameraProxy)
        {
            VirtualCameraProxyInst = virtualCameraProxy;

            // UI initialization
            this.InitializeComponent();
            PopulateVirtualCamInfo();
        }

        /// <summary>
        /// Fill the UI with the information retrieved from the virtual camera this control is initialized with
        /// </summary>
        private void PopulateVirtualCamInfo()
        {
            UIVCamInfo.IsEnabled = VirtualCameraProxyInst.IsKnownInstance;
            UIEditButtons.IsEnabled = VirtualCameraProxyInst.IsKnownInstance;

            UIFriendlyName.Text = VirtualCameraProxyInst.IsKnownInstance ? VirtualCameraProxyInst.FriendlyName : "Unknown to this app";
            UIAccess.Text = VirtualCameraProxyInst.IsKnownInstance ? VirtualCameraProxyInst.Access.ToString() : "?";
            UILifetime.Text = VirtualCameraProxyInst.IsKnownInstance ? VirtualCameraProxyInst.Lifetime.ToString() : "?";
            UISymLinkName.Text = VirtualCameraProxyInst.SymbolicLink;
        }

        /// <summary>
        /// Initialize the view of the virtual camera and configure available interactions
        /// </summary>
        /// <returns></returns>
        public async Task InitializeAsync()
        {
            m_mediaCapture = new MediaCapture();
            m_mediaPlayer = new MediaPlayer();

            // We initialize the MediaCapture instance with the virtual camera in sharing mode
            // to preview its stream without blocking other app from using it
            var initSettings = new MediaCaptureInitializationSettings()
            {
                SharingMode = MediaCaptureSharingMode.SharedReadOnly,
                VideoDeviceId = VirtualCameraProxyInst.SymbolicLink,
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

            // Query support of custom and standard KSProperty and update UI toggle buttons accordingly
            bool isColorModeSupported = (null != CameraKsPropertyInquiry.GetCustomControl(CustomControlKind.ColorMode, m_mediaCapture.VideoDeviceController));
            bool isEyeGazeCorrectionSupported = (null != CameraKsPropertyInquiry.GetExtendedControl(ExtendedControlKind.EyeGazeCorrection, m_mediaCapture.VideoDeviceController));

            UIColorModeControlButton.IsEnabled = isColorModeSupported;
            UIEyeGazeControlButton.IsEnabled = isEyeGazeCorrectionSupported;
        }

        /// <summary>
        /// Cleanup the view of the virtual camera
        /// </summary>
        private void DisposeOfUIPreview()
        {
            if (m_mediaCapture != null)
            {
                m_mediaCapture.Dispose();
                m_mediaCapture = null;
            }

            if (m_mediaPlayer != null)
            {
                m_mediaPlayer.Source = null;
                m_mediaPlayer = null;
            }
        }

        #region UI interactions
        private async void UIEnable_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                DisposeOfUIPreview();

                VirtualCameraProxyInst.EnableVirtualCamera();

                await InitializeAsync();
            }
            catch (Exception ex)
            {
                VirtualCameraControlFailed?.Invoke(ex);
            }
        }

        private void UIDisable_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                DisposeOfUIPreview();

                VirtualCameraProxyInst.DisableVirtualCamera();
            }
            catch (Exception ex)
            {
                VirtualCameraControlFailed?.Invoke(ex);
            }
        }

        private void UIRemove_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                DisposeOfUIPreview();

                VirtualCameraProxyInst.RemoveVirtualCamera();

                VirtualCameraControlRemoved?.Invoke(VirtualCameraProxyInst.SymbolicLink);
            }
            catch (Exception ex)
            {
                VirtualCameraControlFailed?.Invoke(ex);
            }
        }

        private void UIColorModeControlButton_Click(object sender, RoutedEventArgs e)
        {
            var modes = Enum.GetValues(typeof(ColorModeKind));
            m_colorMode = (m_colorMode + 1) % modes.Length;
            CameraKsPropertyInquiry.SetCustomControlFlags(CustomControlKind.ColorMode, m_mediaCapture.VideoDeviceController, (uint)((int)modes.GetValue(m_colorMode)));
        }

        private void UIEyeGazeControlButton_Click(object sender, RoutedEventArgs e)
        {
            m_eyeGazeMode = !m_eyeGazeMode;
            CameraKsPropertyInquiry.SetExtendedControlFlags(ExtendedControlKind.EyeGazeCorrection, m_mediaCapture.VideoDeviceController, (uint)(m_eyeGazeMode == true ? 1 : 0));
        }

        #endregion UI interactions
    }
}
