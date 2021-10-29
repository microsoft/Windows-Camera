// Copyright (C) Microsoft Corporation. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Windows.ApplicationModel;
using VirtualCameraManager_WinRT;
using Windows.Devices.Enumeration;
using Windows.Foundation;
using Windows.Foundation.Metadata;
using Windows.Media.Devices;
using Windows.UI.Core.Preview;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;
using Windows.Media.Capture;
using Windows.Media.MediaProperties;
using Windows.Media.Capture.Frames;
using Windows.UI.Xaml.Navigation;

namespace VirtualCameraManager_App
{
    /// <summary>
    /// Main page of the app.
    /// </summary>
    public sealed partial class MainPage : Page
    {
        internal static List<VirtualCameraControl> m_virtualCameraControls = new List<VirtualCameraControl>();
        private List<DeviceInformation> m_cameraDeviceList;

        public MainPage()
        {
            this.InitializeComponent();

            SystemNavigationManagerPreview mgr =
                SystemNavigationManagerPreview.GetForCurrentView();
            mgr.CloseRequested += SystemNavigationManager_CloseRequested;
        }

        private void SystemNavigationManager_CloseRequested(object sender, SystemNavigationCloseRequestedPreviewEventArgs e)
        {
            Deferral deferral = e.GetDeferral();

            var t = App.LaunchSysTrayAsync(); // fire-forget

            e.Handled = false;
            deferral.Complete();
        }

        protected override async void OnNavigatedTo(NavigationEventArgs e)
        {
            base.OnNavigatedTo(e);

            // look if we are able to leverage our startup task to exit the app by deffering down to the system tray process instead of completely closing
            var startup = await StartupTask.GetAsync("ContosoVCamStartupTaskId");
            switch (startup.State)
            {
                case StartupTaskState.Disabled:
                    await startup.RequestEnableAsync();
                    break;
                case StartupTaskState.DisabledByPolicy:
                    UIResultText.Text = "Startup task is disabled by policy";
                    break;
                case StartupTaskState.DisabledByUser:
                    UIResultText.Text = "Startup task is disabled by user, manually reenable to regain startup functionality";
                    break;
            }
            
            // populate default UI values
            UILifetimeSelector.ItemsSource = Enum.GetNames(typeof(VirtualCameraLifetime));
            UIAccessSelector.ItemsSource = Enum.GetNames(typeof(VirtualCameraAccess));
            UILifetimeSelector.SelectedIndex = 0;
            UIAccessSelector.SelectedIndex = 0;
            UIVirtualCameraList.ItemsSource = null;

            // refresh virtual cameras view
            UIRefreshList_Click(null, null);
        }

        protected override void OnNavigatedFrom(NavigationEventArgs e)
        {
            foreach(var vCamControl in m_virtualCameraControls)
            {
                vCamControl.DisposeOfVCam();
            }
            m_virtualCameraControls = new List<VirtualCameraControl>();
        }

        private void UICreateVirtualCamera_Click(object sender, RoutedEventArgs e)
        {
            UICameraWrappingCheckBox.IsChecked = false;
            UIFriendlyName.Text = "Contoso Virtual Camera";
            var fireAndForgetUITask = UIAddNewVirtualCameraDialog.ShowAsync(); // fire-and-forget
        }

