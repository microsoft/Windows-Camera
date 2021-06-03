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
            UILifeTimeSelector.ItemsSource = Enum.GetNames(typeof(VirtualCameraLifetime));
            UIAccessSelector.ItemsSource = Enum.GetNames(typeof(VirtualCameraAccess));
            UILifeTimeSelector.SelectedIndex = 0;
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
            var isHookCameraChecked = UIHookCameraCheckBox.IsChecked == true;
            var selectedIndex = UICameraHookList.SelectedIndex;
            VirtualCameraLifetime selectedLifetime = (VirtualCameraLifetime)(UILifeTimeSelector.SelectedIndex);
            VirtualCameraAccess selectedAccess = (VirtualCameraAccess)(UIAccessSelector.SelectedIndex);
            try
            {
                if (m_virtualCameraControls.Find(x => x.VirtualCameraProxyInst.FriendlyName == UIFriendlyName.Text) != null)
                {
                    throw new Exception("A virtual camera with this name already exists");
                }

                // Important: We spark a task here so as to not block the UI thread if the app request permission to access the Camera
                // (permission prompt runs on the UI thread)
                var fireAndForgetBackgroundTask = Task.Run( () => // fire-and-forget
                {
                    try
                    {
                        VirtualCameraProxy vcam = null;

                        if (isHookCameraChecked == true && selectedIndex >= 0)
                        {
                            var hookedCamera = m_cameraDeviceList[selectedIndex];
                            vcam = VirtualCameraRegistrar.RegisterNewVirtualCamera(
                               VirtualCameraKind.HookingARealCamera,
                               selectedLifetime,
                               selectedAccess,
                               friendlyText,
                               hookedCamera.Id);
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
                            UIHookCameraCheckBox.IsChecked = false;

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

            // Important: We spark a task here so as to not block the UI thread if the app request permission to access the Camera
            // (permission prompt runs on the UI thread)
            var fireAndForgetBackgroundTask = Task.Run(() =>
            {
               // find existing virtual camera on the system
               var vCameraInformationList = VirtualCameraRegistrar.GetExistingVirtualCameraDevices();

               var fireAndForgetUITask = Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal,
                   async () =>
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
                                       relatedVCamInfo.HookedCameraSymLink);

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

        private async void UIHookCameraCheckBox_Checked(object sender, RoutedEventArgs e)
        {
            // if the checkbox is checked, show a list of available camera devices to hook into
            if (UIHookCameraCheckBox.IsChecked == true)
            {
                UICameraHookList.ItemsSource = null;
                UICameraHookList.IsEnabled = true;
                UICameraHookList.Visibility = Visibility.Visible;

                var cameraDeviceList = await DeviceInformation.FindAllAsync(MediaDevice.GetVideoCaptureSelector());

                var fireAndForgetBackgroundTask = Task.Run(() =>
                {
                    // find existing virtual camera on the system
                    var vCameraInformationList = VirtualCameraRegistrar.GetExistingVirtualCameraDevices().ToList();
                    var fireAndForgetUITask = Dispatcher.RunAsync(Windows.UI.Core.CoreDispatcherPriority.Normal, () =>
                    {
                        // expose a list of available cameras that excludes any known virtual cameras or cameras currently hooked by a virtual camera
                        m_cameraDeviceList = cameraDeviceList.Where(x => vCameraInformationList.Find(info => info.Id == x.Id) == null)
                                                                   .Where(x => m_virtualCameraControls.Find(vcam => vcam.VirtualCameraProxyInst.HookedCameraSymbolicLink == x.Id) == null).ToList();
                                                                   
                        UICameraHookList.ItemsSource = m_cameraDeviceList.Select(x => x.Name);
                    });
                });
            }
            else
            {
                UICameraHookList.IsEnabled = false;
                UICameraHookList.Visibility = Visibility.Collapsed;
            }
        }
    }
}
