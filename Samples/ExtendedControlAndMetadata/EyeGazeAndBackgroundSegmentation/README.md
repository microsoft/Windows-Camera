# WinRT sample application getting and setting extended camera controls as well as extracting frame metadata

This folder contains sample projects for
- **EyeGazeAndBackgroundSegmentation**: a C# UWP application to preview camera and to interact with its extended controls
- **CameraKsPropertyHelper**: a C++/Winrt runtime component to interact with a [VideoDeviceController](https://docs.microsoft.com/en-us/uwp/api/windows.media.devices.videodevicecontroller?view=winrt-20348) to get and set extended camera controls via serialized/deserialized byte buffers. While this sample covers EyeGazeCorrection and BackgroundSegmentation, it can be extended to cover other controls using the same methodology. 
This project also contains a helper method to extract and deserialize frame metadata from a byte buffer. Similarly while it covers how to extract *```KSCAMERA_METADATA_BACKGROUNDSEGMENTATIONMASK```* metadata, it can be extended to cover other type of metadata using the same methodology.  

## Requirements
	
	This sample is built using Visual Studio 2019 and [Windows SDK version 19041](https://developer.microsoft.com/en-US/windows/downloads/windows-10-sdk) at minimum and requires [Windows SDK version 21364](TBD) to interact with the *KSCAMERA_METADATA_BACKGROUNDSEGMENTATIONMASK* metadata and the KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_MASK capability for the KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION control.

## Getting and setting extended camera controls
In the **CameraKsPropertyHelper** project, the ```PropertyInquiry``` runtime class containes static helper methods to set and get extended controls via serialized/deserialized byte buffers.

### GetDevicePropertyByExtendedId
We use the method [```GetDevicePropertyByExtendedId()```](https://docs.microsoft.com/en-us/uwp/api/windows.media.devices.videodevicecontroller.getdevicepropertybyextendedid?view=winrt-20348#Windows_Media_Devices_VideoDeviceController_GetDevicePropertyByExtendedId_System_Byte___Windows_Foundation_IReference_System_UInt32__) to send a GET command of an extended control to a camera device. This is used to verify if the control is supported as well as to query the capabilities of the device regarding this control. 
1) We first create GET command we will send in the proper format,
2) then serialize it to a byte buffer,
3) then call the aforementioned method
4) and retrieve the result for which the status indicates success of failure,
5) then finally deserialize the result value into a format we can parse.


```cpp
// PropertyInquiry.cpp

Windows::Foundation::IPropertyValue GetExtendedCameraControlPayload(Windows::Media::Devices::VideoDeviceController const& controller, int controlId)
    {
        Windows::Foundation::IPropertyValue property = nullptr;

        // 1. create the GET command
        KSPROPERTY prop(
            { 0x1CB79112, 0xC0D2, 0x4213, {0x9C, 0xA6, 0xCD, 0x4F, 0xDB, 0x92, 0x79, 0x72} } /* KSPROPERTYSETID_ExtendedCameraControl */,
            controlId /* KSPROPERTY_CAMERACONTROL_EXTENDED_* */,
            0x00000001 /* KSPROPERTY_TYPE_GET */);
        
        // 2. serialize to byte buffer
        uint8_t* pProp = reinterpret_cast<uint8_t*>(&prop);

        array_view<uint8_t const> serializedProp = array_view<uint8_t const>(pProp, sizeof(KSPROPERTY));

        // 3. send the GET command
        auto getResult = controller.GetDevicePropertyByExtendedId(serializedProp, nullptr);

        // 4. check status
        if (getResult.Status() == Windows::Media::Devices::VideoDeviceControllerGetDevicePropertyStatus::NotSupported)
        {
            return property;
        }
        if (getResult.Status() != Windows::Media::Devices::VideoDeviceControllerGetDevicePropertyStatus::Success)
        {
            throw hresult_invalid_argument(L"Could not retrieve device property " + winrt::to_hstring(controlId) + L" with status: " + winrt::to_hstring((int)getResult.Status()));
        }
        
        // 5. deserialize the result into a format we can understand 
		// see the PropertyValuePayloadHolder class in kshelper.h that takes the IPropertyValue
		// as constructor argument
        auto result = getResult.Value();
        property = result.as<Windows::Foundation::IPropertyValue>();

        return property;
    }
```

### SetDevicePropertyByExtendedId
Similarly to a GET command, we use the method [```SetDevicePropertyByExtendedId()```](https://docs.microsoft.com/en-us/uwp/api/windows.media.devices.videodevicecontroller.setdevicepropertybyextendedid?view=winrt-20348#Windows_Media_Devices_VideoDeviceController_SetDevicePropertyByExtendedId_System_Byte___System_Byte___) to send a SET command of an extended control to a camera device. This is used to modify the state of a device control if it is supported.
1) we first create SET command we will send in the proper format..
2) then serialize it to a byte buffer..
3) we then create the setting payload..
4) then serialize it as well to a byte buffer..
5) then call the aforementioned method..
6) and retrieve the result for which the status indicates success of failure..

