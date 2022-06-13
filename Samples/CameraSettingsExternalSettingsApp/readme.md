# ExternalCameraSettingsApp Sample

This sample app will demonstrate how to build an external camera settings app.

This app will use camera symbolic link(symlink) name that's provided via [Application.OnLaunched](https://docs.microsoft.com/uwp/api/windows.ui.xaml.application.onlaunched?view=winrt-22000) arguments and typically passed further on to the [Page.OnNavigatedTo](https://docs.microsoft.com/uwp/api/windows.ui.xaml.controls.page.onnavigatedto?view=winrt-22000). The camera symlink is only passed to the app launch when the app is registered to be an additional Settings app for the specific camera.

## Registering the app with Camera Settings
As external camera settings apps are typically companion apps for the specific camera, this registering process is normally done by driver inf or MSOS descriptor in FW that will add the registry entry.

The application's package family name (PFN) need to be added to the device interface node in the registry: `HKLM\SYSTEM\CurrentControlSet\Control\DeviceClasses\{e5323777-f976-4f5b-9b55-b94699c46e44}\<camera symlink>\#GLOBAL\Device Parameters`
| Name | Type | Data |
|------|------|------|
| SCSVCamPfn | REG_SZ| \<PFN\> |

To identify PFN for your app, run Get-AppxPackage from PowerShell
e.g., `Get-AppxPackage -Name *ExternalCamera*`

Adding this information will create a button to launch the application from the camera configuration page in the Settings (Settings -> Bluetooth & devices -> Cameras -> \<camera name\> ). If the app is not installed it will try to search it from the Store.

**NOTE:** Disable/enable of the camera and restart of the machine might be needed that the new app association is properly configured.

## Sample functionality
 The app will initialize the given camera, start preview, and show following controls (if camera implements them) that will set current value and save it as the default value by using IMFCameraConfigurationManager functionality.
  - Contrast control slider
  - Brightness control slider
  - Background segmentation toggle for Blur on/off