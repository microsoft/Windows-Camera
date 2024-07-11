using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Controls.Primitives;
using Microsoft.UI.Xaml.Data;
using Microsoft.UI.Xaml.Input;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Navigation;
using Windows.UI;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices.WindowsRuntime;
using Windows.Foundation;
using Windows.Foundation.Collections;
using Windows.Media.Capture;
using System.Threading;
using System.Threading.Tasks;
using Windows.Media.Capture.Frames;
using Windows.Media.Core;
using Windows.Media.Playback;
using Windows.Devices.Enumeration;
using Windows.Media.Devices;
using System.Diagnostics;
using System.Collections.ObjectModel;
using Windows.UI.Core;
using Microsoft.UI.Text;
using Microsoft.UI;
using Windows.Media.MediaProperties;

// To learn more about WinUI, the WinUI project structure,
// and more about our project templates, see: http://aka.ms/winui-project-info.

namespace MediaCaptureWinUI3
{
    /// <summary>
    /// An empty window that can be used on its own or navigated to within a Frame.
    /// </summary>
    public sealed partial class MainWindow : Window
    {
        private MediaCapture m_MediaCapture;
        private MediaPlayer m_MediaPlayer;
        private DeviceInformationCollection m_deviceList;
        private List<MediaFrameSource> m_previewSourceList;
        private MediaFrameSource m_currentPreviewSource;
        private IReadOnlyList<MediaCaptureVideoProfile> m_videoProfileList;
        private List<MediaFrameFormat> m_mediaFrameFormatList;

        private ObservableCollection<string> captureModeList = new ObservableCollection<string> { "", "VideoOnly", "AudioVideo" };
        private ObservableCollection<string> memoryPreferenceList = new ObservableCollection<string> { "", "auto", "cpu" };

        private enum LogMessageType
        {
            Success,
            Error,
            Warning,
            Message
        }

        public MainWindow()
        {
            this.InitializeComponent();

            PopulateCameraList();
            cbMemoryPreferenceList.Items.Clear();
            cbMemoryPreferenceList.ItemsSource = memoryPreferenceList;

            cbCaptureMode.Items.Clear();
            cbCaptureMode.ItemsSource = captureModeList;

            Log("Hello World!", LogMessageType.Message);
        }

        private async void btnStartDevice_Click(object sender, RoutedEventArgs e)
        {
            // Initialize mediaCapture with selected device
            if (m_MediaCapture == null)
            {
                Log("Initialize Media Capture ...", LogMessageType.Message);

                m_MediaCapture = new MediaCapture();
                m_MediaCapture.Failed += MediaCapture_Failed;

                    bool status = await InitializeMediaCapture();
                if (status) 
                {
                    UpdateMediaCaptureSettingsText(m_MediaCapture.MediaCaptureSettings);

                    PopulatePreviewSourceList();
                    cbPreviewSourceList.IsEnabled = true;

                    btnStartDevice.IsEnabled = false;
                    lvInitSettings.IsEnabled = false;
                    btnReset.IsEnabled = true;
                    btnPreview.IsEnabled = true;

                    Log("Initialize Media Capture succeeded!", LogMessageType.Success);
                }
                else
                {
                    m_MediaCapture = null;
                }
            }
            else
            {
                Log("Media capture is initialized already!", LogMessageType.Warning);
            }
        }

       

        private void btnPreview_Click(object sender, RoutedEventArgs e)
        {
            if(btnPreview.IsChecked == false)
            {
                /// stop Preview
                btnPreview.Content = "Start Preview";
                StopPreview();
            }
            else
            {
                if(StartPreview())
                {
                    btnPreview.Content = "Stop Preview";
                }
                else
                {
                    btnPreview.IsChecked = false;
                }
            }
        }