```cpp
// PropertyInquiry.cpp

void PropertyInquiry::SetExtendedControlFlags(
        Windows::Media::Devices::VideoDeviceController const& controller,
        winrt::CameraKsPropertyHelper::ExtendedControlKind const& extendedControlKind,
        uint64_t flags)
    {
        int controlId = static_cast<int>(extendedControlKind);
        
        // 1. create the SET command
        KSPROPERTY prop(
            { 0x1CB79112, 0xC0D2, 0x4213, {0x9C, 0xA6, 0xCD, 0x4F, 0xDB, 0x92, 0x79, 0x72} } /* KSPROPERTYSETID_ExtendedCameraControl */,
            controlId /* KSPROPERTY_CAMERACONTROL_EXTENDED_* */,
            0x00000002 /* KSPROPERTY_TYPE_SET */);

        // 2. serialize to byte buffer
        uint8_t* pProp = reinterpret_cast<uint8_t*>(&prop);
        array_view<uint8_t const> serializedProp = array_view<uint8_t const>(pProp, sizeof(KSPROPERTY));

        // 3. create the setting payload
        KSCAMERA_EXTENDEDPROP_HEADER value =
        {
            1, // Version
            1, // PinId = KSCAMERA_EXTENDEDPROP_FILTERSCOPE
            sizeof(KSCAMERA_EXTENDEDPROP_HEADER) + sizeof(KSCAMERA_EXTENDEDPROP_VALUE), // Size
            0, // Result
            flags, // Flags
            0 // Capability                
        };

        // 4. serialize to byte buffer
        uint8_t* pValue = reinterpret_cast<uint8_t*>(&value);
        array_view<uint8_t const> serializedValue = array_view<uint8_t const>(pValue, value.Size);

        // 5. send the GET command
        auto setResult = controller.SetDevicePropertyByExtendedId(serializedProp, serializedValue);

        // 6. check status
        if (setResult != Windows::Media::Devices::VideoDeviceControllerSetDevicePropertyStatus::Success)
        {
            throw hresult_invalid_argument(L"Could not set device property flags " + winrt::to_hstring(controlId) + L" with status: " + winrt::to_hstring((int)setResult));
        }
    }
```

### MediaFrameReference::Properties
To retrieve frame metadata we use the method [```MediaFrameReference::Properties()```](https://docs.microsoft.com/en-us/uwp/api/windows.media.capture.frames.mediaframereference.properties?view=winrt-20348#Windows_Media_Capture_Frames_MediaFrameReference_Properties). This returns a map of ```<Guid, IInspectable>``` that we can then query for metadata and reinterpret values appropriately. In our case, the capture metadata we are looking for is contained in a sub-map dedicated to capture metadata.
1) we first look for the presence of the capture metadata sub-map using the ```Guid``` key ```MFSampleExtension_CaptureMetadata``` (see [possible capture metadata](https://docs.microsoft.com/en-us/windows-hardware/drivers/stream/mf-capture-metadata))
2) then in that sub-map look for the presence of the capture metadata we are looking for using its ```Guid``` as key ```MF_CAPTURE_METADATA_FRAME_BACKGROUND_MASK```
3) we then deserialize the metadata retrieved as a ```IPropertyValue```. We retrieve the byte buffer and reinterpret it according to its expected format.

```cpp
// PropertyInquiry.cpp

winrt::CameraKsPropertyHelper::IMetadataPayload PropertyInquiry::ExtractFrameMetadata(winrt::Windows::Media::Capture::Frames::MediaFrameReference const& frame, winrt::CameraKsPropertyHelper::FrameMetadataKind const& metadataKind)
    {
        static guid captureMetadataGuid = guid(0x2EBE23A8, 0xFAF5, 0x444A, { 0xA6, 0xA2, 0xEB, 0x81, 0x08, 0x80, 0xAB, 0x5D }); // MFSampleExtension_CaptureMetadata
        static guid backgroundSegmentationMaskGuid = guid(0x3f14dd3, 0x75dd, 0x433a, { 0xa8, 0xe2, 0x1e, 0x3f, 0x5f, 0x2a, 0x50, 0xa0 }); // MF_CAPTURE_METADATA_FRAME_BACKGROUND_MASK
        static guid dwKey = guid(0x276f72a2, 0x59c8, 0x4f69, { 0x97, 0xb4, 0x6, 0x8b, 0x8c, 0xe, 0xc0, 0x44 });
        CameraKsPropertyHelper::IMetadataPayload result = nullptr;

        if (frame == nullptr)
        {
            throw hresult_invalid_argument(L"null frame");
        }
        auto properties = frame.Properties();
        if (properties != nullptr)
        {
            if (metadataKind == FrameMetadataKind::MetadataId_BackgroundSegmentationMask)
            {
                // 1. look for the presence of capture metadata sub-map
                auto captureMetadata = properties.TryLookup(captureMetadataGuid);
                if (captureMetadata != nullptr)
                {
                    auto captureMetadataPropSet = captureMetadata.as<Windows::Foundation::Collections::IMapView<guid, winrt::Windows::Foundation::IInspectable>>();

                    // 2. look for the presence of the specific capture metadata we want
                    auto metadata = captureMetadataPropSet.TryLookup(backgroundSegmentationMaskGuid);

                    if (metadata != nullptr)
                    {
                        auto metadataPropVal = metadata.as<Windows::Foundation::IPropertyValue>();

                        // 3. deserialize the IPropertyValue we retrieve
                        result = make<BackgroundSegmentationMaskMetadata>(metadataPropVal);
                    }
                }
                else
                {
                    // no metadata present
                    return result;
                }
            }
            else
            {
                throw hresult_not_implemented(L"no handler for FrameMetadataKind = " + to_hstring((int)metadataKind));
            }
        }
        // else there is no frame metadata present

        return result;
    }
```

