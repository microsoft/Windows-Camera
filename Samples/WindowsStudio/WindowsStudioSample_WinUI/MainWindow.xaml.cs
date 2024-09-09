using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Runtime.InteropServices;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System.Threading;
using Windows.Media.Capture.Frames;
using Windows.Media.Capture;
using Windows.Media.Playback;
using Microsoft.UI.Dispatching;
using System.Threading.Tasks;
using static WindowsStudioSample_WinUI.KsHelper;
using Windows.Devices.Enumeration;
using Windows.Media.Devices;
using Windows.Media.MediaProperties;
using Windows.Media.Core;
using ControlMonitorHelperWinRT;
using Windows.Graphics.Imaging;
using Microsoft.UI.Xaml.Media.Imaging;


namespace WindowsStudioSample_WinUI;
/// <summary>
/// An empty window that can be used on its own or navigated to within a Frame.
/// </summary>
public sealed partial class MainWindow : Window
{
    // Camera related
    private MediaCapture m_mediaCapture;
    private MediaPlayer m_mediaPlayer;
    private MediaFrameSource m_selectedMediaFrameSource;
    private MediaFrameFormat m_selectedFormat;
    private ControlMonitorManager m_controlManager = null;
    private DispatcherQueue m_uiDispatcherQueue = null;
    private MediaFrameReader m_mediaFrameReader = null;
    private SemaphoreSlim m_backgroundSegmentationImageRefreshLock = new SemaphoreSlim(1);
    private SemaphoreSlim m_initLock = new(1);
    private SemaphoreSlim m_frameReadingLock = new(1);
    private SoftwareBitmapSource m_backgroundMaskImageSource = new SoftwareBitmapSource();
    private bool m_isShowingMask = false;

    // this DevPropKey signifies if the system is capable of exposing a Windows Studio camera
    private const string DEVPKEY_DeviceInterface_IsWindowsCameraEffectAvailable = "{6EDC630D-C2E3-43B7-B2D1-20525A1AF120} 4";

    private enum AppState
    {
        Unitialized,
        Initialized,
        Error
    }

    private AppState m_appState = AppState.Unitialized;

    // Lookup table for Background Segmentation DDI flag values and UI names
    readonly Dictionary<string, ulong> m_possibleBackgroundSegmentationFlagValues = new()
        {
            {
                "Off",
                (ulong)BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_OFF
            },
            {
                "Standard blur",
                (ulong)BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR
            },
            {
                "Mask metadata",
                (ulong)BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_MASK
            },
            {
                "Portrait blur",
                (ulong)BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR
                    + (ulong)BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_SHALLOWFOCUS
            },
            {
                "Standard blur + mask metadata",
                (ulong)BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR
                    + (ulong)BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_MASK
            },
            {
                "Portrait blur + mask metadata",
                (ulong)BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR
                    + (ulong)BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_MASK
                    + (ulong)BackgroundSegmentationCapabilityKind.KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_SHALLOWFOCUS
            }
        };

    // Lookup table for Eye Gaze Correction DDI flag values and UI names
    readonly Dictionary<string, ulong> m_possibleEyeGazeCorrectionFlagValues = new()
        {
            {
                "Off",
                (ulong)EyeGazeCorrectionCapabilityKind.KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_OFF
            },
            {
                "Standard",
                (ulong)EyeGazeCorrectionCapabilityKind.KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_ON
            },
            {
                "Teleprompter",
                (ulong)EyeGazeCorrectionCapabilityKind.KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_ON
                    + (ulong)EyeGazeCorrectionCapabilityKind.KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_STARE
            }
        };

