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
using Windows.Media.MediaProperties;
using Windows.Media.Playback;
using Windows.UI.Core;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Media;
using Windows.UI.Xaml.Media.Imaging;


namespace ExtendedControlAndMetadataSampleApp
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

        private readonly Dictionary<string, ulong> m_possibleBackgroundSegmentationFlagValues = new Dictionary<string, ulong>()
        {
            {
                "off",
                (ulong)BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_OFF
            },
            {
                "blur",
                (ulong)BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR
            },
            {
                "mask metadata",
                (ulong)BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_MASK
            },
            {
                "shallow focus blur",
                (ulong)BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR
                    + (ulong)BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_SHALLOWFOCUS
            },
            {
                "blur and mask metadata",
                (ulong)BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR
                    + (ulong)BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_MASK
            },
            {
                "shallow focus blur and mask metadata",
                (ulong)BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR
                    + (ulong)BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_MASK
                    + (ulong)BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_SHALLOWFOCUS
            }
        };

        private readonly Dictionary<string, ulong> m_possibleEyeGazeCorrectionFlagValues = new Dictionary<string, ulong>()
        {
            {
                "off",
                (ulong)EyeGazeCorrectionCapabilityKind.KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_OFF
            },
            {
                "on",
                (ulong)EyeGazeCorrectionCapabilityKind.KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_ON
            },
            {
                "enhanced",
                (ulong)EyeGazeCorrectionCapabilityKind.KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_ON
                    + (ulong)EyeGazeCorrectionCapabilityKind.KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_STARE
            }
        };

        private List<int> m_discreteFov2Stops = null;

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
            DebugOutput("CleanupCamera");

            // disengage frame reader
            if (m_frameReader != null)
            {
                await m_frameReader.StopAsync();
                m_frameReader.FrameArrived -= FrameReader_FrameArrived;
                m_frameReader = null;
            }

            if (m_mediaPlayer != null)
            {
                m_mediaPlayer.Dispose();
                m_mediaPlayer = null;
            }
            UIPreviewElement.SetMediaPlayer(null);

            if (m_mediaCapture != null)
            {
                m_mediaCapture.Dispose();
                m_mediaCapture = null;
            }

            m_isCleaningUp = false;
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

            if (validColorVideoGroupList.Count() > 0)
            {
                m_mediaFrameSourceGroupList = validColorVideoGroupList.ToList();
            }
            else
            {
                // No camera sources found
                NotifyUser("No suitable camera found", NotifyType.ErrorMessage);
                return;
            }

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

            // leave some time for the player to start the source
            await Task.Delay(1000);

            return true;
        }

        /// <summary>
        /// Callback for whenever a frame is generated by FrameReader
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="args"></param>
        private void FrameReader_FrameArrived(MediaFrameReader sender, MediaFrameArrivedEventArgs args)
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
                        DebugOutput("Attempting to extract background mask");

                        BackgroundSegmentationMaskMetadata maskMetadata = null;

                        // extract the background mask metadata from the frame
                        var metadata = PropertyInquiry.ExtractFrameMetadata(frame, FrameMetadataKind.MetadataId_BackgroundSegmentationMask);
                        if (metadata != null)
                        {
                            maskMetadata = metadata as BackgroundSegmentationMaskMetadata;
                            SoftwareBitmap image = SoftwareBitmap.Convert(maskMetadata.MaskData, BitmapPixelFormat.Bgra8, BitmapAlphaMode.Ignore);
                            var foregroundBBInMaskRelativeCoordinates = ToRectInRelativeCoordinates(maskMetadata.ForegroundBoundingBox, (uint)maskMetadata.MaskResolution.Width, (uint)maskMetadata.MaskResolution.Height);
                            var foregroundBBInFrameAbsoluteCoordinates = new BitmapBounds()
                            {
                                X = (uint)(maskMetadata.MaskCoverageBoundingBox.X + foregroundBBInMaskRelativeCoordinates.X * maskMetadata.MaskCoverageBoundingBox.Width),
                                Y = (uint)(maskMetadata.MaskCoverageBoundingBox.Y + foregroundBBInMaskRelativeCoordinates.Y * maskMetadata.MaskCoverageBoundingBox.Height),
                                Width = (uint)(maskMetadata.MaskCoverageBoundingBox.X + foregroundBBInMaskRelativeCoordinates.Width * maskMetadata.MaskCoverageBoundingBox.Width),
                                Height = (uint)(maskMetadata.MaskCoverageBoundingBox.Y + foregroundBBInMaskRelativeCoordinates.Height * maskMetadata.MaskCoverageBoundingBox.Height)
                            };

                            var t = Dispatcher.RunAsync(CoreDispatcherPriority.Normal, async () =>
                            {
                                // attempt to obtain lock to display mask metadata frame, if we cannot retrieve it (i.e. if we are not done displaying the previous one, skip displaying it
                                if (m_displayLock.Wait(0))
                                {
                                    try
                                    {
                                        await m_backgroundMaskImageSource.SetBitmapAsync(image);
                                        UIBackgroundSegmentationMaskImage.Source = null;
                                        UIBackgroundSegmentationMaskImage.Source = m_backgroundMaskImageSource;

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
                                    DebugOutput("Skip displaying the background segmentation mask metadata frame");
                                }
                            });
                        }
                        else
                        {
                            NotifyUser("Could not extract proper background segmentation mask metadata", NotifyType.ErrorMessage);
                            frame.Dispose();
                            return;
                        }
                    }
                    frame.Dispose();
                }
            }
            catch (System.ObjectDisposedException ex)
            {
                DebugOutput(ex.ToString());
            }
            catch (Exception ex)
            {
                NotifyUser(ex.Message, NotifyType.ErrorMessage);
                DebugOutput(ex.ToString());
            }
        }

        private void UIPreviewElement_SizeChanged(object sender, SizeChangedEventArgs e)
        {
            try
            {
                if (m_selectedFormat == null) return;
                // Make sure the aspect ratio is honored when rendering the overlay
                float cameraAspectRatio = (float)m_selectedFormat.VideoFormat.Width / m_selectedFormat.VideoFormat.Height;
                float previewAspectRatio = (float)(UIPreviewElement.ActualWidth / UIPreviewElement.ActualHeight);
                UICanvasOverlay.Width = cameraAspectRatio >= previewAspectRatio ? UIPreviewElement.ActualWidth : UIPreviewElement.ActualHeight * cameraAspectRatio;
                UICanvasOverlay.Height = cameraAspectRatio >= previewAspectRatio ? UIPreviewElement.ActualWidth / cameraAspectRatio : UIPreviewElement.ActualHeight;

                m_bboxRenderer.ResizeContent(e);
            }
            catch (Exception ex)
            {
                NotifyUser(ex.Message, NotifyType.ErrorMessage);
                DebugOutput(ex.ToString());
            }
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
            try
            {
                NotifyUser("Initializing..", NotifyType.StatusMessage);
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

                    // if we are not processing frames, clean up
                    await CleanupCameraAsync();
                }
            }
            catch (Exception ex)
            {
                NotifyUser(ex.Message, NotifyType.ErrorMessage);
                DebugOutput(ex.ToString());
            }
            m_frameAquisitionLock.Release();

            // cache camera selection
            m_selectedMediaFrameSourceGroup = m_mediaFrameSourceGroupList[UICmbCamera.SelectedIndex];

            // Create MediaCapture and its settings
            m_mediaCapture = new MediaCapture();
            var settings = new MediaCaptureInitializationSettings
            {
                SourceGroup = m_selectedMediaFrameSourceGroup,
                MemoryPreference = MediaCaptureMemoryPreference.Cpu,
                StreamingCaptureMode = StreamingCaptureMode.Video,
                SharingMode = MediaCaptureSharingMode.ExclusiveControl
            };

            // Initialize MediaCapture
            try
            {
                await m_mediaCapture.InitializeAsync(settings);

                if (await StartPreviewAsync())
                {
                    // Query the DDIs we cover in this sample and refresh the UI for each of them relative to how they are supported (or not)
                    QueryDDIsAndRefreshUI();
                }
                else
                {
                    throw new Exception("Could not start preview");
                }
            }
            catch (Exception ex)
            {
                NotifyUser(ex.Message, NotifyType.ErrorMessage);
                DebugOutput(ex.ToString());
                return;
            }
            NotifyUser("Done initializing", NotifyType.StatusMessage);
        }

        /// <summary>
        /// Query a set of DDIs and refresh their related UI
        /// </summary>
        private void QueryDDIsAndRefreshUI()
        {
            // Debug output to see if any of the controls are supported
            string statusText = "control support:";
            Dictionary<ExtendedControlKind, IExtendedPropertyHeader> controlGetPayload = new Dictionary<ExtendedControlKind, IExtendedPropertyHeader>();

            // The set of extended properties we will be trying to use in this sample
            List<ExtendedControlKind> controlsToTry = new List<ExtendedControlKind>
                    {
                        ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_FIELDOFVIEW2,
                        ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_FIELDOFVIEW2_CONFIGCAPS,
                        ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_FRAMERATE_THROTTLE,
                        ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION,
                        ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION                    
                    };

            // 1. for each of the above DDIs, we retrieve the payload returned in response to a GET call
            foreach (ExtendedControlKind controlId in controlsToTry)
            {
                controlGetPayload[controlId] = PropertyInquiry.GetExtendedControl(m_selectedMediaFrameSource.Controller.VideoDeviceController, controlId);
                var controlQueryResult = controlGetPayload[controlId];
                bool isSupported = controlQueryResult != null;
                statusText += $"\n-> {controlId} : {isSupported}";
                if (isSupported)
                {
                    statusText += $" | Capability: {controlQueryResult.Capability} | Flags: {controlQueryResult.Flags} | Size: {controlQueryResult.Size}";

                    // If KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_MASK is a supported capability,
                    // then we expect to retrieve a set of configurations in which the driver can provide a segmentation mask and execute blur
                    if (controlId == ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION
                        && ((int)controlQueryResult.Capability & (int)BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_MASK) != 0)
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

            // 2. for each of the above DDIs, we parse their GET payload then refresh the UI to reflect how the control is supported
            try
            {
                // display debug output
                UIControlCapabilityText.Text = statusText;

                // populate and re-enable parts of the UI
                UIPreviewElement_SizeChanged(null, null);
                UICmbCamera.IsEnabled = true;

                // 2.1 KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION
                {
                    // if background segmentation is supported, enable UI elements
                    UIBackgroundSegmentationPanel.IsEnabled = (controlGetPayload[ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION] != null);
                    if (UIBackgroundSegmentationPanel.IsEnabled)
                    {
                        var backgroundSegmentationCapabilities = new List<string>();
                        int currentFlagValueIndex = 0;
                        foreach (var capability in m_possibleBackgroundSegmentationFlagValues)
                        {
                            // if the driver supports this set of potential flag values in its capability, add it as a choice in the UI
                            if ((capability.Value & (ulong)controlGetPayload[ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION].Capability) == capability.Value)
                            {
                                backgroundSegmentationCapabilities.Add(capability.Key);

                                // if the current value for this control corresponds to this potential flag value, select it
                                if (controlGetPayload[ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION].Flags == capability.Value)
                                {
                                    currentFlagValueIndex = backgroundSegmentationCapabilities.Count - 1;
                                    break;
                                }
                            }
                        }

                        UIBackgroundSegmentationModes.SelectionChanged -= UIBackgroundSegmentationModes_SelectionChanged;
                        UIBackgroundSegmentationModes.ItemsSource = backgroundSegmentationCapabilities;
                        UIBackgroundSegmentationModes.SelectedIndex = currentFlagValueIndex;
                        UIBackgroundSegmentationModes.SelectionChanged += UIBackgroundSegmentationModes_SelectionChanged;

                        m_isBackgroundSegmentationMaskModeOn = (((ulong)BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_MASK & controlGetPayload[ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION].Flags) != 0);
                        UIShowBackgroundImage.IsEnabled = m_isBackgroundSegmentationMaskModeOn;
                        UIShowBackgroundImage.IsChecked = false;
                    }
                }

                // 2.2 KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION
                {
                    // if eye gaze correction is supported, enable UI elements
                    UIEyeGazeCorrectionPanel.IsEnabled = controlGetPayload[ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION] != null;
                    if (UIEyeGazeCorrectionPanel.IsEnabled)
                    {
                        var eyeGazeCorrectionCapabilities = new List<string>();
                        int currentFlagValueIndex = 0;

                        foreach (var capability in m_possibleEyeGazeCorrectionFlagValues)
                        {
                            // if the driver supports this set of potential flag values in its capability, add it as a choice in the UI
                            if ((capability.Value & (ulong)controlGetPayload[ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION].Capability) == capability.Value)
                            {
                                eyeGazeCorrectionCapabilities.Add(capability.Key);

                                // if the current value for this control corresponds to this potential flag value, select it
                                if (controlGetPayload[ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION].Flags == capability.Value)
                                {
                                    currentFlagValueIndex = eyeGazeCorrectionCapabilities.Count - 1;
                                    break;
                                }
                            }
                        }
                        UIEyeGazeCorrectionModes.SelectionChanged -= UIEyeGazeCorrectionModes_SelectionChanged;
                        UIEyeGazeCorrectionModes.ItemsSource = eyeGazeCorrectionCapabilities;
                        UIEyeGazeCorrectionModes.SelectedIndex = currentFlagValueIndex;
                        UIEyeGazeCorrectionModes.SelectionChanged += UIEyeGazeCorrectionModes_SelectionChanged;
                    }
                }

                // 2.3 KSPROPERTY_CAMERACONTROL_EXTENDED_FRAMERATE_THROTTLE
                {
                    // if framerate throttling is supported, enable UI elements
                    UIFramerateThrottlingPanel.IsEnabled = controlGetPayload[ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_FRAMERATE_THROTTLE] != null;
                    if (UIFramerateThrottlingPanel.IsEnabled)
                    {
                        bool currentFlagValueIndex = false;
                        VidProcExtendedPropertyPayload framerateThrottlingPayload = controlGetPayload[ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_FRAMERATE_THROTTLE]
                            as VidProcExtendedPropertyPayload;

                        // if the driver supports KSPROPERTY_CAMERACONTROL_EXTENDED_FRAMERATE_THROTTLE
                        if (framerateThrottlingPayload.Capability == (ulong)FramerateThrottleCapabilityKind.KSCAMERA_EXTENDEDPROP_FRAMERATE_THROTTLE_ON)
                        {
                            // check if the current control value is ON or OFF
                            currentFlagValueIndex = (framerateThrottlingPayload.Flags == (ulong)FramerateThrottleCapabilityKind.KSCAMERA_EXTENDEDPROP_FRAMERATE_THROTTLE_ON);
                        }

                        // refresh UI toggle to reflect the current value
                        UIFramerateThrottlingModes.Toggled -= UIFramerateThrottlingModes_Toggled;
                        UIFramerateThrottlingValue.ValueChanged -= UIFramerateThrottlingValue_ValueChanged;
                        UIFramerateThrottlingModes.IsOn = currentFlagValueIndex;
                        UIFramerateThrottlingValue.Minimum = framerateThrottlingPayload.Min;
                        UIFramerateThrottlingValue.Maximum = framerateThrottlingPayload.Max;
                        UIFramerateThrottlingValue.StepFrequency = framerateThrottlingPayload.Step;
                        UIFramerateThrottlingValue.Value = framerateThrottlingPayload.Value;
                        UIFramerateThrottlingModes.Toggled += UIFramerateThrottlingModes_Toggled;
                        UIFramerateThrottlingValue.ValueChanged += UIFramerateThrottlingValue_ValueChanged;
                    }
                }

                // 2.4 KSPROPERTY_CAMERACONTROL_EXTENDED_FIELDOFVIEW2 and KSPROPERTY_CAMERACONTROL_EXTENDED_FIELDOFVIEW2_CONFIGCAPS
                {
                    // if FieldOfView2 and FieldOfView2_ConfigCaps are supported, enable UI elements to set the field of view
                    UIFieldOfView2Panel.IsEnabled =
                        (controlGetPayload[ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_FIELDOFVIEW2] != null
                        && controlGetPayload[ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_FIELDOFVIEW2_CONFIGCAPS] != null);
                    if (UIFieldOfView2Panel.IsEnabled)
                    {
                        bool currentFlagValueIndex = false;
                        FieldOfView2ConfigCapsPropertyPayload fov2ConfigCapsPayload = controlGetPayload[ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_FIELDOFVIEW2_CONFIGCAPS]
                            as FieldOfView2ConfigCapsPropertyPayload;
                        m_discreteFov2Stops = fov2ConfigCapsPayload.DiscreteFoVStops.ToList();

                        // retrieve the current field of view value
                        var fov2Payload = controlGetPayload[ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_FIELDOFVIEW2]
                            as BasicExtendedPropertyPayload;

                        // refresh UI toggle to reflect the current value and possible other values
                        UIFieldOfView2Modes.SelectionChanged -= UIFieldOfView2Modes_SelectionChanged;
                        UIFieldOfView2Modes.ItemsSource = m_discreteFov2Stops;
                        UIFieldOfView2Modes.SelectedIndex = m_discreteFov2Stops.FindIndex(c => c == fov2Payload.ul);
                        UIFieldOfView2Modes.SelectionChanged += UIFieldOfView2Modes_SelectionChanged;
                    }
                }
            }
            catch (Exception ex)
            {
                NotifyUser(ex.Message, NotifyType.ErrorMessage);
                DebugOutput(ex.ToString());
            }
        }

        private void UIShowBackgroundImage_Checked(object sender, RoutedEventArgs e)
        {
            DebugOutput("show background image checked");

            if (UIShowBackgroundImage.IsChecked == true)
            {
                m_isShowingBackgroundMask = true;
                UIBackgroundSegmentationMaskImage.Visibility = Visibility.Visible;
                UICanvasOverlay.Visibility = Visibility.Visible;

                // spin a frame reader to extract the mask metadata
                var fire_forget = Task.Run(async () =>
                {
                    if (m_frameReader != null)
                    {
                        await m_frameReader.StopAsync();
                        DebugOutput("stop frame reader");
                        m_frameReader.FrameArrived -= FrameReader_FrameArrived;
                        m_frameReader = null;
                    }
                    m_frameReader = await m_mediaCapture.CreateFrameReaderAsync(m_selectedMediaFrameSource);
                    await m_frameReader.StartAsync();
                    DebugOutput("start frame reader");
                    m_frameReader.FrameArrived += FrameReader_FrameArrived;
                });
            }
            else
            {
                UIBackgroundSegmentationMaskImage.Visibility = Visibility.Collapsed;
                m_isShowingBackgroundMask = false;
                m_bboxRenderer.Reset();
                UICanvasOverlay.Visibility = Visibility.Collapsed;
            }
        }

        private void UIEyeGazeCorrectionModes_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            try
            {
                if (UIEyeGazeCorrectionModes.SelectedIndex >= 0)
                {
                    ulong flagValueToSet = m_possibleEyeGazeCorrectionFlagValues[(string)UIEyeGazeCorrectionModes.SelectedItem];
                    PropertyInquiry.SetExtendedControlFlags(m_selectedMediaFrameSource.Controller.VideoDeviceController, ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION, flagValueToSet);
                }
            }
            catch (Exception ex)
            {
                NotifyUser(ex.Message, NotifyType.ErrorMessage);
                DebugOutput(ex.ToString());
            }
        }

        private void UIBackgroundSegmentationModes_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            try
            {
                if (UIBackgroundSegmentationModes.SelectedIndex >= 0)
                {
                    ulong flagValueToSet = m_possibleBackgroundSegmentationFlagValues[(string)UIBackgroundSegmentationModes.SelectedItem];
                    PropertyInquiry.SetExtendedControlFlags(m_selectedMediaFrameSource.Controller.VideoDeviceController, ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION, flagValueToSet);

                    // cache whether or not we are asking for background mask metadata to be produced
                    m_isBackgroundSegmentationMaskModeOn = (((ulong)BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_MASK & flagValueToSet) != 0);
                    UIShowBackgroundImage.IsEnabled = m_isBackgroundSegmentationMaskModeOn;
                    UIShowBackgroundImage.IsChecked = false;
                }
            }
            catch (Exception ex)
            {
                NotifyUser(ex.Message, NotifyType.ErrorMessage);
                DebugOutput(ex.ToString());
            }
        }

        private void UIFramerateThrottlingModes_Toggled(object sender, RoutedEventArgs e)
        {
            try
            {
                ulong flagValueToSet = (ulong)(UIFramerateThrottlingModes.IsOn ? 1 : 0);

                // set KSPROPERTY_CAMERACONTROL_EXTENDED_FRAMERATE_THROTTLE ON or OFF
                PropertyInquiry.SetExtendedControlFlags(
                    m_selectedMediaFrameSource.Controller.VideoDeviceController,
                    ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_FRAMERATE_THROTTLE,
                    flagValueToSet);

                // if we turned the control ON..
                if (UIFramerateThrottlingModes.IsOn)
                {
                    // retrieve the current framerate throttling value
                    VidProcExtendedPropertyPayload framerateThrottlingPayload = PropertyInquiry.GetExtendedControl(
                        m_selectedMediaFrameSource.Controller.VideoDeviceController,
                        ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_FRAMERATE_THROTTLE)
                        as VidProcExtendedPropertyPayload;

                    // refresh UI to reflect the current framerate throttling value
                    UIFramerateThrottlingValue.ValueChanged -= UIFramerateThrottlingValue_ValueChanged;
                    UIFramerateThrottlingValue.Value = framerateThrottlingPayload.Value;
                    UIFramerateThrottlingValue.ValueChanged += UIFramerateThrottlingValue_ValueChanged;
                }
            }
            catch (Exception ex)
            {
                NotifyUser(ex.Message, NotifyType.ErrorMessage);
                DebugOutput(ex.ToString());
            }
        }

        private void UIFramerateThrottlingValue_ValueChanged(object sender, Windows.UI.Xaml.Controls.Primitives.RangeBaseValueChangedEventArgs e)
        {
            try
            {
                // if we turned the control ON..
                if (UIFramerateThrottlingModes.IsOn)
                {
                    // set desired framerate throttling value
                    ulong flagValueToSet = (ulong)(UIFramerateThrottlingModes.IsOn ?
                        FramerateThrottleCapabilityKind.KSCAMERA_EXTENDEDPROP_FRAMERATE_THROTTLE_ON :
                        FramerateThrottleCapabilityKind.KSCAMERA_EXTENDEDPROP_FRAMERATE_THROTTLE_OFF);
                    PropertyInquiry.SetExtendedControlFlagsAndValue(
                        m_selectedMediaFrameSource.Controller.VideoDeviceController,
                        ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_FRAMERATE_THROTTLE,
                        flagValueToSet,
                        (ulong)UIFramerateThrottlingValue.Value);
                }
            }
            catch (Exception ex)
            {
                NotifyUser(ex.Message, NotifyType.ErrorMessage);
                DebugOutput(ex.ToString());
            }
        }

        private void UIFieldOfView2Modes_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            try
            {
                if (UIFieldOfView2Modes.SelectedIndex >= 0)
                {
                    // set desired field of view value
                    PropertyInquiry.SetExtendedControlFlagsAndValue(
                        m_selectedMediaFrameSource.Controller.VideoDeviceController,
                        ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_FIELDOFVIEW2,
                        0,
                        (ulong)m_discreteFov2Stops[UIFieldOfView2Modes.SelectedIndex]);
                }
            }
            catch (Exception ex)
            {
                NotifyUser(ex.Message, NotifyType.ErrorMessage);
                DebugOutput(ex.ToString());
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

        private Windows.Foundation.Rect ToRectInRelativeCoordinates(BitmapBounds bounds, uint imageWidth, uint imageHeight)
        {
            return new Windows.Foundation.Rect(
                (double)bounds.X / imageWidth,
                (double)bounds.Y / imageHeight,
                (double)bounds.Width / imageWidth,
                (double)bounds.Height / imageHeight);
        }


        #endregion DebugMessageHelpers
    }
}