        private void btnReset_Click(object sender, RoutedEventArgs e)
        {
            if (m_MediaCapture!= null)
            {
                m_MediaCapture.Dispose();
                m_MediaCapture = null;
            }

            btnPreview.IsChecked = false;
            btnPreview.Content = "Start Preview";
            btnPreview.IsEnabled = false;

            btnReset.IsEnabled = false;
            btnStartDevice.IsEnabled = true;
            lvInitSettings.IsEnabled = true;
            cbDeviceList.SelectedIndex = -1;
            UpdateMediaCaptureSettingsText(null);
            PopulateCameraList();

            cbPreviewSourceList.Items.Clear();
            UpdateCurrentSourceText(null);

            cbMediaTypeList.Items.Clear();
            UpdateCurrentMediaTypeText(null);
        }

        private void MediaCapture_Failed(MediaCapture sender, MediaCaptureFailedEventArgs errorEventArgs)
        {
            Log("MediaPlayer_Failed" + errorEventArgs.Message + " " + errorEventArgs.Code, LogMessageType.Error);
        }

        private async Task<bool> InitializeMediaCapture()
        {
            try
            {
                MediaCaptureInitializationSettings settings = new MediaCaptureInitializationSettings();

                int deviceIdx = cbDeviceList.SelectedIndex;
                if (m_deviceList == null || deviceIdx < 0)
                {
                    Log("Select device before start!", LogMessageType.Error);
                    return false;
                }
                settings.VideoDeviceId = m_deviceList[deviceIdx].Id;

                // configure Memory preference base in UI selection.
                // by default media capture will use memory that preference by the camera driver.
                // If application needs to process frame in CPU, it can use this setting to configure the pipeline to output frames in CPU memory.
                int memoryPreferenceIdx = cbMemoryPreferenceList.SelectedIndex;

                if (memoryPreferenceIdx == 1)
                {
                    settings.MemoryPreference = MediaCaptureMemoryPreference.Auto;
                }
                else if (memoryPreferenceIdx == 2)
                {
                    settings.MemoryPreference = MediaCaptureMemoryPreference.Cpu;
                }

                // Configure video Profile base on UI selection.
                int profileIdx = cbProfileList.SelectedIndex;
                if (m_videoProfileList != null && profileIdx > 0)
                {
                    // Display list index is offset by 1 with index 0 for no profile selection.
                    settings.VideoProfile = m_videoProfileList[profileIdx - 1];
                }

                // Configure capture mode base on UI selection
                // If your application doesn't need to use both audio and video device at the same time,
                // this setting can help restrict media capture session to just video only or audio only
                int captureModeIdx = cbCaptureMode.SelectedIndex;
                if (captureModeIdx == 1)
                {
                    settings.StreamingCaptureMode = StreamingCaptureMode.Video;
                }
                else if (captureModeIdx == 2)
                {
                    settings.StreamingCaptureMode = StreamingCaptureMode.AudioAndVideo;
                }

                // Initialize media capture with settings
                await m_MediaCapture.InitializeAsync(settings);
                return true;
            }
            catch (Exception ex)
            {
                Log("Initialize media capture failed with error: " + ex.Message, LogMessageType.Error);
            }
            return false;
        }

        private async void SetMediaType()
        {
            var idx = cbMediaTypeList.SelectedIndex;
            if (m_currentPreviewSource == null || m_mediaFrameFormatList == null || idx < 0)
            {
                return;
            }

            await m_currentPreviewSource.SetFormatAsync(m_mediaFrameFormatList[idx]);
            UpdateCurrentMediaTypeText(m_currentPreviewSource.CurrentFormat);
        }

        private bool StartPreview()
        {
            Log("Start Preview ...", LogMessageType.Message);
            if(m_MediaCapture == null)
            {
                Log("Media capture is not initialized!", LogMessageType.Error);
                return false;
            }
            var idx = cbPreviewSourceList.SelectedIndex;
            if(m_previewSourceList == null || idx < 0)
            {
                Log("Select preview source before start preview!", LogMessageType.Error);
                return false;
            }

            m_currentPreviewSource = m_previewSourceList[idx];
            SetMediaType();

            // Create MediaPlayer with the preview source
            m_MediaPlayer = new MediaPlayer();
            m_MediaPlayer.RealTimePlayback = true;
            m_MediaPlayer.AutoPlay = false;
            m_MediaPlayer.Source = MediaSource.CreateFromMediaFrameSource(m_currentPreviewSource);
            m_MediaPlayer.MediaFailed += MediaPlayer_MediaFailed;

            // Set the mediaPlayer on the MediaPlayerElement
            myPreview.SetMediaPlayer(m_MediaPlayer);

            // Start preview
            m_MediaPlayer.Play();

            UpdateCurrentSourceText(m_currentPreviewSource);
            cbPreviewSourceList.IsEnabled = false;

            Log("Start preview succeeded!", LogMessageType.Success);
            return true;
        }