    // Lookup table for Creative Filters DDI flag values and UI names
    readonly Dictionary<string, ulong> m_possibleCreativeFilterFlagValues = new()
        {
            {
                "Off",
                (ulong)CreativeFilterCapabilityKind.KSCAMERA_WINDOWSSTUDIO_CREATIVEFILTER_OFF
            },
            {
                "Illustrated",
                (ulong)CreativeFilterCapabilityKind.KSCAMERA_WINDOWSSTUDIO_CREATIVEFILTER_ILLUSTRATED
            },
            {
                "Animated",
                (ulong)CreativeFilterCapabilityKind.KSCAMERA_WINDOWSSTUDIO_CREATIVEFILTER_ANIMATED
            },
            {
                "Watercolor",
                (ulong)CreativeFilterCapabilityKind.KSCAMERA_WINDOWSSTUDIO_CREATIVEFILTER_WATERCOLOR
            }
        };

    public MainWindow()
    {
        this.InitializeComponent();
        this.ExtendsContentIntoTitleBar = true;;
        this.SetTitleBar(UITitle);
        
        m_uiDispatcherQueue = DispatcherQueue.GetForCurrentThread();

        var fireAndForget = InitializeCameraAndUI();
    }

    /// <summary>
    /// Uninitialize the camera
    /// </summary>
    private async Task UninitializeCamera()
    {
        if (m_mediaPlayer != null)
        {
            m_mediaPlayer.Source = null;
            m_mediaPlayer = null;
        }
        m_frameReadingLock.Wait();
        m_backgroundSegmentationImageRefreshLock.Wait();
        if (m_mediaFrameReader != null)
        {
            m_mediaFrameReader.FrameArrived -= MediaFrameReader_FrameArrived;
            await m_mediaFrameReader.StopAsync();
            m_mediaFrameReader = null;
        }
        if (m_mediaCapture != null)
        {
            m_mediaCapture = null;
        }
        m_backgroundSegmentationImageRefreshLock.Release();
        m_frameReadingLock.Release();

        if (m_controlManager != null)
        {
            m_controlManager.CameraControlChanged -= CameraControlMonitor_ControlChanged;
            m_controlManager = null;
        }

        m_appState = AppState.Unitialized;
    }