## Eye Gaze Correction
the eye gaze correction DDI (*```KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION```*) has been introduced in Windows starting with build 20348. If supported by the driver, it slightly changes the predominant face's eye position so that the main subject appears as if looking directly into the camera.
Programmatically it acts simply as a ON/OFF toggle by setting these flag values accordingly in the ```KSCAMERA_EXTENDEDPROP_HEADER```:
```
KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_OFF = 0x0000000000000000
KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_ON = 0x0000000000000001
```

## Background Segmentation control
the Background segmentation DDI (*```KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION```*) has been introduced in Windows starting with build 20348. In this initial release, if supported by the driver, it allows to blur the background of a scene and preserve only parts of the image where the main subject is present. This is a popular scenario in video teleconferencing applications.
Programmatically it acts simply as a ON/OFF toggle for background blur by setting these flag values accordingly in the ```KSCAMERA_EXTENDEDPROP_HEADER```:
```
KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_OFF = 0x0000000000000000
KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR = 0x0000000000000001
```

Additionaly in Windows build above 21364, a new mode has been added that requests the driver to attach the segmentation mask as metadata to each frame streamed out. This mode is signaled using this flag value:
```
KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_MASK = 0x0000000000000002
```
When a GET command is sent for the ```KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION```, **and only** whenever the camera device supports the ```KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_MASK``` mode, it will advise with which stream configurations it supports the generation of the mask metadata. This is  for an application wanting to understand if there are any limitations before attempting to rely on a mask to operate scenarios such as alpha blending of the background portion of an image. A camera device that supports this control may not always do so across all its stream configurations. For example, a camera could be able to segment background at 30fps on a 1080p stream, but not at 60fps even though it may be able to stream at such resolution and framerate combination. The structure returned to advise which stream configuration support the generation of the mask frame metadata is a set of ```KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_CONFIGCAPS```. The app can then correlate the available stream configurations of a camera with this set to understand what to expect if it sets this control with the ```KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_MASK``` flag.

## Background Segmentation mask metadata
The background segmentation mask contains the mask metadata frame as a contiguous buffer, but also some relevant information about how the mask relates to the frame it correlates with.
```cpp
struct KSCAMERA_METADATA_BACKGROUNDSEGMENTATIONMASK
    {
        KSCAMERA_METADATA_ITEMHEADER Header;
        RECT MaskCoverageBoundingBox; // rectangle in absolute coordinates of the frame defining which part of the field of view is covered by the mask
        SIZE MaskResolution; // the resolution of the mask frame attached below in MaskData (i.e. a contiguous buffer of MaskResolution.cx * MaskResolution.cy)
        RECT ForegroundBoundingBox; // rectangle in absolute coordinates of the mask that specifies which portion contains only foreground pixels. Implementation may chose to simply return coordinates for the whole mask and let the consumer parse the whole mask.
        BYTE MaskData[1]; // Array of mask data of dimension MaskResolution.cx * MaskResolution.cy
    };
```
See example depiction of a background segmentation mask in regard to the ```KSCAMERA_METADATA_BACKGROUNDSEGMENTATIONMASK``` metadata structure:

![example](example_BACKGROUNDSEGMENTATIONMASK.jpg)

## Using the samples

The easiest way to use these samples without using Git is to download the zip file containing the current version (using the following link or by clicking the "Download ZIP" button on the repo page). You can then unzip the entire archive and use the samples in Visual Studio 2017.

   [Download the samples ZIP](../../archive/master.zip)

   **Notes:** 
   * Before you unzip the archive, right-click it, select **Properties**, and then select **Unblock**.
   * Be sure to unzip the entire archive, and not just individual samples. The samples all depend on the SharedContent folder in the archive.   
   * In Visual Studio 2017, the platform target defaults to ARM, so be sure to change that to x64 or x86 if you want to test on a non-ARM device. 


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