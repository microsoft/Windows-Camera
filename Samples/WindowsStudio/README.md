#  Windows Studio Effects camera sample application - C# .Net WinUI & WinRT

additional documentation regarding ***[Windows Studio Effect (WSE) and its Driver-Defined Interfaces (DDIs)](/Windows%20Studio%20Effects%20DDIs.md)***

>This sample will only run fully on a system equipped with a [Windows Studio Effects (*WSE*)](https://learn.microsoft.com/en-us/windows/ai/studio-effects/) camera, which in itself requires:
>1. a compatible NPU
>2. the related Windows Studio Effects driver package installed or pulled-in via Windows Update
>3. a camera opted into *WSE* by the device manufacturer in its driver. Currently these camera must be front-facing


This folder contains a C# sample named **WindowsStudioSample_WinUI** which checks if a Windows Studio Effects camera is available on the system. It then proceeds using WinRT APIs to leverage [extended camera controls](https://learn.microsoft.com/en-us/windows-hardware/drivers/stream/kspropertysetid-extendedcameracontrol) standardized in the OS and defined in Windows SDK such as the following 3 implemented as Windows Studio Effects in version 1: 
- Standard Blur, Portrait Blur and Segmentation Mask Metadata : KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION (*[DDI documentation](https://learn.microsoft.com/en-us/windows-hardware/drivers/stream/ksproperty-cameracontrol-extended-backgroundsegmentation)*)
- Eye Contact Standard and Teleprompter: KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION (*[DDI documentation](https://learn.microsoft.com/en-us/windows-hardware/drivers/stream/ksproperty-cameracontrol-extended-eyegazecorrection)*)
- Automatic Framing: KSPROPERTY_CAMERACONTROL_EXTENDED_DIGITALWINDOW (*[DDI documentation](https://learn.microsoft.com/en-us/windows-hardware/drivers/stream/ksproperty-cameracontrol-extended-digitalwindow)*) and KSPROPERTY_CAMERACONTROL_EXTENDED_DIGITALWINDOW_CONFIGCAPS (*[DDI documentation](https://learn.microsoft.com/en-us/windows-hardware/drivers/stream/ksproperty-cameracontrol-extended-digitalwindow-configcaps)*)


It also taps into newer effects available in version 2 that are exposed using a set of DDIs custom to Windows Studio Effects. Since these are exclusive to Windows Studio Effects and shipped outside the OS, their defninition is not part of the Windows SDK and has to be copied into your code base ([see DDI documentation](<./Windows Studio Effects DDIs.md>)):
- Portrait Light (*[DDI documentation](<./Windows Studio Effects DDIs.md#ksproperty_cameracontrol_windowsstudio_stagelight-control>)*)
- Creative Filters (Animated, Watercolor and Illustrated) (*[DDI documentation](<./Windows Studio Effects DDIs.md#ksproperty_cameracontrol_windowsstudio_creativefilter-control>)*)

The app demonstrates the following:

1. Looks for a Windows Studio camera on the system which must conform to the 2 below criteria:
    
    1. Checks if the system exposes the related Windows Studio dev prop key.
        ```csharp
        private const string DEVPKEY_DeviceInterface_IsWindowsCameraEffectAvailable = "{6EDC630D-C2E3-43B7-B2D1-20525A1AF120} 4";
        //...
        DeviceInformationCollection deviceInfoCollection = await DeviceInformation.FindAllAsync(MediaDevice.GetVideoCaptureSelector(), new List<string>() { DEVPKEY_DeviceInterface_IsWindowsCameraEffectAvailable });
        ```
    
    2. Searches for a front facing camera by checking the panel enclosure location of the camera devices.
        ```csharp
       DeviceInformation selectedDeviceInfo = deviceInfoCollection.FirstOrDefault(x => x.EnclosureLocation.Panel == Windows.Devices.Enumeration.Panel.Front);
        ```
    
2. [](WinRTGETSET) Check if the newer set of Windows Studio Effects in version 2 are supported. These new DDIs are defined in a new property set ([see DDI documentation](<./Windows Studio Effects DDIs.md>).
    ```csharp
    // New Windows Studio Effects custom KsProperties live under this property set
    public static readonly Guid KSPROPERTYSETID_WindowsStudioEffects = 
        Guid.Parse("1666d655-21A6-4982-9728-52c39E869F90");

    // Custom KsProperties exposed in version 2
    public enum KSPROPERTY_CAMERACONTROL_WINDOWS_EFFECTS : uint
    {
        KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_SUPPORTED = 0,
        KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_STAGELIGHT = 1,
        KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_CREATIVEFILTER = 2,
    };

    // ...
    // in InitializeCameraAndUI()
    // query support for new effects in v2
    byte[] byteResultPayload = GetExtendedControlPayload(
                                m_mediaCapture.VideoDeviceController,
                                KSPROPERTYSETID_WindowsStudioEffects,
                                (uint)KSPROPERTY_CAMERACONTROL_WINDOWS_EFFECTS.KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_SUPPORTED);

    // reinterpret the byte array as an extended property payload
    KSCAMERA_EXTENDEDPROP_HEADER payloadHeader = FromBytes<KSCAMERA_EXTENDEDPROP_HEADER>(byteResultPayload);
    int sizeofHeader = Marshal.SizeOf<KSCAMERA_EXTENDEDPROP_HEADER>();
    int sizeofKsProperty = Marshal.SizeOf<KsProperty>(); ;
    int supportedControls = ((int)payloadHeader.Size - sizeofHeader) / sizeofKsProperty;

    for (int i = 0; i < supportedControls; i++)
    {
        KsProperty payloadKsProperty = FromBytes<KsProperty>(byteResultPayload, sizeofHeader + i * sizeofKsProperty);

        if (new Guid(payloadKsProperty.Set) == KSPROPERTYSETID_WindowsStudioEffects)
        {
            // if we support StageLight (also known as PortraitLight)
            if (payloadKsProperty.Id == (uint)KSPROPERTY_CAMERACONTROL_WINDOWS_EFFECTS.KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_STAGELIGHT)
            {
                RefreshStageLightUI();
            }
            // if we support CreativeFilter
            else if (payloadKsProperty.Id == (uint)KSPROPERTY_CAMERACONTROL_WINDOWS_EFFECTS.KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_CREATIVEFILTER)
            {
                RefreshCreativeFilterUI();
            }
        }
    }
    // ...
    ```

3. Expose a button that launches the Windows Settings page for the camera where the user can also toggle effects concurrently while the sample app is running.
    ```csharp
    // launch Windows Settings page for the camera identified by the specified Id
    var uri = new Uri($"ms-settings:camera?cameraId={Uri.EscapeDataString(m_mediaCapture.MediaCaptureSettings.VideoDeviceId)}");
    var fireAndForget = Windows.System.Launcher.LaunchUriAsync(uri);

    // launch the Windows Studio quick setting panel using URI if it is supported
    var uri = new Uri($"ms-controlcenter:studioeffects");
    var supportStatus = await Windows.System.Launcher.QueryUriSupportAsync(uri, Windows.System.LaunchQuerySupportType.Uri);
    if (supportStatus == Windows.System.LaunchQuerySupportStatus.Available)
    {
        var fireAndForget = Windows.System.Launcher.LaunchUriAsync(uri);
    }
    ```


4. Proceeds to then stream from this camera and interact with Windows Studio effects via Extended Property DDI using [`VideoDeviceController.GetDevicePropertyByExtendedId()`](https://learn.microsoft.com/en-us/uwp/api/windows.media.devices.videodevicecontroller.getdevicepropertybyextendedid) and [`VideoDeviceController.SetDevicePropertyByExtendedId()`](https://learn.microsoft.com/en-us/uwp/api/windows.media.devices.videodevicecontroller.setdevicepropertybyextendedid) :
    1. Send a GET command and deserialize the retrieved payload, see implementation and usage of 
        ```csharp 
        byte[] KsHelper.GetExtendedControlPayload(
            VideoDeviceController controller,
            Guid propertySet,
            uint controlId)
        ```
        and 
        ```csharp 
        T KsHelper.FromBytes<T>(byte[] bytes, int startIndex = 0) 
        ``` 
        such as in this example:
        ```csharp 
        // send a GET call for the eye gaze DDI and retrieve the result payload
        byte[] byteResultPayload = KsHelper.GetExtendedControlPayload(
          m_mediaCapture.VideoDeviceController,
          KsHelper.KSPROPERTYSETID_ExtendedCameraControl,
          (uint)KsHelper.ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION);

        // reinterpret the byte array as an extended property payload
        KsHelper.KsBasicCameraExtendedPropPayload payload = KsHelper.FromBytes<KsHelper.KsBasicCameraExtendedPropPayload>(byteResultPayload); 
        ```

        2. Send a SET command using a serialized payload containing flags value to toggle effects, see implementation and usage of 
        ```csharp 
        void KsHelper.SetExtendedControlFlags(
            VideoDeviceController controller,
            Guid propertySet,
            uint controlId,
            ulong flags)
        ```
        and 
        ```csharp 
        byte[] KsHelper.ToBytes<T>(T item) 
        ``` 
        such as in this example:
        ```csharp 
        // set the flags value for the corresponding extended control
        KsHelper.SetExtendedControlFlags(
          m_mediaCapture.VideoDeviceController,
          KsHelper.KSPROPERTYSETID_ExtendedCameraControl,
          (uint)KsHelper.ExtendedControlKind.KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION,
          flagToSet);
        ```

The .sln solution file is also pulling a dependency on the existing WinRT Component named ["*ControlMonitorHelperWinRT*"](..\ControlMonitorApp\README.md) that wraps and projects to WinRT the [`IMFCameraControlMonitor`](https://learn.microsoft.com/en-us/windows/win32/api/mfidl/nn-mfidl-imfcameracontrolmonitor) native APIs so that we can refresh the app UI whenever an external application such as Windows Settings changes concurrently the current value of one of the DDI that drives Windows Studio effects.

## Requirements
	
This sample is built using Visual Studio 2022 and requires [Windows SDK version 22621](https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/) at the very least.

## Using the samples

The easiest way to use these samples without using Git is to download the zip file containing the current version (using the following link or by clicking the "Download ZIP" button on the repo page). You can then unzip the entire archive and use the samples in Visual Studio 2022.

   [Download the samples ZIP](../../archive/master.zip)

   **Notes:** 
   * Before you unzip the archive, right-click it, select **Properties**, and then select **Unblock**.
   * Be sure to unzip the entire archive, and not just individual samples. The samples all depend on the SharedContent folder in the archive.   
   * In Visual Studio 2022, the platform target defaults to ARM64, so be sure to change that to x64 if you want to test on a non-ARM64 device. 


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