    /// <summary>
    /// Initialize the camera and the UI states
    /// </summary>
    /// <returns></returns>
    private async Task InitializeCameraAndUI()
    {
        if (m_appState != AppState.Unitialized)
        {
            return;
        }

        try
        {
            m_initLock.Wait();

            // Attempt to find a Windows Studio camera then initialize a MediaCapture instance
            // for it and start streaming
            await InitializeWindowsStudioMediaCaptureAsync();

            // verify if Windows Studio Effects version 2 is supported
            try
            {
                byte[] byteResultPayload = GetExtendedControlPayload(
                    m_mediaCapture.VideoDeviceController,
                    KSPROPERTYSETID_WindowsCameraEffect,
                    (uint)KSPROPERTY_CAMERACONTROL_WINDOWS_EFFECTS.KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_SUPPORTED);

                // reinterpret the byte array as an extended property payload
                KSCAMERA_EXTENDEDPROP_HEADER payloadHeader = FromBytes<KSCAMERA_EXTENDEDPROP_HEADER>(byteResultPayload);
                int sizeofHeader = Marshal.SizeOf<KSCAMERA_EXTENDEDPROP_HEADER>();
                int sizeofKsProperty = Marshal.SizeOf<KsProperty>(); ;
                int supportedControls = ((int)payloadHeader.Size - sizeofHeader) / sizeofKsProperty;

                for (int i = 0; i < supportedControls; i++)
                {
                    KsProperty payloadKsProperty = FromBytes<KsProperty>(byteResultPayload, sizeofKsProperty, sizeofHeader + i * sizeofKsProperty);

                    if (new Guid(payloadKsProperty.Set) == KSPROPERTYSETID_WindowsCameraEffect)
                    {
                        if (payloadKsProperty.Id == (uint)KSPROPERTY_CAMERACONTROL_WINDOWS_EFFECTS.KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_STAGELIGHT)
                        {
                            RefreshStageLightUI();
                        }
                        else if (payloadKsProperty.Id == (uint)KSPROPERTY_CAMERACONTROL_WINDOWS_EFFECTS.KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_CREATIVEFILTER)
                        {
                            RefreshCreativeFilterUI();
                        }
                    }
                }
            }
            catch (Exception)
            {
                // Windows Studio Effects v2 not supported
                Debug.Print("Windows Studio Effects v2 not supported");
            }

            // create a CameraControlMonitor to be alerted when a control value is changed from outside our app
            // while we are using it (i.e. via the Windows Settings)
            List<ControlData> controlDataToMonitor = new List<ControlData>
                {
                    new()
                    {
                        controlKind = ControlKind.All,
                        controlId = 0
                    }
                };

            m_controlManager = ControlMonitorManager.CreateCameraControlMonitor(
                m_mediaCapture.MediaCaptureSettings.VideoDeviceId,
                controlDataToMonitor.ToArray());
            m_controlManager.CameraControlChanged += CameraControlMonitor_ControlChanged;

            // Refresh UI element associated with Windows Studio effects (version 1)
            RefreshEyeGazeUI();
            RefreshBackgroundSegmentationUI();
            RefreshAutomaticFramingUI();
        }
        catch (Exception ex)
        {
            NotifyUser(ex.Message, NotifyType.ErrorMessage);
            await UninitializeCamera();
            m_appState = AppState.Error;
        }
        finally
        {
            m_initLock.Release();
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
            throw new InvalidOperationException("Windows Studio Effects not supported");
        }

        // Windows Studio is currently only supported on front facing cameras.
        // Look at the panel information to identify which camera is front facing
        DeviceInformation selectedDeviceInfo = deviceInfoCollection.FirstOrDefault(x => x.EnclosureLocation != null && x.EnclosureLocation.Panel == Windows.Devices.Enumeration.Panel.Front);
        if (selectedDeviceInfo == null)
        {
            throw new InvalidOperationException("Could not find a front facing camera with Windows Studio Effects");
        }

        // Initialize MediaCapture with the Windows Studio camera device id
        m_mediaCapture = new MediaCapture();

        m_mediaCapture.Failed += MediaCapture_Failed;

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

        UILaunchSettingsPage.IsEnabled = true;

        // Check if we can launch the Windows Studio quick setting panel using URI
        var uri = new Uri($"ms-controlcenter:studioeffects");
        var supportStatus = await Windows.System.Launcher.QueryUriSupportAsync(uri, Windows.System.LaunchQuerySupportType.Uri);
        if (supportStatus == Windows.System.LaunchQuerySupportStatus.Available)
        {
            UILaunchQuickSettingsPanel.IsEnabled = true;
        }

        // create a frame reader and register a callback to monitor if background segmentation metadata is available
        // on each frame so that we can visualize the mask frame
        m_mediaFrameReader = await m_mediaCapture.CreateFrameReaderAsync(m_selectedMediaFrameSource);
        m_mediaFrameReader.FrameArrived += MediaFrameReader_FrameArrived;
        await m_mediaFrameReader.StartAsync();
    }

    /// <summary>
    /// Callback for when a new frame is available
    /// </summary>
    /// <param name="sender"></param>
    /// <param name="args"></param>
    private void MediaFrameReader_FrameArrived(MediaFrameReader sender, MediaFrameArrivedEventArgs args)
    {
        var frame = sender.TryAcquireLatestFrame();
        if (frame != null)
        {
            if(m_frameReadingLock.Wait(0))
            {
                SoftwareBitmap maskFrame = null;

                // Attempt to extract the background segmentation mask metadata from the frame
                var maskSoftwareBitmap = ExtractBackgroundSegmentationMaskMetadataAsBitmap(frame);
                if (maskSoftwareBitmap != null)
                {
                    maskFrame = SoftwareBitmap.Convert(maskSoftwareBitmap, BitmapPixelFormat.Bgra8, BitmapAlphaMode.Ignore);
                }
                
                // if we have mask metadata visualize it
                if (maskFrame != null)
                {
                    m_isShowingMask = true;
                    m_uiDispatcherQueue.TryEnqueue(async () =>
                    {
                        if (m_backgroundSegmentationImageRefreshLock.Wait(0))
                        {

                            await m_backgroundMaskImageSource.SetBitmapAsync(maskFrame);
                            UIMaskPreviewElement.Source = m_backgroundMaskImageSource;
                            m_backgroundSegmentationImageRefreshLock.Release();
                        }
                    });
                }
                // if we were showing the mask frame and we don't have mask metadata, stop doing so
                else if (maskFrame == null && m_isShowingMask)
                {
                    m_isShowingMask = false;
                    m_uiDispatcherQueue.TryEnqueue(() =>
                    {
                        m_backgroundSegmentationImageRefreshLock.Wait();
                        UIMaskPreviewElement.Source = null;
                        m_backgroundSegmentationImageRefreshLock.Release();
                    });
                }
                m_frameReadingLock.Release();
            }
        }
    }

