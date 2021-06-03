// Copyright (C) Microsoft Corporation. All rights reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using VirtualCameraManager_WinRT;
using Windows.Devices.Enumeration;
using Windows.Media.Devices;
using Windows.UI.Xaml;
using Windows.UI.Xaml.Controls;

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
        }

        private void Page_Loaded(object sender, RoutedEventArgs e)
        {
            // populate default UI values
            UILifetimeSelector.ItemsSource = Enum.GetNames(typeof(VirtualCameraLifetime));
            UIAccessSelector.ItemsSource = Enum.GetNames(typeof(VirtualCameraAccess));
            UILifetimeSelector.SelectedIndex = 0;
            UIAccessSelector.SelectedIndex = 0;
            UIVirtualCameraList.ItemsSource = null;

            // refresh virtual cameras view
            UIRefreshList_Click(null, null);
        }

        private void UICreateVirtualCamera_Click(object sender, RoutedEventArgs e)
        {
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
                            var wrappedCamera = m_cameraDeviceList[selectedIndex];
                            vcam = VirtualCameraRegistrar.RegisterNewVirtualCamera(
                               VirtualCameraKind.CameraWrapper,
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
            m_virtualCameraControls.RemoveAll(x => x.VirtualCameraProxyInst.SymbolicLink == symLink);

            // refresh list view
            UIVirtualCameraList.ItemsSource = null;
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
                UICameraToWrapList.ItemsSource = null;
                UICameraToWrapList.IsEnabled = true;
                UICameraToWrapList.Visibility = Visibility.Visible;

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
                                                                   
                        UICameraToWrapList.ItemsSource = m_cameraDeviceList.Select(x => x.Name);
                    });
                });
            }
            else
            {
                UICameraToWrapList.IsEnabled = false;
                UICameraToWrapList.Visibility = Visibility.Collapsed;
            }
        }
    }
}
