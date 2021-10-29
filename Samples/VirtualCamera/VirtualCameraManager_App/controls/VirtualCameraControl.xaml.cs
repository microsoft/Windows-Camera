// Copyright (C) Microsoft Corporation. All rights reserved.

using System;
using System.Linq;
using System.Threading.Tasks;
using VirtualCameraManager_App.controls;
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
        private MediaFrameSource m_frameSource = null;
        private VideoDeviceControls m_videoDeviceControlsUI = null;
        private MediaSource m_mediaSource = null;

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
            m_frameSource = m_mediaCapture.FrameSources.FirstOrDefault(source => source.Value.Info.MediaStreamType == MediaStreamType.VideoPreview
                                                                              && source.Value.Info.SourceKind == MediaFrameSourceKind.Color).Value;
            if (m_frameSource == null)
            {
                m_frameSource = m_mediaCapture.FrameSources.FirstOrDefault(source => source.Value.Info.MediaStreamType == MediaStreamType.VideoRecord
                                                                                  && source.Value.Info.SourceKind == MediaFrameSourceKind.Color).Value;
            }

            // if no preview stream is available, bail
            if (m_frameSource == null)
            {
                UIPreviewToggle.IsEnabled = false;
                return;
            }

            // Setup MediaPlayer with the preview source
            m_mediaPlayer = new MediaPlayer();
            m_mediaPlayer.RealTimePlayback = true;
            m_mediaPlayer.AutoPlay = true;
            UIMediaPlayerElement.SetMediaPlayer(m_mediaPlayer);

            // Query support of custom and standard KSProperty and update UI toggle buttons accordingly
            if(m_videoDeviceControlsUI != null)
            {
                UIMainPanel.Children.Remove(m_videoDeviceControlsUI);
                m_videoDeviceControlsUI = null;
            }
            m_videoDeviceControlsUI = new VideoDeviceControls(m_mediaCapture.VideoDeviceController);
            UIMainPanel.Children.Add(m_videoDeviceControlsUI);
            m_videoDeviceControlsUI.Initialize();

            UIPreviewToggle.IsEnabled = true;
        }

        /// <summary>
        /// Cleanup the view of the virtual camera
        /// </summary>
        private void DisposeOfUIPreview()
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
        }

        #region UI interactions
        private async void UIEnable_Click(object sender, RoutedEventArgs e)
        {
            try
            {
                DisposeOfUIPreview();
                VirtualCameraProxyInst.EnableVirtualCamera();

                await InitializeAsync();

                UIPreviewToggle.IsOn = true;
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
                VirtualCameraProxyInst.DisableVirtualCamera();
                DisposeOfUIPreview();
                UIPreviewToggle.IsEnabled = false;
                UIPreviewToggle.IsOn = false;
            }
            catch (Exception ex)
            {
                VirtualCameraControlFailed?.Invoke(ex);
            }
        }

        internal void DisposeOfVCam()
        {
            try
            {
                VirtualCameraControlRemoved?.Invoke(VirtualCameraProxyInst.SymbolicLink);
                DisposeOfUIPreview();
                VirtualCameraProxyInst.RemoveVirtualCamera();
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
                VirtualCameraControlRemoved?.Invoke(VirtualCameraProxyInst.SymbolicLink);
                VirtualCameraProxyInst.RemoveVirtualCamera();
                DisposeOfUIPreview();
                
            }
            catch (Exception ex)
            {
                VirtualCameraControlFailed?.Invoke(ex);
            }
        }

        #endregion UI interactions

        private void UIPreviewToggle_Toggled(object sender, RoutedEventArgs e)
        {
            if(UIPreviewToggle.IsOn)
            {
                m_mediaSource = MediaSource.CreateFromMediaFrameSource(m_frameSource);
                m_mediaPlayer.Source = m_mediaSource;

                // Query support of custom and standard KSProperty and update UI toggle buttons accordingly
                m_videoDeviceControlsUI.Initialize();
            }
            else
            {
                if(m_mediaSource != null)
                {
                    m_mediaSource.Dispose();
                }
                if (m_mediaPlayer != null)
                {
                    m_mediaPlayer.Source = null;
                }
            }
        }
    }
}