    /// <summary>
    /// Callback for when MediaCapture fails
    /// </summary>
    /// <param name="sender"></param>
    /// <param name="errorEventArgs"></param>
    private async void MediaCapture_Failed(MediaCapture sender, MediaCaptureFailedEventArgs errorEventArgs)
    {
        m_initLock.Wait();
        NotifyUser(errorEventArgs.Message, NotifyType.ErrorMessage);
        var t = UninitializeCamera();
        m_appState = AppState.Error;
        m_initLock.Release();
    }


    /// <summary>
    /// Callback when a monitored camera control changes so that we can refresh the UI accordingly
    /// </summary>
    /// <param name="sender"></param>
    /// <param name="control"></param>
    /// <exception cref="Exception"></exception>
    private void CameraControlMonitor_ControlChanged(object sender, ControlData control)
    {
        Debug.WriteLine($"{(ExtendedControlKind)((uint)control.controlId)} changed externally");
        if (control.controlKind == ControlKind.ExtendedControlKind)
        {
            switch (control.controlId)
            {
                case (uint)ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION:
                    m_uiDispatcherQueue.TryEnqueue(DispatcherQueuePriority.Normal, () => RefreshEyeGazeUI());
                    break;

                case (uint)ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION:
                    m_uiDispatcherQueue.TryEnqueue(DispatcherQueuePriority.Normal, () => RefreshBackgroundSegmentationUI());
                    break;

                case (uint)ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_DIGITALWINDOW:
                    m_uiDispatcherQueue.TryEnqueue(DispatcherQueuePriority.Normal, () => RefreshAutomaticFramingUI());
                    break;

                default:
                    throw new Exception("unhandled control change, implement or allow through at your convenience");
            }
        }
        else if (control.controlKind == ControlKind.WindowsStudioEffectsKind)
        {
            switch (control.controlId)
            {
                case (uint)KSPROPERTY_CAMERACONTROL_WINDOWS_EFFECTS.KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_STAGELIGHT:
                    m_uiDispatcherQueue.TryEnqueue(DispatcherQueuePriority.Normal, () => RefreshStageLightUI());
                    break;

                case (uint)KSPROPERTY_CAMERACONTROL_WINDOWS_EFFECTS.KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_CREATIVEFILTER:
                    m_uiDispatcherQueue.TryEnqueue(DispatcherQueuePriority.Normal, () => RefreshCreativeFilterUI());
                    break;

                default:
                    throw new Exception("unhandled Windows Studio Effects control change, implement or allow through at your convenience");
            }
        }
    }

