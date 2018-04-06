//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the Microsoft Public License.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Numerics;
using System.Threading;
using System.Threading.Tasks;
using Windows.ApplicationModel;
using Windows.Devices.Enumeration;
using Windows.Graphics.Display;
using Windows.Graphics.Imaging;
using Windows.Media.Capture;
using Windows.Media.Capture.Frames;
using Windows.Media.Core;
using Windows.Media.Devices;
using Windows.Media.MediaProperties;
using Windows.Media.Playback;
using Windows.Storage;
using Windows.System.Display;
using Windows.UI.Core;
using Windows.UI.Popups;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.UI.Xaml.Controls.Primitives;
using Windows.UI.Xaml.Input;
using Windows.UI.Xaml.Navigation;

namespace Cam360
{
    /// <summary>
    /// An empty page that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class MainPage : Page
    {
        // Prevent the screen from sleeping while the camera is running
        private readonly DisplayRequest _displayRequest = new DisplayRequest();

       // state variables
        private bool _isInitialized = false;
        private bool _isPreviewing = false;
        private bool _isPanning = false;
        private bool _isRecording = false;
        private double _panSpeed;

        private static readonly Semaphore semaphore = new Semaphore(1, 1);
        private string _manufacturer;
        private string _product;

        // Mediacapture and projection variables
        private MediaCapture _mediaCapture;
        private Windows.Media.Effects.VideoTransformEffectDefinition _projectionXVP;
        private MediaFrameSource _selectedFrameSource;
        private DeviceInformationCollection _cameraList;
        private List<MediaFrameSourceGroup> _mediaFrameSourceGroupList;
        private DeviceInformationCollection _microphoneList;
        private MediaPlayer _mediaPlayer;
        private MediaPlaybackSphericalVideoProjection _mediaPlayerProjection;
        private List<MediaFrameFormat> _filteredSourceFormat;
        private LowLagPhotoCapture _lowLagPhotoCapture = null;


        /// <summary>
        /// Constructor
        /// </summary>
        public MainPage()
        {
            this.InitializeComponent();

            Application.Current.Suspending += Application_Suspending;
            Application.Current.Resuming += Application_Resuming;

            var info = (new Windows.Security.ExchangeActiveSyncProvisioning.EasClientDeviceInformation());
            _manufacturer = info.SystemManufacturer;
            _product = info.SystemProductName.Replace('_', '-').Replace(' ', '-');
        }

        /// <summary>
        /// On application resuming
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private async void Application_Resuming(object sender, object e)
        {
            Debug.WriteLine("Application_Resuming");

            SetDisplayRotationPreference();

            await InitializeCameraAsync();
        }

        /// <summary>
        /// On application suspending
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private async void Application_Suspending(object sender, SuspendingEventArgs e)
        {
            Debug.WriteLine("Application_Suspending");
            var deferral = e.SuspendingOperation.GetDeferral();
            if (_isRecording)
            {
                RecordSymbol.Symbol = Symbol.Target;
                await StopVideoRecordingAsync();
                if (_projectionXVP != null)
                {
                    _projectionXVP = null;
                }
            }
            await CleanupCameraAsync();

            ResetDisplayRotationPreference();

            deferral.Complete();
        }

        /// <summary>
        /// On page navigated to
        /// </summary>
        /// <param name="e"></param>
        protected override async void OnNavigatedTo(NavigationEventArgs e)
        {
            Debug.WriteLine("OnNavigatedTo");

            SetDisplayRotationPreference();

            PreviewElement.ManipulationStarted += PreviewElement_ManipulationStarted;
            PreviewElement.ManipulationDelta += PreviewElement_ManipulationDelta;
            PreviewElement.ManipulationCompleted += PreviewElement_ManipulationCompleted;

            await InitializeCameraAsync();
        }

        #region UI callbacks

        /// <summary>
        /// Desired camera changed
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private async void cmbCamera_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            await CleanupCameraAsync();

            if (_mediaFrameSourceGroupList.Count == 0 || cmbCamera.SelectedIndex < 0)
            {
                return;
            }

            var requestedGroup = _mediaFrameSourceGroupList[cmbCamera.SelectedIndex];
            var capturemode = _microphoneList.Count > 0 ? StreamingCaptureMode.AudioAndVideo : StreamingCaptureMode.Video;
            // Create MediaCapture and its settings
            _mediaCapture = new MediaCapture();
            var settings = new MediaCaptureInitializationSettings
            {
                SourceGroup = requestedGroup,
                AlwaysPlaySystemShutterSound = true,
                StreamingCaptureMode = capturemode
            };

            // Initialize MediaCapture
            try
            {
                await _mediaCapture.InitializeAsync(settings);

                _selectedFrameSource = _mediaCapture.FrameSources.FirstOrDefault(source => source.Value.Info.MediaStreamType == MediaStreamType.VideoPreview 
                                                                                           && source.Value.Info.SourceKind == MediaFrameSourceKind.Color).Value;
                if (_selectedFrameSource == null)
                {
                    _selectedFrameSource = _mediaCapture.FrameSources.FirstOrDefault(source => source.Value.Info.MediaStreamType == MediaStreamType.VideoRecord 
                                                                                               && source.Value.Info.SourceKind == MediaFrameSourceKind.Color).Value;
                }

                // if no preview stream are available, bail
                if (_selectedFrameSource == null)
                {
                    throw (new Exception("could not find a VideoPreview or VideoRecord stream on the selected camera"));
                }

                _isInitialized = true;

                await Dispatcher.RunAsync(CoreDispatcherPriority.Normal, () => { RecordButton.IsEnabled = true; PhotoButton.IsEnabled = true; });
            }
            catch (Exception ex)
            {
                TraceException(ex);
            }

            // If initialization succeeded, start the preview
            if (_isInitialized)
            {
                // Populate UI with the available resolution and select the maximum one available
                InitializeResolutionSettings();
            }
        }

