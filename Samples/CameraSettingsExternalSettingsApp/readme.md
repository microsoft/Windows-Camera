# ExternalCameraSettingsApp Sample

This sample app will demonstrate how to build an external camera settings app (also known as a "companion app") associated to a camera device. This app allows to adjust control in real time as well as set a control value as the default one to be applied every time the associated camera is started. The association between app and camera device is typically done by the camera driver at installation time (i.e. by a camera manufacturer wanting to offer an app tie-in to their physical device or a virtual camera implemented offering an app such as [the ***VirtualCameraManager_App*** in our other sample](..\VirtualCamera\README.md)). This settings app would then be launchable directly from the associated camera device settings page *(Windows Settings &rarr; Bluetooth & devices &rarr; Cameras &rarr; \<the associated camera\> in the "Related settings" section at the bottom)*

This sample also registers a callback for when a user changes the default value for a control it displays, such as when using the Windows Settings page of the target camera concurrently to this sample, so that it can refresh its UI to show the default value now stored. See the [IMFCameraControlMonitor sample](..\ControlMonitorApp\readme.md) for more detail on this mechanism.

This app will use camera symbolic link(symlink) name that's provided via [Application.OnLaunched](https://docs.microsoft.com/uwp/api/windows.ui.xaml.application.onlaunched?view=winrt-22000) arguments and typically passed further on to the [Page.OnNavigatedTo](https://docs.microsoft.com/uwp/api/windows.ui.xaml.controls.page.onnavigatedto?view=winrt-22000). The camera symlink is only passed to the app launch when the app is registered to be an additional Settings app for the specific camera.

## Requirements
This sample is built using Visual Studio 2019 and requires [Windows SDK version 22621](https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/).

## Registering the companion app with the target camera so that it can be launched from Windows Settings page
As external camera settings apps are typically companion apps for the specific camera, this registering process is normally done by driver inf or MSOS descriptor in FW that will add the registry entry.

**example excerpt showing how to declare a companion app in a .inf:**
~~~
[ContosoAvs.NT.Interfaces]
AddInterface = %KSCATEGORY_VIDEO_CAMERA%, "%Cam.RefGuid%", Cam.CaptureInterface.NT,
AddInterface = %KSCATEGORY_CAPTURE%, "%Cam.RefGuid%", Cam.CaptureInterface.NT,
AddInterface = %KSCATEGORY_VIDEO%, "%Cam.RefGuid%", Cam.CaptureInterface.NT,

[Cam.CaptureInterface.NT]
AddReg=ContosoAvs.Reg.Cam

[ContosoAvs.Reg.Cam]
HKR,,"SCSVCamPfn",,%YOUR_APP_PFN%

[Strings]
YOUR_APP_PFN="ExternalCameraSettingsAppSample_kd1fqbpx9e9ng"
Cam.RefGuid="{CF65BE8C-5F60-4C39-A2E7-504A365E6BD6}"
KSCATEGORY_VIDEO_CAMERA="{E5323777-F976-4F5B-9B55-B94699C46E44}"
KSCATEGORY_CAPTURE="{65E8773D-8F56-11D0-A3B9-00A0C9223196}"
KSCATEGORY_VIDEO="{6994AD05-93EF-11D0-A3CC-00A0C9223196}"
~~~

You can also specify it for now by editing the registry. The application's package family name (PFN) need to be added to the device interface node in the registry under: `HKLM\SYSTEM\CurrentControlSet\Control\DeviceClasses\{e5323777-f976-4f5b-9b55-b94699c46e44}\<camera symlink>\#GLOBAL\Device Parameters`
| Name | Type | Data |
|------|------|------|
| SCSVCamPfn | REG_SZ| \<PFN\> |

To identify PFN for your app, run Get-AppxPackage from PowerShell
e.g., `Get-AppxPackage -Name *ExternalCamera*`

Adding this information will create a button to launch the application from the camera configuration page in the Settings (Settings -> Bluetooth & devices -> Cameras -> \<camera name\> ). If the app is not installed it will try to search it from the Store.

**NOTE:** Uninstall and reinstall of the camera device and restart of the machine might be needed to edit or remove properly the associated configuration app.

## Sample functionality
 The app will initialize the given camera, start preview, and show the following controls (if camera implements them) that will set current value and save it as the default value by using [IMFCameraConfigurationManager](https://docs.microsoft.com/windows/win32/api/mfidl/nn-mfidl-imfcameraconfigurationmanager) functionality.
  - Contrast control slider
  - Brightness control slider
  - Background segmentation toggle for Blur on/off
  - Exposure Compensation Value control slider

**NOTE:** For default value configurations the sample uses MF_CAMERA_CONTROL_CONFIGURATION_TYPE_POSTSTART but some controls might require MF_CAMERA_CONTROL_CONFIGURATION_TYPE_PRESTART. These requirements come from the DDI definitions of that control.