        /// <summary>
        /// Register and start a new virtual camera
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="args"></param>
        private void UIAddNewVirtualCameraDialog_PrimaryButtonClick(ContentDialog sender, ContentDialogButtonClickEventArgs args)
        {
            UIResultText.Text = ""; // clear error text
            var friendlyText = UIFriendlyName.Text;
            var isCameraWrappingChecked = UICameraWrappingCheckBox.IsChecked == true;
            var isAugmentedVirtualCameraChecked = UIAugmentedCameraCheckBox.IsChecked == true;
            var selectedIndex = UICameraToWrapList.SelectedIndex;
            VirtualCameraLifetime selectedLifetime = (VirtualCameraLifetime)(UILifetimeSelector.SelectedIndex);
            VirtualCameraAccess selectedAccess = (VirtualCameraAccess)(UIAccessSelector.SelectedIndex);
            try
            {
                if (m_virtualCameraControls.Find(x => x.VirtualCameraProxyInst.FriendlyName == UIFriendlyName.Text) != null)
                {
                    throw new Exception("A virtual camera with this name already exists");
                }

                // Important: We start a task here so as to not block the UI thread if the app request permission to access the Camera
                // (permission prompt runs on the UI thread)
                var fireAndForgetBackgroundTask = Task.Run( () => // fire-and-forget
                {
                    try
                    {
                        VirtualCameraProxy vcam = null;

                        if (isCameraWrappingChecked == true && selectedIndex >= 0)
                        {
                            VirtualCameraKind wrapperCameraKindSelected = isAugmentedVirtualCameraChecked ? VirtualCameraKind.AugmentedCameraWrapper : VirtualCameraKind.CameraWrapper;
                            var wrappedCamera = m_cameraDeviceList[selectedIndex];
                            vcam = VirtualCameraRegistrar.RegisterNewVirtualCamera(
                               wrapperCameraKindSelected,
                               selectedLifetime,
                               selectedAccess,
                               friendlyText,
                               wrappedCamera.Id);
                        }
                        else
                        {
                            vcam = VirtualCameraRegistrar.RegisterNewVirtualCamera(
                               VirtualCameraKind.Synthetic,
                               selectedLifetime,
                               selectedAccess,
                               friendlyText,
                               "");
                        }
                        vcam.EnableVirtualCamera();

                        var fireAndForgetUITask = Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, async () =>
                        {
                            var vCamControl = new VirtualCameraControl(vcam);
                            vCamControl.VirtualCameraControlFailed += VirtualCameraControlFailed;
                            vCamControl.VirtualCameraControlRemoved += VirtualCameraControlRemoved;

                            m_virtualCameraControls.Add(vCamControl);
                            App.m_virtualCameraInfoList.Add(new App.VCamInfo() 
                            { 
                                FriendlyName = vCamControl.VirtualCameraProxyInst.FriendlyName,
                                VCamType = (int)vCamControl.VirtualCameraProxyInst.VirtualCameraKind,
                                SymLink = vCamControl.VirtualCameraProxyInst.SymbolicLink,
                                WrappedCameraSymLink = vCamControl.VirtualCameraProxyInst.WrappedCameraSymbolicLink
                            });

                            // force user to re-check that box triggering an update to available cameras
                            UICameraWrappingCheckBox.IsChecked = false;

                            try
                            {
                                await vCamControl.InitializeAsync();
                            }
                            catch (Exception ex)
                            {
                                UIResultText.Text = $"{ex.HResult} : {ex.Message}";
                            }

                            // refresh list view
                            UIVirtualCameraList.ItemsSource = null;
                            UIVirtualCameraList.ItemsSource = m_virtualCameraControls;
                        });
                    }
                    catch (Exception ex)
                    {
                        var fireAndForgetUITask = Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
                        {
                            UIResultText.Text = $"{ex.HResult} : {ex.Message}";
                        });
                    }
                });
            }
            catch (Exception ex)
            {
                UIResultText.Text = $"{ex.HResult} : {ex.Message}";
            }
        }

        /// <summary>
        /// Refresh the view of existing virtual cameras. 
        /// If a virtual camera has been registered by this app, its information should be populated accordingly. 
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void UIRefreshList_Click(object sender, RoutedEventArgs e)
        {
            UIResultText.Text = ""; // clear error text

            // Important: We start a task here so as to not block the UI thread if the app request permission to access the Camera
            // (permission prompt runs on the UI thread)
            var fireAndForgetBackgroundTask = Task.Run(() =>
            {
                // find existing virtual camera on the system
                var vCameraInformationList = VirtualCameraRegistrar.GetExistingVirtualCameraDevices();

                // if there are no current vcam, clean slate (error handling from bad closure in a past session?)
                if (vCameraInformationList.Count == 0)
                {
                    App.m_virtualCameraInfoList.Clear();
                }

                var fireAndForgetUITask = Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, async () =>
                {
                    // for each virtual camera found on the system.. 
                    foreach (var vCamInfo in vCameraInformationList)
                    {
                        // if we have not already added this camera to the UI (i.e. when the app starts)..
                        if (m_virtualCameraControls.Find(x => x.VirtualCameraProxyInst.SymbolicLink == vCamInfo.Id) == null)
                        {
                            // Check if we have created this Virtual camera at some point in a past session with this app
                            // and gain back control of it if so by registering it with the same parameters once again
                            var relatedVCamInfo = App.m_virtualCameraInfoList.Find(x => x.SymLink == vCamInfo.Id);
                            if (relatedVCamInfo != null)
                            {
                                var virtualCamera = VirtualCameraRegistrar.RegisterNewVirtualCamera(
                                  (VirtualCameraKind)relatedVCamInfo.VCamType,
                                  VirtualCameraLifetime.System, // only possible option if retrieving from this app upon relaunch
                                  VirtualCameraAccess.CurrentUser, // only possible option with a UWP app (non-admin)
                                  relatedVCamInfo.FriendlyName,
                                  relatedVCamInfo.WrappedCameraSymLink);

                                virtualCamera.EnableVirtualCamera();

                                var vCamControl = new VirtualCameraControl(virtualCamera);
                                vCamControl.VirtualCameraControlFailed += VirtualCameraControlFailed;
                                vCamControl.VirtualCameraControlRemoved += VirtualCameraControlRemoved;
                                m_virtualCameraControls.Add(vCamControl);

                                try
                                {
                                    await vCamControl.InitializeAsync();

                                }
                                catch (Exception ex)
                                {
                                    UIResultText.Text = $"{ex.HResult} : {ex.Message}";
                                }
                            }
                        }
                    }

                    // refresh list view
                    UIVirtualCameraList.ItemsSource = null;
                    UIVirtualCameraList.ItemsSource = m_virtualCameraControls;
                });
            });
        }

        private void VirtualCameraControlRemoved(string symLink)
        {
            // refresh list view

            var controlToRemove = m_virtualCameraControls.Find(x => x.VirtualCameraProxyInst.SymbolicLink == symLink);
            if (controlToRemove != null)
            {
                m_virtualCameraControls.RemoveAll(x => x.VirtualCameraProxyInst.SymbolicLink == symLink);
            }

            UIVirtualCameraList.ItemsSource = null;
            UIMainPanel.Children.Remove(UIVirtualCameraList);
            UIVirtualCameraList = new ListBox();
            UIMainPanel.Children.Add(UIVirtualCameraList);
            UIVirtualCameraList.ItemsSource = m_virtualCameraControls;
        }

        private void VirtualCameraControlFailed(Exception ex)
        {
            UIResultText.Text = $"{ex.HResult} : {ex.Message}";

            UIRefreshList_Click(null, null);
        }

        private async void UICameraWrappingCheckBox_Checked(object sender, RoutedEventArgs e)
        {
            // if the checkbox is checked, show a list of available camera devices to wrap
            if (UICameraWrappingCheckBox.IsChecked == true)
            {
                // disable UI momentarily
                UICameraWrappingCheckBox.IsEnabled = false;
                UIAugmentedCameraCheckBox.IsEnabled = false;
                UICameraToWrapList.ItemsSource = null;
                UIAddNewVirtualCameraDialog.IsPrimaryButtonEnabled = false;


                var cameraDeviceList = await DeviceInformation.FindAllAsync(MediaDevice.GetVideoCaptureSelector());

                var fireAndForgetBackgroundTask = Task.Run(() =>
                {
                    // find existing virtual camera on the system
                    var vCameraInformationList = VirtualCameraRegistrar.GetExistingVirtualCameraDevices().ToList();
                    var fireAndForgetUITask = Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
                    {
                        // expose a list of available cameras that excludes any known virtual cameras or cameras currently wrapped by a virtual camera
                        m_cameraDeviceList = cameraDeviceList.Where(x => vCameraInformationList.Find(info => info.Id == x.Id) == null)
                                                                   .Where(x => m_virtualCameraControls.Find(vcam => vcam.VirtualCameraProxyInst.WrappedCameraSymbolicLink == x.Id) == null).ToList();

                        // re-enable and refresh UI
                        UICameraToWrapList.ItemsSource = m_cameraDeviceList.Select(x => x.Name);
                        UICameraToWrapList.SelectedIndex = 0;
                        UICameraToWrapList.IsEnabled = true;
                        UICameraToWrapList.Visibility = Visibility.Visible;

                        UICameraWrappingCheckBox.IsEnabled = true;
                        UIAugmentedCameraCheckBox.IsEnabled = true;
                        UIAugmentedCameraCheckBox.Visibility = Visibility.Visible;

                        UIAddNewVirtualCameraDialog.IsPrimaryButtonEnabled = true;
                    });
                });
            }
            else
            {
                // disable camera wrapping UI
                UIAugmentedCameraCheckBox.IsChecked = false;
                UIAugmentedCameraCheckBox.IsEnabled = false;
                UIAugmentedCameraCheckBox.Visibility = Visibility.Collapsed;
                UICameraToWrapList.IsEnabled = false;
                UICameraToWrapList.Visibility = Visibility.Collapsed;

                UIAddNewVirtualCameraDialog.IsPrimaryButtonEnabled = true;
            }
        }

        private async void UIAugmentedCameraCheckbox_Checked(object sender, RoutedEventArgs e)
        {
            if (UIAugmentedCameraCheckBox.IsChecked == true)
            {
                // disable UI momentarily
                UICameraWrappingCheckBox.IsEnabled = false;
                UIAugmentedCameraCheckBox.IsEnabled = false;
                UICameraToWrapList.ItemsSource = null;
                UIAddNewVirtualCameraDialog.IsPrimaryButtonEnabled = false;

                // Find all non-vcam color video source camera devices
                var frameSourceGroups = await MediaFrameSourceGroup.FindAllAsync();
                var colorPreviewOrVideoGroups = frameSourceGroups.Where(group => group.SourceInfos.Any(sourceInfo => sourceInfo.SourceKind == MediaFrameSourceKind.Color
                                                                                                           && (sourceInfo.MediaStreamType == MediaStreamType.VideoPreview
                                                                                                               || sourceInfo.MediaStreamType == MediaStreamType.VideoRecord))
                                                                                                           && m_cameraDeviceList.Any(x => x.Id == group.Id))
                                                                       .GroupBy(x => x.DisplayName).Select(y => y.First()).ToList();

                // if there are no non-vcam camera devices with a color video source present..
                if (colorPreviewOrVideoGroups.Count == 0)
                {
                    UIFriendlyName.Text = "No valid camera found";
                }

                // expose a list of available conformant cameras that excludes virtual cameras, that have at least
                // 1) a video or preview pin
                // 2) exposing a NV12 stream
                // 3) that is above 320p and below 1080p (any aspect ratio is ok)
                // 4) at framerate between 15fps and 30fps inclusively
                bool isValidSourceFound = false;
                var cameraDeviceList = new List<DeviceInformation>();
                try
                {
                    foreach (var group in colorPreviewOrVideoGroups)
                    {
                        MediaCapture mc = new MediaCapture();
                        var settings = new MediaCaptureInitializationSettings
                        {
                            SharingMode = MediaCaptureSharingMode.SharedReadOnly,
                            SourceGroup = group,
                            MemoryPreference = MediaCaptureMemoryPreference.Cpu,
                            StreamingCaptureMode = StreamingCaptureMode.Video
                        };
                        await mc.InitializeAsync(settings);
                        var frameSources = mc.FrameSources.Where(x => x.Value.Info.SourceKind == MediaFrameSourceKind.Color
                                                     && (x.Value.Info.MediaStreamType == MediaStreamType.VideoPreview
                                                        || x.Value.Info.MediaStreamType == MediaStreamType.VideoRecord)).ToDictionary(x => x.Key, x => x.Value);

                        foreach (var frameSource in frameSources)
                        {
                            var frameSourceFormats = frameSource.Value.SupportedFormats.Where(format =>
                                                                             string.Compare(format.Subtype, MediaEncodingSubtypes.Nv12, true) == 0
                                                                             && ((int)((float)format.FrameRate.Numerator / format.FrameRate.Denominator) >= 15)
                                                                             && ((int)((float)format.FrameRate.Numerator / format.FrameRate.Denominator) <= 30)
                                                                             && format.VideoFormat.Width <= 1920 && format.VideoFormat.Width >= 320
                                                                             && format.VideoFormat.Height <= 1080 && format.VideoFormat.Height >= 180);
                            mc.Dispose();

                            if (frameSourceFormats.Count() > 0)
                            {
                                var matchingDevice = m_cameraDeviceList.Find(x => x.Id == group.Id);
                                if (matchingDevice != null)
                                {
                                    cameraDeviceList.Add(matchingDevice);
                                    break;
                                }
                            }
                        }
                    }

                    isValidSourceFound = (cameraDeviceList != null && cameraDeviceList.Any());

                }
                catch (Exception ex)
                {
                    isValidSourceFound = false;
                    m_cameraDeviceList = null;
                }

                // refresh UI (re-enable or keep disabled)
                if (isValidSourceFound)
                {
                    m_cameraDeviceList = cameraDeviceList;
                    UICameraToWrapList.ItemsSource = m_cameraDeviceList.Select(x => x.Name);
                    UICameraToWrapList.SelectedIndex = 0;
                    UIAddNewVirtualCameraDialog.IsPrimaryButtonEnabled = true;
                }
                else
                {
                    UIFriendlyName.Text = "No valid camera found";
                }
                
                UICameraWrappingCheckBox.IsEnabled = true;
                UIAugmentedCameraCheckBox.IsEnabled = true;
            }
            else
            {
                UICameraWrappingCheckBox_Checked(null, null);
            }
        }

        private void UICameraToWrapList_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (UICameraToWrapList.SelectedItem != null)
            {
                UIFriendlyName.Text = UICameraToWrapList.SelectedItem.ToString() + " + Contoso";
            }
        }
    }
}