        /// <summary>
        /// Desired resolution changed
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private async void cmbResolution_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            try
            {
                if (_isPreviewing)
                {
                    await StopPreviewAsync();
                }

                if (cmbResolution.SelectedIndex >= 0)
                {
                    await _selectedFrameSource.SetFormatAsync(_filteredSourceFormat[cmbResolution.SelectedIndex]);

                    if (!_isPreviewing)
                    {
                        await StartPreviewAsync();
                    }

                    await Dispatcher.RunAsync(CoreDispatcherPriority.Normal, () => {RecordButton.IsEnabled = true; PhotoButton.IsEnabled = true;});
                }
            }
            catch (Exception ex)
            {
                TraceException(ex);
            }
        }

        /// <summary>
        /// Force spherical projection
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void ForceSpherical_Checked(object sender, RoutedEventArgs e)
        {
            cmbResolution_SelectionChanged(null, null);
        }
        /// <summary>
        /// Photo button tapped for capturing a photo
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private async void PhotoButton_Tapped(object sender, TappedRoutedEventArgs e)
        {
            if (_isRecording)
            {
                await CaptureLowLagPhotoAsync();
            }
            else
            {
                await CapturePhotoAsync(); 
            }
        }

        /// <summary>
        /// Record button tapped for either starting or stopping video recording
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private async void RecordButton_Tapped(object sender, TappedRoutedEventArgs e)
        {
            if(_isRecording)
            {
                RecordSymbol.Symbol = Symbol.Target;
                await StopVideoRecordingAsync();
                await _mediaCapture.ClearEffectsAsync(MediaStreamType.VideoRecord);
                if (_projectionXVP != null)
                {
                    if (_projectionXVP.SphericalProjection.IsEnabled)
                    {
                        _mediaPlayerProjection.IsEnabled = (_mediaPlayerProjection.FrameFormat == SphericalVideoFrameFormat.Equirectangular) || (ToggleForceSpherical.IsChecked == true);
                        _mediaPlayerProjection.FrameFormat = _projectionXVP.SphericalProjection.FrameFormat;
                        _mediaPlayerProjection.HorizontalFieldOfViewInDegrees = _projectionXVP.SphericalProjection.HorizontalFieldOfViewInDegrees;
                        _mediaPlayerProjection.ProjectionMode = _projectionXVP.SphericalProjection.ProjectionMode;
                        _mediaPlayerProjection.ViewOrientation = _projectionXVP.SphericalProjection.ViewOrientation;
                    }
                    _projectionXVP = null;
                }
            }
            else
            {
                RecordSymbol.Symbol = Symbol.Stop;
                if (ToggleRecordProjection.IsChecked == true)
                {
                    _projectionXVP = new Windows.Media.Effects.VideoTransformEffectDefinition();
                    _projectionXVP.SphericalProjection.IsEnabled = _mediaPlayerProjection.IsEnabled;
                    _projectionXVP.SphericalProjection.FrameFormat = _mediaPlayerProjection.FrameFormat;
                    _projectionXVP.SphericalProjection.HorizontalFieldOfViewInDegrees = _mediaPlayerProjection.HorizontalFieldOfViewInDegrees;
                    _projectionXVP.SphericalProjection.ProjectionMode = _mediaPlayerProjection.ProjectionMode;
                    _projectionXVP.SphericalProjection.ViewOrientation = _mediaPlayerProjection.ViewOrientation;
                    _mediaPlayerProjection.IsEnabled = false;
                    await _mediaCapture.AddVideoEffectAsync(_projectionXVP, MediaStreamType.VideoRecord);
                }
                await StartVideoRecordingAsync();
            }
            await LockUnlockUIandSettingsAsync(_isRecording);
            _isRecording = !_isRecording;
        }

        /// <summary>
        /// Callback for when the FoV slider value changed
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void SliderFieldOfView_ValueChanged(object sender, RangeBaseValueChangedEventArgs e)
        {
            if ((_mediaPlayerProjection != null) && _mediaPlayerProjection.IsEnabled)
            {
                _mediaPlayerProjection.HorizontalFieldOfViewInDegrees = SliderFieldOfView.Value;
            }
            if (_projectionXVP != null)
            {
                _projectionXVP.SphericalProjection.HorizontalFieldOfViewInDegrees = SliderFieldOfView.Value;
            }
        }

        /// <summary>
        /// Callback for when the PreviewElement manipulation completed
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void PreviewElement_ManipulationCompleted(object sender, ManipulationCompletedRoutedEventArgs e)
        {
            if (_isPanning)
            {
                // hide UI insight on how to interact with the view
                PanningGuidance.Visibility = Visibility.Collapsed;

                _isPanning = false;
            }
        }

        /// <summary>
        /// Callback for when the PreviewElement manipulation changes
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void PreviewElement_ManipulationDelta(object sender, ManipulationDeltaRoutedEventArgs e)
        {
            double horizontalMovementFactor = (PreviewElement.FlowDirection == FlowDirection.LeftToRight ? -1 : 1) * (e.Delta.Translation.X * _panSpeed);
            double verticalMovementFactor = e.Delta.Translation.Y * _panSpeed;
            double rotation = e.Delta.Rotation * -1;
            if (_mediaPlayerProjection.IsEnabled)
            {
                _mediaPlayerProjection.ViewOrientation *= CreateFromHeadingPitchRoll(horizontalMovementFactor, verticalMovementFactor, rotation);
            }
            if (_projectionXVP != null)
            {
                _projectionXVP.SphericalProjection.ViewOrientation *= CreateFromHeadingPitchRoll(horizontalMovementFactor, verticalMovementFactor, rotation);
            }
        }

        /// <summary>
        /// Callback for when the PreviewElement manipulation started
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void PreviewElement_ManipulationStarted(object sender, ManipulationStartedRoutedEventArgs e)
        {
            if ((_mediaPlayerProjection != null && _mediaPlayerProjection.IsEnabled)
                   || (_projectionXVP != null && _projectionXVP.SphericalProjection.IsEnabled))
            {
                _isPanning = true;

                // Show UI insight on how to interact with the view
                PanningGuidance.Visibility = Visibility.Visible;
                if (_projectionXVP != null)
                {
                    _panSpeed = _projectionXVP.SphericalProjection.HorizontalFieldOfViewInDegrees / (PreviewElement.ActualWidth);
                }
                else if (_mediaPlayerProjection != null)
                {
                    _panSpeed = _mediaPlayerProjection.HorizontalFieldOfViewInDegrees / (PreviewElement.ActualWidth); // degrees per pixel pan
                }
            }
        }

        /// <summary>
        /// Resets the projection orientation
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void ResetView_Click(object sender, RoutedEventArgs e)
        {
            if (_mediaPlayerProjection != null
                   && _mediaPlayerProjection.IsEnabled
                   && _mediaPlayerProjection.FrameFormat == SphericalVideoFrameFormat.Equirectangular)
            {
                if (_mediaPlayerProjection.IsEnabled)
                {
                    _mediaPlayerProjection.ViewOrientation = Quaternion.Identity;
                }
                if (_projectionXVP != null)
                {
                    _projectionXVP.SphericalProjection.ViewOrientation = Quaternion.Identity;
                }
                SliderFieldOfView.Value = 120;
            }
        }

        #endregion UI callbacks

        /// <summary>
        /// Trace and show exception to the UI
        /// </summary>
        /// <param name="ex"></param>
        /// <param name="memberName"></param>
        private void TraceException(
            Exception ex,
            [System.Runtime.CompilerServices.CallerMemberName] string memberName = ""
            )
        {
            string m = memberName + "\n" + ex.ToString();
            ExceptionText.Text = m;
            ExceptionText.Visibility = Visibility.Visible;
            Debug.WriteLine(m + ex.ToString());
        }

        /// <summary>
        /// Set display rotation preference
        /// </summary>
        private void SetDisplayRotationPreference()
        {
            // Attempt to lock page to landscape orientation to prevent the CaptureElement from rotating
            DisplayInformation.AutoRotationPreferences = DisplayOrientations.Landscape;
        }

        /// <summary>
        /// Reset display rotation preference
        /// </summary>
        private void ResetDisplayRotationPreference()
        {
            // Revert orientation preferences
            DisplayInformation.AutoRotationPreferences = DisplayOrientations.None;
        }

        /// <summary>
        /// Locking or unlocking the relevant UI controls
        /// </summary>
        /// <param name="lockingValue"></param>
        /// <returns></returns>
        private async Task LockUnlockUIandSettingsAsync(bool unlockingValue)
        {
            Debug.WriteLine($"unlocking UI: {unlockingValue}");

            await Dispatcher.RunAsync(CoreDispatcherPriority.Normal, () =>
            {
                cmbResolution.IsEnabled = unlockingValue;
                cmbCamera.IsEnabled = unlockingValue;
            });
        }

        #region Camera methods

        /// <summary>
        /// Initialize camera
        /// </summary>
        /// <returns></returns>
        private async Task InitializeCameraAsync()
        {
            try
            {
                Debug.WriteLine("InitializeCameraAsync");

                if (_mediaCapture == null)
                {
                    IReadOnlyList<MediaFrameSourceGroup> allGroups = await MediaFrameSourceGroup.FindAllAsync();
                    List<MediaFrameSourceGroup> allGroupsList = allGroups.ToList();
                    string audioDeviceSelectorString = MediaDevice.GetAudioCaptureSelector();
                    foreach (var sourceGroup in allGroupsList)
                    {
                        Debug.WriteLine($"{ sourceGroup.DisplayName}");
                        List<MediaFrameSourceInfo> sourceInfo = sourceGroup.SourceInfos.ToList();
                        var names = Enum.GetNames(typeof(MediaFrameSourceKind));
                        var streamTypes = Enum.GetNames(typeof(MediaStreamType));
                        foreach (var info in sourceInfo)
                        {
                            Debug.WriteLine($" {info.DeviceInformation.Name} ---- { names[(int)info.SourceKind]} -- {streamTypes[(int)info.MediaStreamType]}");
                        }
                    }

                    _mediaFrameSourceGroupList = allGroupsList.Where(group => group.SourceInfos.Any(sourceInfo => sourceInfo.SourceKind == MediaFrameSourceKind.Color 
                                                                                                                  && (sourceInfo.MediaStreamType == MediaStreamType.VideoPreview 
                                                                                                                      || sourceInfo.MediaStreamType == MediaStreamType.VideoRecord))).ToList();
                    _microphoneList = await DeviceInformation.FindAllAsync(audioDeviceSelectorString);

                    _cameraList = await DeviceInformation.FindAllAsync(DeviceClass.VideoCapture);

                    if (_mediaFrameSourceGroupList.Count > 0)
                    {
                        var cameraNamesList = _mediaFrameSourceGroupList.Select(group => group.DisplayName);
                        cmbCamera.ItemsSource = cameraNamesList;
                        cmbCamera.SelectedIndex = 0;

                        await LockUnlockUIandSettingsAsync(true);
                    }
                    else
                    {
                        string errorMessage = "No camera device found!";
                        Debug.WriteLine(errorMessage);
                        throw (new Exception(errorMessage));
                    }
                }
            }
            catch (Exception ex)
            {
                TraceException(ex);
            }
        }

        /// <summary>
        /// Find and expose all formats exposed by the frame source currently toggled 
        /// </summary>
        private void InitializeResolutionSettings()
        {
            cmbResolution.Items.Clear();
            _filteredSourceFormat = new List<MediaFrameFormat>();
            List<MediaFrameFormat> supportedFormats = _selectedFrameSource.SupportedFormats.ToList();
            uint maxResolution = 0;
            int maxResolutionPropertyIndex = 0;
            int supportedFormatIndex = 0;
            foreach(MediaFrameFormat format in supportedFormats)
            {
                VideoMediaFrameFormat videoFormat = format.VideoFormat;
                string subtype = format.Subtype.ToLowerInvariant();
                if (subtype == MediaEncodingSubtypes.Mjpg.ToLowerInvariant()
                    || subtype == MediaEncodingSubtypes.Rgb24.ToLowerInvariant())
                {
                    continue;
                }

                uint resolution = videoFormat.Width * videoFormat.Height;
                if (resolution > maxResolution)
                {
                    maxResolution = resolution;
                    maxResolutionPropertyIndex = supportedFormatIndex;
                }

                Debug.WriteLine("Resolution of : {0} x {1}", videoFormat.Width, videoFormat.Height);
                Debug.WriteLine("Type: {0} and subtype: {1}", format.MajorType, format.Subtype);

                string aspectRatio = GetAspectRatio(videoFormat.Width, videoFormat.Height);
                cmbResolution.Items.Add(string.Format("{0}, {1}, {2}MP, {3}x{4}@{5}fps",
                    format.Subtype, 
                    aspectRatio, 
                    ((float)((videoFormat.Width * videoFormat.Height) / 1000000.0)).ToString("0.00"), 
                    videoFormat.Width, videoFormat.Height,
                    format.FrameRate.Numerator/format.FrameRate.Denominator));

                _filteredSourceFormat.Add(format);
                supportedFormatIndex++;
            }

            // Select maximum resolution available
            cmbResolution.SelectedIndex = maxResolutionPropertyIndex;
        }

        /// <summary>
        /// Cleanup camera instances
        /// </summary>
        /// <returns></returns>
        private async Task CleanupCameraAsync()
        {
            Debug.WriteLine("CleanupCameraAsync");

            if (_isInitialized)
            {
                if (_isPreviewing)
                {
                    await StopPreviewAsync();
                }
                // Allow the device to sleep now that the preview is stopped
                _displayRequest.RequestRelease();
            }

            _selectedFrameSource = null;
            if (_mediaCapture != null)
            {
                _mediaCapture.Dispose();
                _mediaCapture = null;
                _isInitialized = false;
            }
        }

        /// <summary>
        /// Start previewing
        /// </summary>
        /// <returns></returns>
        private async Task StartPreviewAsync()
        {
            _mediaPlayer = new MediaPlayer();
            _mediaPlayer.RealTimePlayback = true;
            _mediaPlayer.AutoPlay = true;
            _mediaPlayer.Source = MediaSource.CreateFromMediaFrameSource(_selectedFrameSource);
            PreviewElement.SetMediaPlayer(_mediaPlayer);
            _mediaPlayerProjection = _mediaPlayer.PlaybackSession.SphericalVideoProjection;

            // Set up the UI to preview the stream
            await Dispatcher.RunAsync(CoreDispatcherPriority.Normal, () =>
            {
                if (ToggleForceSpherical.IsChecked == true)
                {
                    // force spherical projection format from UI button
                    _mediaPlayerProjection.FrameFormat = SphericalVideoFrameFormat.Equirectangular;
                    _mediaPlayerProjection.IsEnabled = true;
                    _mediaPlayerProjection.HorizontalFieldOfViewInDegrees = 120;
                    if (_projectionXVP != null)
                    {
                        _projectionXVP.SphericalProjection.HorizontalFieldOfViewInDegrees = 120;
                    }
                }
                else
                {
                    _mediaPlayerProjection.IsEnabled = (_mediaPlayerProjection.FrameFormat == SphericalVideoFrameFormat.Equirectangular);
                }

                // Show vertical scroll bar for zoom
                FieldOfViewControl.Visibility = _mediaPlayerProjection.IsEnabled ?
                    Visibility.Visible
                    : Visibility.Collapsed;

                PreviewElement.ManipulationMode = _mediaPlayerProjection.IsEnabled ?
                    ManipulationModes.TranslateX | ManipulationModes.TranslateY | ManipulationModes.Rotate | ManipulationModes.TranslateInertia
                    : ManipulationModes.None;

                // Prevent the device from sleeping while we run the preview
                _displayRequest.RequestActive();

                _isPreviewing = true;
            });
        }

        /// <summary>
        /// Stop previewing
        /// </summary>
        /// <returns></returns>
        private async Task StopPreviewAsync()
        {
            try
            {
                _mediaPlayerProjection = null;
                _mediaPlayer = null;
                _isPreviewing = false;
            }
            catch (Exception ex)
            {
                TraceException(ex);
            }
            finally
            {
                await Dispatcher.RunAsync(CoreDispatcherPriority.Normal, () => { RecordButton.IsEnabled = false; PhotoButton.IsEnabled = false; });
            }
        }

        /// <summary>
        /// photo lowlag capture
        /// </summary>
        /// <returns></returns>
        private async Task CaptureLowLagPhotoAsync()
        {
            try
            {
                var folder = await KnownFolders.PicturesLibrary.CreateFolderAsync("Cam360", CreationCollisionOption.OpenIfExists);
                var file = await folder.CreateFileAsync("PhotoWithRecord.jpg", CreationCollisionOption.GenerateUniqueName);
                var outputStream = await file.OpenAsync(FileAccessMode.ReadWrite);
                if (_lowLagPhotoCapture != null)
                {
                    CapturedPhoto photo = await _lowLagPhotoCapture.CaptureAsync();
                    var properties = photo.Frame.BitmapProperties;
                    var softwareBitmap = photo.Frame.SoftwareBitmap;
                    if (softwareBitmap != null)
                    {
                        var encoder = await BitmapEncoder.CreateAsync(BitmapEncoder.JpegEncoderId, outputStream);
                        if (properties != null)
                        {
                            await encoder.BitmapProperties.SetPropertiesAsync(properties);
                        }

                        encoder.SetSoftwareBitmap(softwareBitmap);
                        await encoder.FlushAsync();
                    }
                    else 
                    {
                        // This is a jpeg frame
                        var decoder = await BitmapDecoder.CreateAsync(photo.Frame.CloneStream());
                        var encoder = await BitmapEncoder.CreateForTranscodingAsync(outputStream, decoder);
                        await encoder.FlushAsync();
                    }
                }
            }
            catch (Exception ex)
            {
                string errorMessage = string.Format("Photo capture did not complete: {0}", ex.Message);
                Debug.Write(errorMessage);
                await Dispatcher.RunAsync(CoreDispatcherPriority.Normal, async () =>
                {
                    await new MessageDialog(errorMessage).ShowAsync();
                });
            }
        }
        /// <summary>
        /// photo capture
        /// </summary>
        /// <returns></returns>
        private async Task CapturePhotoAsync()
        {
            try
            {
                var folder = await KnownFolders.PicturesLibrary.CreateFolderAsync("Cam360", CreationCollisionOption.OpenIfExists);
                var file = await folder.CreateFileAsync("Photo.jpg", CreationCollisionOption.GenerateUniqueName);
                ImageEncodingProperties imageEncodingProperties = ImageEncodingProperties.CreateJpeg();
                await _mediaCapture.CapturePhotoToStorageFileAsync(imageEncodingProperties, file);
            }
            catch (Exception ex)
            {
                string errorMessage = string.Format("Photo capture did not complete: {0}", ex.Message);
                Debug.Write(errorMessage);
                await Dispatcher.RunAsync(CoreDispatcherPriority.Normal, async () =>
                {
                    await new MessageDialog(errorMessage).ShowAsync();
                });
            }
        }
        /// <summary>
        /// Start video recording
        /// </summary>
        /// <returns></returns>
        private async Task StartVideoRecordingAsync()
        {
            try
            {
                await Dispatcher.RunAsync(CoreDispatcherPriority.Normal, () => PhotoButton.IsEnabled = false);
                if (_mediaCapture.MediaCaptureSettings.ConcurrentRecordAndPhotoSupported)
                {
                    ImageEncodingProperties imageEncodingProperties = ImageEncodingProperties.CreateJpeg();
                    _lowLagPhotoCapture = await _mediaCapture.PrepareLowLagPhotoCaptureAsync(imageEncodingProperties);
                    await Dispatcher.RunAsync(CoreDispatcherPriority.Normal, () => PhotoButton.IsEnabled = true);
                }

                var mediaEncodingProfile = MediaEncodingProfile.CreateMp4(VideoEncodingQuality.Auto);
                var folder = await KnownFolders.PicturesLibrary.CreateFolderAsync("Cam360", CreationCollisionOption.OpenIfExists);
                var file = await folder.CreateFileAsync("test.mp4", CreationCollisionOption.GenerateUniqueName);

                await _mediaCapture.StartRecordToStorageFileAsync(mediaEncodingProfile, file);
            }
            catch (Exception ex)
            {
                string errorMessage = string.Format("StartVideoRecording did not complete: {0}", ex.Message);
                Debug.Write(errorMessage);
                await Dispatcher.RunAsync(CoreDispatcherPriority.Normal, async () =>
                {
                    await new MessageDialog(errorMessage).ShowAsync();
                });
            }
        }

        /// <summary>
        /// Stop video recording
        /// </summary>
        /// <returns></returns>
        private async Task StopVideoRecordingAsync()
        {
            try
            {
                await _mediaCapture.StopRecordAsync();
                await Dispatcher.RunAsync(CoreDispatcherPriority.Normal, () => PhotoButton.IsEnabled = false);
                if (_lowLagPhotoCapture != null)
                {
                    await _lowLagPhotoCapture.FinishAsync();
                    _lowLagPhotoCapture = null;
                }
                await Dispatcher.RunAsync(CoreDispatcherPriority.Normal, () => PhotoButton.IsEnabled = true);
            }
            catch (Exception ex)
            {
                string errorMessage = string.Format("StopVideoRecording did not complete: {0}", ex.Message);
                Debug.Write(errorMessage);
                await Dispatcher.RunAsync(CoreDispatcherPriority.Normal, async () =>
                {
                    await new MessageDialog(errorMessage).ShowAsync();
                });
            }
        }

        #endregion Camera methods

        /// <summary>
        /// Get standard aspect ratio given width and height
        /// </summary>
        /// <param name="width"></param>
        /// <param name="height"></param>
        /// <returns></returns>
        private string GetAspectRatio(uint width, uint height)
        {
            double value = width / (double)height;

            const double tolerance = 0.015;
            if (Math.Abs(1 - value / (16 / 9.0)) <= tolerance)
            {
                return "16:9";
            }
            if (Math.Abs(1 - value / (4 / 3.0)) <= tolerance)
            {
                return "4:3";
            }
            if (Math.Abs(1 - value / (5 / 3.0)) <= tolerance)
            {
                return "5:3";
            }
            if (Math.Abs(1 - value) <= tolerance)
            {
                return "1:1";
            }
            uint gcd = GCD(width, height);
            return string.Format("{0}:{1}", width / gcd, height / gcd);
        }

        /// <summary>
        /// Frind the greatest common denominator
        /// </summary>
        /// <param name="a"></param>
        /// <param name="b"></param>
        /// <returns></returns>
        private uint GCD(uint a, uint b)
        {
            return b == 0 ? a : GCD(b, a % b);
        }

        /// <summary>
        /// Converts angle from degree to radian
        /// </summary>
        /// <param name="degree"></param>
        /// <returns></returns>
        public double DegreetoRadians(double degree)
        {
            return degree * Math.PI / 180.0;
        }

        /// <summary>
        /// Creates a quaternion from 3 axis rotation
        /// </summary>
        /// <param name="heading"></param>
        /// <param name="pitch"></param>
        /// <param name="roll"></param>
        /// <returns></returns>
        public Quaternion CreateFromHeadingPitchRoll(double heading, double pitch, double roll)
        {
            Quaternion result;
            double headingPart = DegreetoRadians(heading) * 0.5;
            double sin1 = Math.Sin(headingPart);
            double cos1 = Math.Cos(headingPart);

            double pitchPart = DegreetoRadians(-pitch) * 0.5;
            double sin2 = Math.Sin(pitchPart);
            double cos2 = Math.Cos(pitchPart);

            double rollPart = DegreetoRadians(roll) * 0.5;
            double sin3 = Math.Sin(rollPart);
            double cos3 = Math.Cos(rollPart);

            result.W = (float)(cos1 * cos2 * cos3 - sin1 * sin2 * sin3);
            result.X = (float)(cos1 * cos2 * sin3 + sin1 * sin2 * cos3);
            result.Y = (float)(sin1 * cos2 * cos3 + cos1 * sin2 * sin3);
            result.Z = (float)(cos1 * sin2 * cos3 - sin1 * cos2 * sin3);

            return result;
        }
    }
}