    /// <summary>
    /// get the current value for eye gaze correction and update UI accordingly
    /// </summary>
    private void RefreshEyeGazeUI()
    {
        // send a GET call for the eye gaze DDI and retrieve the result payload
        byte[] byteResultPayload = GetExtendedControlPayload(
            m_mediaCapture.VideoDeviceController,
            KSPROPERTYSETID_ExtendedCameraControl,
            (uint)ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION);

        // reinterpret the byte array as an extended property payload
        KsBasicCameraExtendedPropPayload payload = FromBytes<KsBasicCameraExtendedPropPayload>(byteResultPayload);

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
        byte[] byteResultPayload = GetExtendedControlPayload(
            m_mediaCapture.VideoDeviceController,
            KSPROPERTYSETID_ExtendedCameraControl,
            (uint)ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION);

        // reinterpret the byte array as an extended property payload
        KsBackgroundCameraExtendedPropPayload payload = FromBytes(byteResultPayload);

        // refresh the list of values displayed for this control
        {
            UIBackgroundSegmentationModes.SelectionChanged -= UIBackgroundSegmentationModes_SelectionChanged;

            var backgroundSegmentationCapabilities = m_possibleBackgroundSegmentationFlagValues.Where(x => (x.Value & payload.header.Capability) == x.Value).ToList();
            UIBackgroundSegmentationModes.ItemsSource = backgroundSegmentationCapabilities.Select(x => x.Key);

            // reflect in UI what is the current value
            var currentFlag = payload.header.Flags;
            var currentIndexToSelect = backgroundSegmentationCapabilities.FindIndex(x => x.Value == currentFlag);
            UIBackgroundSegmentationModes.SelectedIndex = currentIndexToSelect;

            UIBackgroundSegmentationModes.IsEnabled = true;
            UIBackgroundSegmentationModes.SelectionChanged += UIBackgroundSegmentationModes_SelectionChanged;
        };
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

    /// <summary>
    /// get the current value for Windows Studio Effects StageLight and update UI accordingly
    /// </summary>
    private void RefreshStageLightUI()
    {
        // send a GET call for the StageLight DDI and retrieve the result payload
        byte[] byteResultPayload = GetExtendedControlPayload(
            m_mediaCapture.VideoDeviceController,
            KSPROPERTYSETID_WindowsCameraEffect,
            (uint)KSPROPERTY_CAMERACONTROL_WINDOWS_EFFECTS.KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_STAGELIGHT);

        // reinterpret the byte array as an extended property payload
        KsBasicCameraExtendedPropPayload payload = FromBytes<KsBasicCameraExtendedPropPayload>(byteResultPayload);

        // reflect in UI what is the current value
        UIStageLightSwitch.Toggled -= UIStageLightSwitch_Toggled;
        UIStageLightSwitch.IsOn = (payload.header.Flags == (uint)StageLightCapabilityKind.KSCAMERA_WINDOWSSTUDIO_STAGELIGHT_ON);
        UIStageLightSwitch.IsEnabled = true;
        UIStageLightSwitch.Toggled += UIStageLightSwitch_Toggled;
    }

    /// <summary>
    /// get the current value for Windows Studio Effects CreativeFilter and update UI accordingly
    /// </summary>
    private void RefreshCreativeFilterUI()
    {
        // send a GET call for the CreativeFilter DDI and retrieve the result payload
        byte[] byteResultPayload = GetExtendedControlPayload(
            m_mediaCapture.VideoDeviceController,
            KSPROPERTYSETID_WindowsCameraEffect,
            (uint)KSPROPERTY_CAMERACONTROL_WINDOWS_EFFECTS.KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_CREATIVEFILTER);

        // reinterpret the byte array as an extended property payload
        KsBasicCameraExtendedPropPayload payload = FromBytes<KsBasicCameraExtendedPropPayload>(byteResultPayload);

        // refresh the list of values displayed for this control
        UICreativeFilterModes.SelectionChanged -= UICreativeFilterModes_SelectionChanged;

        var creativeFilterCapabilities = m_possibleCreativeFilterFlagValues.Where(x => (x.Value & payload.header.Capability) == x.Value).ToList();
        UICreativeFilterModes.ItemsSource = creativeFilterCapabilities.Select(x => x.Key);

        // reflect in UI what is the current value
        var currentFlag = payload.header.Flags;
        var currentIndexToSelect = creativeFilterCapabilities.FindIndex(x => x.Value == currentFlag);
        UICreativeFilterModes.SelectedIndex = currentIndexToSelect;

        UICreativeFilterModes.IsEnabled = true;
        UICreativeFilterModes.SelectionChanged += UICreativeFilterModes_SelectionChanged;
    }

    #region UICallbacks
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
        SetExtendedControlFlags(
            m_mediaCapture.VideoDeviceController,
            KSPROPERTYSETID_ExtendedCameraControl,
            (uint)ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION,
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
        SetExtendedControlFlags(
            m_mediaCapture.VideoDeviceController,
            KSPROPERTYSETID_ExtendedCameraControl,
            (uint)ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION,
            flagToSet);
    }

    private void UIAutomaticFramingSwitch_Toggled(object sender, RoutedEventArgs e)
    {
        // set automatic framing On or Off given following the toggle switch state
        DigitalWindowMode modeToSet = UIAutomaticFramingSwitch.IsOn ? DigitalWindowMode.Auto : DigitalWindowMode.Off;
        m_mediaCapture.VideoDeviceController.DigitalWindowControl.Configure(modeToSet);
    }

    private void UIStageLightSwitch_Toggled(object sender, RoutedEventArgs e)
    {
        // find the flags value associated with the current toggle value selected
        ulong flagToSet = (uint)(UIStageLightSwitch.IsOn == true ? StageLightCapabilityKind.KSCAMERA_WINDOWSSTUDIO_STAGELIGHT_ON : StageLightCapabilityKind.KSCAMERA_WINDOWSSTUDIO_STAGELIGHT_OFF);

        // set the flags value for the corresponding extended control
        SetExtendedControlFlags(
            m_mediaCapture.VideoDeviceController,
            KSPROPERTYSETID_WindowsCameraEffect,
            (uint)KSPROPERTY_CAMERACONTROL_WINDOWS_EFFECTS.KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_STAGELIGHT,
            flagToSet);
    }

    private void UICreativeFilterModes_SelectionChanged(object sender, SelectionChangedEventArgs e)
    {
        var selectedIndex = UICreativeFilterModes.SelectedIndex;
        if (selectedIndex < 0)
        {
            return;
        }

        // find the flags value associated with the current mode selected
        string selection = UICreativeFilterModes.SelectedItem.ToString();
        ulong flagToSet = m_possibleCreativeFilterFlagValues.FirstOrDefault(x => x.Key == selection).Value;

        // set the flags value for the corresponding extended control
        SetExtendedControlFlags(
            m_mediaCapture.VideoDeviceController,
            KSPROPERTYSETID_WindowsCameraEffect,
            (uint)KSPROPERTY_CAMERACONTROL_WINDOWS_EFFECTS.KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_CREATIVEFILTER,
            flagToSet);
    }

    private void UILaunchSettingsPage_Click(object sender, RoutedEventArgs e)
    {
        // launch Windows Settings page for the camera identified by the specified Id
        var uri = new Uri($"ms-settings:camera?cameraId={Uri.EscapeDataString(m_mediaCapture.MediaCaptureSettings.VideoDeviceId)}");
        var fireAndForget = Windows.System.Launcher.LaunchUriAsync(uri);
    }

    private void UILaunchQuickSettingsPanel_Click(object sender, RoutedEventArgs e)
    {
        // launch the Windows Studio quick setting panel using URI
        var uri = new Uri($"ms-controlcenter:studioeffects");
        var fireAndForget = Windows.System.Launcher.LaunchUriAsync(uri);
    }
    #endregion UICallbacks


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
        if (m_uiDispatcherQueue.HasThreadAccess)
        {
            UpdateStatus(strMessage, type);
        }
        else
        {
            var task = m_uiDispatcherQueue.TryEnqueue(DispatcherQueuePriority.Normal, () => UpdateStatus(strMessage, type));
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
                UIStatusBar.Severity = InfoBarSeverity.Success;
                break;
            case NotifyType.ErrorMessage:
                UIStatusBar.Severity = InfoBarSeverity.Error;
                break;
        }
        UIStatusBar.Message= strMessage;
        Debug.WriteLine($"{type}: {strMessage}");

        // Collapse the StatusBlock if it has no text to conserve UI real estate.
        UIStatusBar.IsOpen = (!string.IsNullOrEmpty(UIStatusBar.Message));
    }
    #endregion DebugMessageHelpers
}
