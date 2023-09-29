# C# UWP WinRT sample application for using a Windows Studio camera and its set of effects

>**This sample will only run fully on a system equipped with a [Windows Studio](https://blogs.windows.com/windowsexperience/2022/09/20/available-today-the-windows-11-2022-update/) camera, which in itself requires a NPU and the related Windows Studio driver package installed or pulled-in via Windows Update by the device manufacturer.**

This folder contains a single C# .csproj sample project named **UWPWindowsStudioSample** which checks if a Windows Studio camera is available on the system and then gets and sets extended camera controls associated with Windows Studio effects such as Background Blur, Eye Gaze Correction and Automatic Framing:
1. Looks for a Windows Studio camera on the system 
    1. checks if the system exposes the related Windows Studio dev prop key
        ```csharp
        private const string DEVPKEY_DeviceInterface_IsWindowsCameraEffectAvailable = "{6EDC630D-C2E3-43B7-B2D1-20525A1AF120} 4";
        //...
        DeviceInformationCollection deviceInfoCollection = await DeviceInformation.FindAllAsync(MediaDevice.GetVideoCaptureSelector(), new List<string>() { DEVPKEY_DeviceInterface_IsWindowsCameraEffectAvailable });
        ```
    2. Searches for a front facing camera by checking the panel enclosure location of the camera devices
        ```csharp
       DeviceInformation selectedDeviceInfo = deviceInfoCollection.FirstOrDefault(x => x.EnclosureLocation.Panel == Windows.Devices.Enumeration.Panel.Front);
        ```

2. Proceeds to then stream from this camera and interact with Windows Studio effects via Extended Property DDI using [`VideoDeviceController.GetDevicePropertyByExtendedId()`](https://learn.microsoft.com/en-us/uwp/api/windows.media.devices.videodevicecontroller.getdevicepropertybyextendedid) and [`VideoDeviceController.SetDevicePropertyByExtendedId()`](https://learn.microsoft.com/en-us/uwp/api/windows.media.devices.videodevicecontroller.setdevicepropertybyextendedid) :
    1. Send a GET command and deserialize the retrieved payload, see implementation and usage of 
        ```csharp 
        byte[] KsHelper.GetExtendedControlPayload(VideoDeviceController controller, KsHelper.ExtendedControlKind controlKind) 
        ```
        and 
        ```csharp 
        T KsHelper.FromBytes<T>(byte[] bytes, int startIndex = 0) 
        ``` 
        such as in this example:
        ```csharp 
        // send a GET call for the eye gaze DDI and retrieve the result payload
        byte[] byteResultPayload = KsHelper.GetExtendedControlPayload(
        m_mediaCapture.VideoDeviceController, KsHelper.ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION);

        // reinterpret the byte array as an extended property payload
        KsHelper.KsBasicCameraExtendedPropPayload payload = KsHelper.FromBytes<KsHelper.KsBasicCameraExtendedPropPayload>(byteResultPayload); 
        ```

        2. Send a SET command using a serialized payload containing flags value to toggle effects, see implementation and usage of 
        ```csharp 
        void KsHelper.SetExtendedControlFlags(
            VideoDeviceController controller, 
            KsHelper.ExtendedControlKind controlKind,
            ulong flags) 
        ```
        and 
        ```csharp 
        byte[] KsHelper.ToBytes<T>(T item) 
        ``` 
        such as in this example:
        ```csharp 
        // set the flags value for the corresponding extended control
        KsHelper.SetExtendedControlFlags(m_mediaCapture.VideoDeviceController, KsHelper.ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION, flagToSet); 
        ```

The .sln solution file is also pulling a dependency on the existing WinRT Component named ["*ControlMonitorHelper*"](..\ControlMonitorApp\README.md) that wraps and projects to WinRT the [`IMFCameraControlMonitor`](https://learn.microsoft.com/en-us/windows/win32/api/mfidl/nn-mfidl-imfcameracontrolmonitor) native APIs so that we can refresh the app UI whenever an external application such as Windows Settings changes concurrently the current value of one of the DDI that drives Windows Studio effects.

## Requirements
	
This sample is built using Visual Studio 2019 and requires [Windows SDK version 22621](https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/) at the very least.

## Using the samples

The easiest way to use these samples without using Git is to download the zip file containing the current version (using the following link or by clicking the "Download ZIP" button on the repo page). You can then unzip the entire archive and use the samples in Visual Studio 2019.

   [Download the samples ZIP](../../archive/master.zip)

   **Notes:** 
   * Before you unzip the archive, right-click it, select **Properties**, and then select **Unblock**.
   * Be sure to unzip the entire archive, and not just individual samples. The samples all depend on the SharedContent folder in the archive.   
   * In Visual Studio 2019, the platform target defaults to ARM, so be sure to change that to x64 if you want to test on a non-ARM device. 


**Reminder:** If you unzip individual samples, they will not build due to references to other portions of the ZIP file that were not unzipped. You must unzip the entire archive if you intend to build the samples.

For more info about the programming models, platforms, languages, and APIs demonstrated in these samples, please refer to the guidance, tutorials, and reference topics provided in the Windows 10 documentation available in the [Windows Developer Center](http://go.microsoft.com/fwlink/p/?LinkID=532421). These samples are provided as-is in order to indicate or demonstrate the functionality of the programming models and feature APIs for Windows.

## Contributing

This project welcomes contributions and suggestions.  Most contributions require you to agree to a
Contributor License Agreement (CLA) declaring that you have the right to, and actually do, grant us
the rights to use your contribution. For details, visit https://cla.microsoft.com.

When you submit a pull request, a CLA-bot will automatically determine whether you need to provide
a CLA and decorate the PR appropriately (e.g., label, comment). Simply follow the instructions
provided by the bot. You will only need to do this once across all repos using our CLA.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/).
For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or
contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.
