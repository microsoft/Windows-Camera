// Copyright (c) Microsoft. All rights reserved.

using CameraKsPropertyHelper;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Windows.Devices.Enumeration;
using Windows.Foundation;
using Windows.Graphics.Imaging;
using Windows.Media.Capture;
using Windows.Media.Capture.Frames;
using Windows.Media.Core;
using Windows.Media.Effects;
using Windows.Media.MediaProperties;
using Windows.Media.Playback;
using Windows.UI.Core;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Media.Imaging;


namespace EyeGazeAndBackgroundSegmentation
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
        private MediaFrameReader m_frameReader;
        private MediaFrameFormat m_selectedFormat;

        // Synchronisation
        private SemaphoreSlim m_frameAquisitionLock = new SemaphoreSlim(1);
        private SemaphoreSlim m_displayLock = new SemaphoreSlim(1);
        private bool m_isCleaningUp = false;
        private bool m_isProcessingFrames = false;

        // For Background segmentation mask display
        private bool m_isBackgroundSegmentationMaskModeOn = false;
        private SoftwareBitmapSource m_backgroundMaskImageSource = new SoftwareBitmapSource();
        private bool m_isShowingBackgroundMask;
        private BoundingBoxRenderer m_bboxRenderer = null;

        /// <summary>
        /// Constructor
        /// </summary>
        public MainPage()
        {
            this.InitializeComponent();
        }


        /// <summary>
        /// Called after initialization of the Mainpage instance
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private async void Page_Loaded(object sender, RoutedEventArgs e)
        {
            m_bboxRenderer = new BoundingBoxRenderer(UICanvasOverlay, 2, 6);
            UIShowBackgroundImage_Checked(null, null);
            try
            {
                // Initialized MediaCapture and list available cameras
                await InitializeMediaCaptureAsync();
            }
            catch (Exception ex)
            {
                NotifyUser(ex.Message, NotifyType.ErrorMessage);
            }
        }

        /// <summary>
        /// Cleanup
        /// </summary>
        private async Task CleanupCameraAsync()
        {
            Debug.WriteLine("CleanupCamera");
            if(m_mediaPlayer != null)
            {
                m_mediaPlayer.Dispose();
                m_mediaPlayer = null;
            }

            // disengage frame reader
            if (m_frameReader != null)
            {
                await m_frameReader.StopAsync();
                m_frameReader.FrameArrived -= FrameReader_FrameArrived;
                m_frameReader = null;
            }

            if (m_mediaCapture != null)
            {
                m_mediaCapture.Dispose();
                m_mediaCapture = null;
            }
        }

        /// <summary>
        /// Initialize MediaCapture
        /// </summary>
        /// <returns></returns>
        private async Task InitializeMediaCaptureAsync()
        {
            DeviceInformationCollection deviceInfoCollection = await DeviceInformation.FindAllAsync(DeviceClass.VideoCapture);

            // Find the sources and de-duplicate from source groups not containing VideoCapture devices
            var allGroups = await MediaFrameSourceGroup.FindAllAsync();
            var colorVideoGroupList = allGroups.Where(group => 
                                                      group.SourceInfos.Any(sourceInfo => sourceInfo.SourceKind == MediaFrameSourceKind.Color
                                                                                                       && (sourceInfo.MediaStreamType == MediaStreamType.VideoPreview
                                                                                                           || sourceInfo.MediaStreamType == MediaStreamType.VideoRecord)));
            var validColorVideoGroupList = colorVideoGroupList.Where(group => 
                                                                     group.SourceInfos.All(sourceInfo =>
                                                                                           deviceInfoCollection.Any((deviceInfo) =>
                                                                                                                     sourceInfo.DeviceInformation == null || deviceInfo.Id == sourceInfo.DeviceInformation.Id)));

            if(validColorVideoGroupList.Count() > 0)
            {
                m_mediaFrameSourceGroupList = validColorVideoGroupList.ToList();
            }
            else
            {
                // No camera sources found
                NotifyUser("No suitable camera found", NotifyType.ErrorMessage);
                return;
            }

            //m_mediaFrameSourceGroupList = allGroups.Where(group => group.SourceInfos.Any(sourceInfo => sourceInfo.SourceKind == MediaFrameSourceKind.Color
            //                                                                                           && (sourceInfo.MediaStreamType == MediaStreamType.VideoPreview
            //                                                                                               || sourceInfo.MediaStreamType == MediaStreamType.VideoRecord))
            //                                                       && group.SourceInfos.All(sourceInfo => deviceInfoCollection.Any((deviceInfo) => deviceInfo.Id == sourceInfo.DeviceInformation.Id))).ToList();

            //if (m_mediaFrameSourceGroupList.Count == 0)
            //{
            //    // No camera sources found
            //    NotifyUser("No suitable camera found", NotifyType.ErrorMessage);
            //    return;
            //}

            var cameraNamesList = m_mediaFrameSourceGroupList.Select(group => group.DisplayName);

            await Dispatcher.RunAsync(CoreDispatcherPriority.Normal, () =>
            {
                UICmbCamera.ItemsSource = cameraNamesList;
                UICmbCamera.SelectedIndex = 0;
            });
        }

        /// <summary>
        /// Fetch a video preview stream on the selected media source and start camera preview
        /// </summary>
        private async Task<bool> StartPreviewAsync()
        {
            m_selectedMediaFrameSource = m_mediaCapture.FrameSources.FirstOrDefault(source => source.Value.Info.MediaStreamType == MediaStreamType.VideoPreview
                                                                                  && source.Value.Info.SourceKind == MediaFrameSourceKind.Color).Value;
            if (m_selectedMediaFrameSource == null)
            {
                m_selectedMediaFrameSource = m_mediaCapture.FrameSources.FirstOrDefault(source => source.Value.Info.MediaStreamType == MediaStreamType.VideoRecord
                                                                                      && source.Value.Info.SourceKind == MediaFrameSourceKind.Color).Value;
            }

            // if no preview stream is available, bail
            if (m_selectedMediaFrameSource == null)
            {
                NotifyUser($"Could not find a valid frame source", NotifyType.ErrorMessage);
                return false;
            }

            // Filter MediaType given resolution and framerate preference, and filter out non-compatible subtypes
            // in this case we are looking for a MediaType closest to 720p @ above 15fps
            var formats = m_selectedMediaFrameSource.SupportedFormats.Where(format =>
            format.FrameRate.Numerator / format.FrameRate.Denominator > 15
            && (string.Compare(format.Subtype, MediaEncodingSubtypes.Nv12, true) == 0
            || string.Compare(format.Subtype, MediaEncodingSubtypes.Bgra8, true) == 0
            || string.Compare(format.Subtype, MediaEncodingSubtypes.Yuy2, true) == 0
            || string.Compare(format.Subtype, MediaEncodingSubtypes.Rgb32, true) == 0)
            )?.OrderBy(format => Math.Abs((int)(format.VideoFormat.Width * format.VideoFormat.Height) - (1280 * 720)));
            m_selectedFormat = formats.FirstOrDefault();

            if (m_selectedFormat != null)
            {
                await m_selectedMediaFrameSource.SetFormatAsync(m_selectedFormat);
            }
            else
            {
                NotifyUser($"Could not find a valid frame source format", NotifyType.ErrorMessage);
                return false;
            }

            // Connect the camera to the UI element
            m_mediaPlayer = new MediaPlayer();
            m_mediaPlayer.RealTimePlayback = true;
            m_mediaPlayer.AutoPlay = true;
            var mediaSource = MediaSource.CreateFromMediaFrameSource(m_selectedMediaFrameSource);
            m_mediaPlayer.Source = mediaSource;
            m_mediaPlayer.CommandManager.IsEnabled = false;
            UIPreviewElement.SetMediaPlayer(m_mediaPlayer);
            UITxtBlockPreviewProperties.Text = string.Format("{0}x{1}@{2}, {3}",
                        m_selectedMediaFrameSource.CurrentFormat.VideoFormat.Width,
                        m_selectedMediaFrameSource.CurrentFormat.VideoFormat.Height,
                        m_selectedMediaFrameSource.CurrentFormat.FrameRate.Numerator + "/" + m_selectedMediaFrameSource.CurrentFormat.FrameRate.Denominator,
                        m_selectedMediaFrameSource.CurrentFormat.Subtype);

            // mirror preview if not streaming from the back
            if (m_selectedMediaFrameSource.Info.DeviceInformation?.EnclosureLocation == null ||
                m_selectedMediaFrameSource.Info.DeviceInformation?.EnclosureLocation.Panel != Windows.Devices.Enumeration.Panel.Back)
            {
                var effect = new VideoTransformEffectDefinition();
                effect.Mirror = MediaMirroringOptions.Horizontal;
                m_mediaPlayer.AddVideoEffect(effect.ActivatableClassId, false, effect.Properties);
            }

            return true;
        }

        /// <summary>
        /// Callback for whenever a frame is generated by FrameReader
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="args"></param>
        private async void FrameReader_FrameArrived(MediaFrameReader sender, MediaFrameArrivedEventArgs args)
        {
            try
            {
                // retrieve latest frame
                MediaFrameReference frame = sender.TryAcquireLatestFrame();

                if (frame != null)
                {
                    // if we are set to display the background segmentation mask metadata
                    if (m_isBackgroundSegmentationMaskModeOn && m_isShowingBackgroundMask)
                    {
                        Debug.WriteLine("Attempting to extract background mask");

                        BackgroundSegmentationMaskMetadata maskMetadata = null;

                        // extract the background mask metadata from the frame
                        var metadata = PropertyInquiry.ExtractFrameMetadata(frame, FrameMetadataKind.MetadataId_BackgroundSegmentationMask);
                        if (metadata != null)
                        {
                            maskMetadata = metadata as BackgroundSegmentationMaskMetadata;
                        }
                        else
                        {
                            NotifyUser("Could not extract proper background segmentation mask metadata", NotifyType.ErrorMessage);
                            return;
                        }

                        await Dispatcher.RunAsync(CoreDispatcherPriority.Normal, async () =>
                        {
                            // attempt to obtain lock to display mask metadata frame, if we cannot retrieve it (i.e. if we are not done displaying the previous one, skip displaying it
                            if (m_displayLock.Wait(0))
                            {
                                try
                                {
                                    var image = SoftwareBitmap.Convert(maskMetadata.MaskData, BitmapPixelFormat.Bgra8, BitmapAlphaMode.Ignore);
                                    await m_backgroundMaskImageSource.SetBitmapAsync(image);
                                    UIBackgroundSegmentationMaskImage.Source = null;
                                    UIBackgroundSegmentationMaskImage.Source = m_backgroundMaskImageSource;

                                    var foregroundBBInMaskRelativeCoordinates = ToRectInRelativeCoordinates(maskMetadata.ForegroundBoundingBox, (uint)maskMetadata.MaskResolution.Width, (uint)maskMetadata.MaskResolution.Height);
                                    var foregroundBBInFrameAbsoluteCoordinates = new BitmapBounds()
                                    {
                                        X = (uint)(maskMetadata.MaskCoverageBoundingBox.X + foregroundBBInMaskRelativeCoordinates.X * maskMetadata.MaskCoverageBoundingBox.Width),
                                        Y = (uint)(maskMetadata.MaskCoverageBoundingBox.Y + foregroundBBInMaskRelativeCoordinates.Y * maskMetadata.MaskCoverageBoundingBox.Height),
                                        Width = (uint)(maskMetadata.MaskCoverageBoundingBox.X + foregroundBBInMaskRelativeCoordinates.Width * maskMetadata.MaskCoverageBoundingBox.Width),
                                        Height = (uint)(maskMetadata.MaskCoverageBoundingBox.Y + foregroundBBInMaskRelativeCoordinates.Height * maskMetadata.MaskCoverageBoundingBox.Height)
                                    };

                                    m_bboxRenderer.Render(
                                        new List<Rect>
                                        {
                                            ToRectInRelativeCoordinates(maskMetadata.MaskCoverageBoundingBox, m_selectedFormat.VideoFormat.Width, m_selectedFormat.VideoFormat.Height),
                                            ToRectInRelativeCoordinates(foregroundBBInFrameAbsoluteCoordinates, m_selectedFormat.VideoFormat.Width, m_selectedFormat.VideoFormat.Height),
                                        },
                                        new List<string>
                                        {
                                            "MaskCoverageBoundingBox",
                                            "ForegroundBoundingBox"
                                        });
                                }
                                finally
                                {
                                    m_displayLock.Release();
                                }
                            }
                            else
                            {
                                Debug.WriteLine("Skip displaying the background segmentation mask metadata frame");
                            }
                        });
                    }
                }
            }
            catch (System.ObjectDisposedException ex)
            {
                Debug.WriteLine(ex.ToString());
            }
            catch (Exception ex)
            {
                NotifyUser(ex.Message, NotifyType.ErrorMessage);
                Debug.WriteLine(ex.ToString());
            }
        }

        private void UIPreviewElement_SizeChanged(object sender, SizeChangedEventArgs e)
        {
            if (m_selectedFormat == null) return;
            // Make sure the aspect ratio is honored when rendering the overlay
            float cameraAspectRatio = (float)m_selectedFormat.VideoFormat.Width / m_selectedFormat.VideoFormat.Height;
            float previewAspectRatio = (float)(UIPreviewElement.ActualWidth / UIPreviewElement.ActualHeight);
            UICanvasOverlay.Width = cameraAspectRatio >= previewAspectRatio ? UIPreviewElement.ActualWidth : UIPreviewElement.ActualHeight * cameraAspectRatio;
            UICanvasOverlay.Height = cameraAspectRatio >= previewAspectRatio ? UIPreviewElement.ActualWidth / cameraAspectRatio : UIPreviewElement.ActualHeight;

            m_bboxRenderer.ResizeContent(e);
        }

        /// <summary>
        /// Callback when the camera selection changes
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private async void UICmbCamera_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (m_mediaFrameSourceGroupList.Count == 0 || UICmbCamera.SelectedIndex < 0)
            {
                return;
            }

            m_frameAquisitionLock.Wait();
            {
                if (m_isCleaningUp != true)
                {
                    m_isCleaningUp = true;
                }

                // If we are processing frames, re-attempt the camera change later on
                if (m_isProcessingFrames)
                {
                    m_frameAquisitionLock.Release();
                    var fireAndForgetTask = Dispatcher.RunAsync(CoreDispatcherPriority.Normal, () =>
                    {
                        Thread.Sleep(10);
                        UICmbCamera_SelectionChanged(sender, e);
                    }); // fire and forget
                    return;
                }

                // if we are not procesing frames, clean up
                await CleanupCameraAsync();
            }
            m_frameAquisitionLock.Release();

            UIShowBackgroundImage.IsChecked = false;

            // cache camera selection
            m_selectedMediaFrameSourceGroup = m_mediaFrameSourceGroupList[UICmbCamera.SelectedIndex];

            // Create MediaCapture and its settings
            m_mediaCapture = new MediaCapture();
            var settings = new MediaCaptureInitializationSettings
            {
                SourceGroup = m_selectedMediaFrameSourceGroup,
                MemoryPreference = MediaCaptureMemoryPreference.Cpu,
                StreamingCaptureMode = StreamingCaptureMode.Video
            };

            // Initialize MediaCapture
            try
            {
                await m_mediaCapture.InitializeAsync(settings);

                if (await StartPreviewAsync())
                {
                    // Debug output to see if any of the controls are supported
                    string statusText = "control support:";
                    Dictionary<ExtendedControlKind, IExtendedPropertyPayload> controlSupport = new Dictionary<ExtendedControlKind, IExtendedPropertyPayload>();
                    foreach (ExtendedControlKind controlId in Enum.GetValues(typeof(ExtendedControlKind)))
                    {
                        controlSupport[controlId] = PropertyInquiry.GetExtendedControl(m_selectedMediaFrameSource.Controller.VideoDeviceController, controlId);
                        var controlQueryResult = controlSupport[controlId];
                        bool isSupported = controlQueryResult != null;
                        statusText += $"\n{controlId} : {isSupported}";
                        if (isSupported)
                        {
                            statusText += $"| Capability: {controlQueryResult.Capability} | Flags: {controlQueryResult.Flags} | Size: {controlQueryResult.Size}";
                            if (controlId == ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION && ((int)controlQueryResult.Capability & (int)BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_MASK) != 0)
                            {
                                BackgroundSegmentationPropertyPayload bsPayload = (controlQueryResult as BackgroundSegmentationPropertyPayload);
                                for (int i = 0; i < bsPayload.ConfigCaps.Count; i++)
                                {
                                    BackgroundSegmentationConfigCaps cap = bsPayload.ConfigCaps[i];
                                    string subTypeSupported = cap.SubType == Guid.Empty ? "Any stream subtype" : cap.SubType.ToString();
                                    statusText += $"\nMASK supported for stream capability that correlates with: subtype = {subTypeSupported}\n " +
                                        $"| res:{cap.Resolution.Width}x{cap.Resolution.Height} | mask res:{cap.MaskResolution.Width}x{cap.MaskResolution.Height} | mask res:{cap.MaxFrameRateNumerator}/{cap.MaxFrameRateDenominator}fps";
                                }
                            }
                        }
                    }

                    await Dispatcher.RunAsync(CoreDispatcherPriority.Normal, () =>
                    {
                        try
                        {
                            // display debug output
                            UIControlCapabilityText.Text = statusText;

                            // populate and re-enable parts of the UI
                            UIPreviewElement_SizeChanged(null, null);
                            UICmbCamera.IsEnabled = true;

                            // if background segmentation is supported, enable its toggle
                            UIBackgroundSegmentationPanel.IsEnabled = controlSupport[ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION] != null;
                            if (UIBackgroundSegmentationPanel.IsEnabled)
                            {
                                var possibleFlags = Enum.GetValues(typeof(BackgroundSegmentationCapabilityKind));
                                var capabilities = new List<BackgroundSegmentationCapabilityKind>();
                                foreach (BackgroundSegmentationCapabilityKind capability in possibleFlags)
                                {
                                    if (capability == BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_OFF ||
                                    ((int)controlSupport[ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION].Capability & (int)capability) != 0)
                                    {
                                        capabilities.Add(capability);
                                    }
                                }
                                UIBackgroundSegmentationModes.ItemsSource = capabilities;
                                UIBackgroundSegmentationModes.SelectedIndex = capabilities.IndexOf((BackgroundSegmentationCapabilityKind)controlSupport[ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION].Flags);
                            }

                            // if eye gaze correction is supported, enable its toggle
                            UIEyeGazeCorrectionPanel.IsEnabled = controlSupport[ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION] != null;
                            if (UIEyeGazeCorrectionPanel.IsEnabled)
                            {
                                UIEyeGazeCorrectionModes.ItemsSource = Enum.GetValues(typeof(EyeGazeCorrectionCapabilityKind));
                                UIEyeGazeCorrectionModes.SelectedIndex = (int)controlSupport[ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION].Flags;
                            }
                        }
                        catch(Exception ex)
                        {
                            NotifyUser(ex.Message, NotifyType.ErrorMessage);
                            Debug.WriteLine(ex.ToString());
                        }
                    });
                }
                else
                {
                    throw new Exception("Could not start preview");
                }
            }
            catch (Exception ex)
            {
                NotifyUser(ex.Message, NotifyType.ErrorMessage);
                Debug.WriteLine(ex.ToString());
            }
            finally
            {
                m_isCleaningUp = false;
            }
        }

        private async void UIShowBackgroundImage_Checked(object sender, RoutedEventArgs e)
        {
            await m_displayLock.WaitAsync();
            try
            {

                if (UIShowBackgroundImage.IsChecked == true)
                {
                    m_isShowingBackgroundMask = true;
                    UIBackgroundSegmentationMaskImage.Visibility = Visibility.Visible;
                    UICanvasOverlay.Visibility = Visibility.Visible;
                }
                else
                {
                    UIBackgroundSegmentationMaskImage.Visibility = Visibility.Collapsed;
                    m_isShowingBackgroundMask = false;
                    m_bboxRenderer.Reset();
                    UICanvasOverlay.Visibility = Visibility.Collapsed;
                }
            }
            finally
            {
                m_displayLock.Release();
            }
        }

        private void UIEyeGazeCorrectionModes_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (UIEyeGazeCorrectionModes.SelectedIndex >= 0)
            {
                PropertyInquiry.SetExtendedControlFlags(m_selectedMediaFrameSource.Controller.VideoDeviceController, ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION, (ulong)UIEyeGazeCorrectionModes.SelectedIndex);
            }
        }

        private async void UIBackgroundSegmentationModes_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (UIBackgroundSegmentationModes.SelectedIndex >= 0)
            {
                PropertyInquiry.SetExtendedControlFlags(m_selectedMediaFrameSource.Controller.VideoDeviceController, ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION, (ulong)UIBackgroundSegmentationModes.SelectedIndex);
                
                // cache whether or not we are asking for background mask metadata to be produced
                m_isBackgroundSegmentationMaskModeOn = (BackgroundSegmentationCapabilityKind)UIBackgroundSegmentationModes.SelectedItem == BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_MASK;
                UIShowBackgroundImage.IsEnabled = m_isBackgroundSegmentationMaskModeOn;

                if (m_isBackgroundSegmentationMaskModeOn)
                {
                    if (m_frameReader != null)
                    {
                        await m_frameReader.StopAsync();
                        m_frameReader.FrameArrived -= FrameReader_FrameArrived;
                        m_frameReader = null;
                    }
                    m_frameReader = await m_mediaCapture.CreateFrameReaderAsync(m_selectedMediaFrameSource);
                    m_frameReader.FrameArrived += FrameReader_FrameArrived;
                    await m_frameReader.StartAsync();
                }
                else
                {
                    UIShowBackgroundImage.IsChecked = false;
                }
            }
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

        private Rect ToRectInRelativeCoordinates(BitmapBounds bounds, uint imageWidth, uint imageHeight)
        {
            return new Rect(
                (double)bounds.X / imageWidth,
                (double)bounds.Y / imageHeight,
                (double)bounds.Width / imageWidth,
                (double)bounds.Height / imageHeight);
        }

        #endregion DebugMessageHelpers
    }
}