        private void StopPreview()
        {
            Log("Stop Preview ...", LogMessageType.Message);
            m_MediaPlayer.Pause();
            cbPreviewSourceList.IsEnabled = true;
            Log("Stop Preview succeeded!", LogMessageType.Success);
        }

        private void MediaPlayer_MediaFailed(MediaPlayer sender, MediaPlayerFailedEventArgs args)
        {
            Log("MediaPlayer_MediaFailed" + args.Error.ToString() + " " + args.ErrorMessage, LogMessageType.Error);
        }

        private void cbDeviceList_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            var idx = cbDeviceList.SelectedIndex;
            if (m_deviceList == null || idx < 0)
            {
                return;
            }
            var device = m_deviceList[idx];
            PopulateVideoProfileList(device.Id);
        }

        private void cbPreviewSourceList_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            var idx = cbPreviewSourceList.SelectedIndex;
            if (m_previewSourceList == null  || idx < 0 )
            {
                return;
            }
            var frameSource = m_previewSourceList[idx];
            PopulateMediaTypeList(frameSource);
        }


        private async void PopulateCameraList()
        {
            cbDeviceList.Items.Clear();

            m_deviceList = await DeviceInformation.FindAllAsync(MediaDevice.GetVideoCaptureSelector());
            foreach (var device in m_deviceList)
            {
                cbDeviceList.Items.Add(device.Name);
            }
        }

        private void PopulateVideoProfileList(string deviceId)
        {
            m_videoProfileList = MediaCapture.FindAllVideoProfiles(deviceId);
            cbProfileList.Items.Clear();

            if (m_videoProfileList == null || m_videoProfileList.Count == 0)
            {
                cbProfileList.IsEnabled = false;
                return;
            }

            cbProfileList.Items.Add("");
            foreach (var profile in m_videoProfileList)
            {
                cbProfileList.Items.Add(profile.Id);
            }
            cbProfileList.IsEnabled = true;
        }

        private void PopulatePreviewSourceList()
        {
            if (m_MediaCapture == null)
            {
                return;
            }

            m_previewSourceList = new List<MediaFrameSource>();
            cbPreviewSourceList.Items.Clear();

            // Find preview source.
            // The preferred preview stream from a camera is defined by MediaStreamType.VideoPreview on the RGB camera (SourceKind == color).
            MediaFrameSource previewSource = m_MediaCapture.FrameSources.FirstOrDefault(source => source.Value.Info.MediaStreamType == MediaStreamType.VideoPreview
                                                                                        && source.Value.Info.SourceKind == MediaFrameSourceKind.Color).Value;

            if(previewSource != null)
            {
                m_previewSourceList.Add(previewSource);
                cbPreviewSourceList.Items.Add(previewSource.Info.Id);
            }

            // Some camera might not have a dedicate preview stream, so the fallback is the record stream (MediaStreamType.VideoRecord)
            MediaFrameSource recordSource = m_MediaCapture.FrameSources.FirstOrDefault(source => source.Value.Info.MediaStreamType == MediaStreamType.VideoRecord
                                                                                           && source.Value.Info.SourceKind == MediaFrameSourceKind.Color).Value;

            if(recordSource != null)
            {
                m_previewSourceList.Add(recordSource);
                cbPreviewSourceList.Items.Add(recordSource.Info.Id);
            }

            // if neither stream is present, this camera is doesn't have a renderable stream.
            if (m_previewSourceList.Count == 0)
            {
                m_previewSourceList = null;
                throw (new Exception("could not find a VideoPreview or VideoRecord stream on the selected camera"));
            }
        }

        private void PopulateMediaTypeList(MediaFrameSource frameSource)
        {
            var formatList = frameSource.SupportedFormats;
            m_mediaFrameFormatList = new List<MediaFrameFormat>();
            cbMediaTypeList.Items.Clear();

            foreach (var format in formatList)
            {
                if(IsSupportedPreviewFormat(format))
                {
                    cbMediaTypeList.Items.Add(MediaFrameFormatToString(format));
                    m_mediaFrameFormatList.Add(format);
                }
            }

            UpdateCurrentMediaTypeText(frameSource.CurrentFormat);
        }

        private bool IsSupportedPreviewFormat(MediaFrameFormat format)
        {
            if(format == null)
            {
                return false;
            }

            // Know camera device support subtype that is also support for media player for rendering.
            if(String.Compare(format.Subtype, MediaEncodingSubtypes.Nv12, StringComparison.OrdinalIgnoreCase) == 0 || 
               String.Compare(format.Subtype, MediaEncodingSubtypes.Yuy2, StringComparison.OrdinalIgnoreCase) == 0 || 
               String.Compare(format.Subtype, MediaEncodingSubtypes.Rgb24, StringComparison.OrdinalIgnoreCase) == 0 ||
               String.Compare(format.Subtype, MediaEncodingSubtypes.Rgb32, StringComparison.OrdinalIgnoreCase) == 0)
            {
                return true;
            }
            return false;
        }

        //private async void MessageLoggedEvent(string message, Logger.LogMessageType logMessageType)
        private void Log(string message, LogMessageType logMessageType)
        {
            this.DispatcherQueue.TryEnqueue(() =>
            {
                FrameworkElement item;

                TextBlock txtblockItem = new TextBlock()
                {
                    Text = message,
                    FontWeight = FontWeights.Bold,
                    TextWrapping = TextWrapping.Wrap,
                    TextLineBounds = TextLineBounds.TrimToBaseline,
                };
                switch (logMessageType)
                {
                    case LogMessageType.Success:
                        txtblockItem.Foreground = new SolidColorBrush(Colors.GreenYellow);
                        break;
                    case LogMessageType.Error:
                        txtblockItem.Foreground = new SolidColorBrush(Colors.Red);
                        break;
                    case LogMessageType.Warning:
                        txtblockItem.Foreground = new SolidColorBrush(Colors.Yellow);
                        break;
                    case LogMessageType.Message:
                    default:
                        txtblockItem.Foreground = new SolidColorBrush(Colors.White);
                        break;

                }
                item = txtblockItem;

                if (lvLog.Items != null)
                {
                    lvLog.Items.Add(item);
                    lvLog.ScrollIntoView(item, ScrollIntoViewAlignment.Leading);
                }
            });
        }

        private void UpdateMediaCaptureSettingsText(MediaCaptureSettings settings)
        {
            var str = "";

            if (settings != null)
            {
                str += "Video Device Id: " + ((settings.VideoDeviceId == null) ? "\n" : settings.VideoDeviceId + "\n");
                str += "Streaming Capture mode: " + settings.StreamingCaptureMode.ToString() + "\n";
            }

            tbCurrentSettings.Text = str;
        }

        private void UpdateCurrentSourceText(MediaFrameSource source)
        {
            tbCurrentSource.Text = (source == null) ? "" : ("Current Source: " + source.Info.Id);
        }

        private void UpdateCurrentMediaTypeText(MediaFrameFormat format)
        {
            String str = "";
            if(format != null)
            {
                str += "Current Format: \n" + MediaFrameFormatToString(format);
            }
            tbMediaType.Text = str;
        }

        private string MediaFrameFormatToString(MediaFrameFormat format)
        {
            if (format == null)
            {
                return "";
            }

            string strFormat = format.MajorType + "; " + format.Subtype;
            strFormat += "; " + format.VideoFormat.Width + "x" + format.VideoFormat.Height;
            strFormat += "; " + format.FrameRate.Numerator + "/" + format.FrameRate.Denominator;

            return strFormat;
        }
    }
}
