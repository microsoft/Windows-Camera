# Windows Studio Effects *(WSE)* driver – Windows DDIs and Exclusive DDIs

The *WSE* driver can be interacted with using APIs which transacts according to a specified Driver-Defined Interface *(DDI)* using a [KSPROPERTY](https://learn.microsoft.com/en-us/windows-hardware/drivers/stream/ksproperty-structure) that dictates either a GET or a SET operation, followed by a data payload (*PropertyData*). 



## Windows standardized DDIs implemented by *WSE*
Some of the Windows Studio Effects are driven using standardized Windows DDIs included in the Windows SDK under 
the [extended camera controls](https://learn.microsoft.com/en-us/windows-hardware/drivers/stream/kspropertysetid-extendedcameracontrol) property set:
- **Standard Blur, Portrait Blur and Segmentation Mask Metadata**: KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION (*[DDI documentation](https://learn.microsoft.com/en-us/windows-hardware/drivers/stream/ksproperty-cameracontrol-extended-backgroundsegmentation)*)
- **Eye Contact Standard and Teleprompter**: KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION (*[DDI documentation](https://learn.microsoft.com/en-us/windows-hardware/drivers/stream/ksproperty-cameracontrol-extended-eyegazecorrection)*)
- **Automatic Framing**: KSPROPERTY_CAMERACONTROL_EXTENDED_DIGITALWINDOW (*[DDI documentation](https://learn.microsoft.com/en-us/windows-hardware/drivers/stream/ksproperty-cameracontrol-extended-digitalwindow)*) and KSPROPERTY_CAMERACONTROL_EXTENDED_DIGITALWINDOW_CONFIGCAPS (*[DDI documentation](https://learn.microsoft.com/en-us/windows-hardware/drivers/stream/ksproperty-cameracontrol-extended-digitalwindow-configcaps)*)

From an application standpoint, these Windows standardized DDIs can be programmatically interacted with either using:
- *WinRT*, refer to the example on the [previous page here](.\README.md#WinRT_GET_SET) and in the code sample included in this repo
- *Win32* level via the *[IMFExtendedCameraController](https://learn.microsoft.com/en-us/windows/win32/api/mfidl/nn-mfidl-imfextendedcameracontroller)* retrieved from an *IMFMediaSource* followed by 
an instance of the *[IMFExtendedCameraControl](https://learn.microsoft.com/en-us/windows/win32/api/mfidl/nn-mfidl-imfextendedcameracontrol)* for each control. Here's an example for sending a GET and a SET payload for the **[Eye Contact Standard](https://learn.microsoft.com/en-us/windows-hardware/drivers/stream/ksproperty-cameracontrol-extended-eyegazecorrection)** DDI.
~~~cpp
#include <ks.h>
#include <ksmedia.h>

// Assuming you initialize your camera source "spMediaSource" in your code using IMFCaptureEngine
wil::com_ptr_nothrow<IMFMediaSource> spMediaSource;

    //... in your method ...

    // Retrieve the IMFExtendedCameraController
    wil::com_ptr_nothrow<IMFExtendedCameraController> spExtendedCameraController;
    wil::com_ptr_nothrow<IMFGetService> spGetService;
    wil::com_ptr_nothrow<IMFExtendedCameraControl> spExtendedCameraControl;

    RETURN_IF_FAILED(spMediaSource->QueryInterface(IID_PPV_ARGS(&spGetService)));
    RETURN_IF_FAILED(spGetService->GetService(GUID_NULL, IID_PPV_ARGS(&spExtendedCameraController)));

    // If the GET call succeeds
    if (SUCCEEDED(spExtendedCameraController->GetExtendedCameraControl(MF_CAPTURE_ENGINE_MEDIASOURCE,
        KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION,
        &spExtendedCameraControl)))
    {
        // Check if Eye gaze correction DDI is supported and it is not already ON
        if (KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_ON & spExtendedCameraControl->GetCapabilities()
            && (KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_OFF == spExtendedCameraControl->GetFlags()))
        {
            // Tell the camera to turn it on.
            RETURN_IF_FAILED(spExtendedCameraControl->SetFlags(KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_ON));
            // Write the changed settings to the driver.
            RETURN_IF_FAILED(spExtendedCameraControl->CommitSettings());
        }
    }
~~~


## Custom DDIs implemented by *WSE* driver
A newer set of effects supported in *WSE* version 2 are exposed as custom DDIs under a different and exclusive property set (***KSPROPERTYSETID_WindowsStudioCameraControl*** defined below) not standardized with a particular Windows SDK:

 - **Portrait Light: [KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_STAGELIGHT](#KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_STAGELIGHT)**
 - **Creative Filters: [KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_CREATIVEFILTER](#KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_CREATIVEFILTER)**

 Furthermore, the Windows Studio driver component exposes additional DDIs that can be useful to applications to advertise some capabilities: 
 - **[KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_SUPPORTED](#KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_SUPPORTED)**
 - **[KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_PERFORMANCEMITIGATION](#KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_PERFORMANCEMITIGATION)**

 Finally, the Windows Studio driver component exposes a DDI useful to other driver components so that they can register to be notified when certain effects are toggled in WSE:
  - **[KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_SETNOTIFICATION](#KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_SETNOTIFICATION)**

~~~cpp
// Property set exclusive to Windows Studio Effects under which custom DDIs are defined
// {C1D740A8-BC18-43F4-A500-6BDF91839FDE}
static GUID KSPROPERTYSETID_WindowsStudioCameraControl = { 0x1666d655, 0x21a6, 0x4982, 0x97, 0x28, 0x52, 0xc3, 0x9e, 0x86, 0x9f, 0x90 };

// Windows Studio Effects custom DDIs defined under the KSPROPERTYSETID_WindowsStudioCameraControl property set
typedef enum {
    KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_SUPPORTED = 0,
    KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_STAGELIGHT = 1,
    KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_CREATIVEFILTER = 2,
    KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_SETNOTIFICATION = 3,
    KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_PERFORMANCEMITIGATION = 4
} KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_PROPERTY;
~~~
In order to use these DDIs for *WSE* version 2 in your code, you would copy those definitions into your header file as they are not included in any of the Windows SDK.

From an application standpoint, these *WSE* custom DDIs can be programmatically interacted with either using:
- *WinRT*, refer to the example on the [previous page here](.\README.md#WinRT_GET_SET) and in the code sample included in this repo
- *Win32* level via the the *[IKSControl](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ksproxy/nn-ksproxy-ikscontrol)* interface retrieved from an *IMFMediaSource* followed by invoking the *[IKsControl::KsProperty()](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ksproxy/nf-ksproxy-ikscontrol-ksproperty)* method with the data payload below prescribed for each of the *WSE* custom DDI. Here's an example for sending a GET and a SET payload for the **[Creative Filters](#KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_CREATIVEFILTER)** custom DDI.
~~~cpp
#include <ks.h>
#include <ksmedia.h>

// Assuming you initialize your camera source "spMediaSource" in your code using IMFCaptureEngine
wil::com_ptr_nothrow<IMFMediaSource> spMediaSource;

    //... in your method ...

    // Retrieve the IKsControl interface
    wil::com_ptr_nothrow<IKsControl> spKsControl;
    if (SUCCEEDED(spMediaSource->QueryInterface(IID_PPV_ARGS(&spKsControl))))
    {
        // Create a GET data payload to send down to the *WSE* driver to query capability

        // First define the target DDI and operation (a GET) using a KSPROPERTY
        KSPROPERTY creativeFilterGetProperty =
        {
            KSPROPERTYSETID_WindowsCameraEffect, // set
            KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_CREATIVEFILTER, // id
            KSPROPERTY_TYPE_GET // flag
        };

        // Create the data payload for a GET operation.
        // The data payload layout follows the specification and is comprised in this case of a KSCAMERA_EXTENDEDPROP_HEADER 
        // followed by a KSCAMERA_EXTENDEDPROP_VALUE
        ULONG getCreativeFilterPayloadSizeNeeded = sizeof(KSCAMERA_EXTENDEDPROP_HEADER) + sizeof(KSCAMERA_EXTENDEDPROP_VALUE);
        wil::unique_cotaskmem_ptr<BYTE[]> spGetCreativeFilterPayload = wil::make_unique_cotaskmem_nothrow<BYTE[]>(getCreativeFilterPayloadSizeNeeded);
        KSCAMERA_EXTENDEDPROP_HEADER* pExtendedHeader = (KSCAMERA_EXTENDEDPROP_HEADER*)spGetCreativeFilterPayload.get();
        KSCAMERA_EXTENDEDPROP_VALUE* pValue = (KSCAMERA_EXTENDEDPROP_VALUE*)(pExtendedHeader + 1);
        
        // Assign proper values to the payload
        *pExtendedHeader =
        {
            1, // Version
            KSCAMERA_EXTENDEDPROP_FILTERSCOPE, // PinID
            getCreativeFilterPayloadSizeNeeded, // Size
            0, // Result
            0, // Flags
            0 // Capability
        };
        pValue->Value.ull = 0;

        // Check if CreativeFilter DDI is supported in driver
        HRESULT hr = spKsControl->KsProperty(
            &creativeFilterGetProperty,
            sizeof(KSPROPERTY), 
            spGetCreativeFilterPayload.get(), 
            getCreativeFilterPayloadSizeNeeded, 
            &getCreativeFilterPayloadSizeNeeded);

        // if the DDI is supported and a particular Flags value value reported as supported in the Capability field, 
        // in this case KSCAMERA_WINDOWSSTUDIO_CREATIVEFILTER_ILLUSTRATED, 
        // let's now send a SET payload to set that particular Flags value
        if (SUCCEEDED(hr) && (KSCAMERA_WINDOWSSTUDIO_CREATIVEFILTER_ILLUSTRATED & pExtendedHeader->Capability))
        {
            // Define the target DDI and operation (a SET this time) using a KSPROPERTY
            KSPROPERTY creativeFilterSetProperty =
            {
                KSPROPERTYSETID_WindowsCameraEffect, // set
                KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_CREATIVEFILTER, // id
                KSPROPERTY_TYPE_SET// flag
            };

            // Create the data payload for a SET operation.
            // The data payload layout follows the specification and is comprised of a KSCAMERA_EXTENDEDPROP_HEADER 
            // followed by a KSCAMERA_EXTENDEDPROP_VALUE
            ULONG setCreativeFilterPayloadSizeNeeded = getCreativeFilterPayloadSizeNeeded;
            wil::unique_cotaskmem_ptr<BYTE[]> spSetCreativeFilterPayload = wil::make_unique_cotaskmem_nothrow<BYTE[]>(setCreativeFilterPayloadSizeNeeded);
            KSCAMERA_EXTENDEDPROP_HEADER* pExtendedHeader2 = (KSCAMERA_EXTENDEDPROP_HEADER*)spSetCreativeFilterPayload.get();
            KSCAMERA_EXTENDEDPROP_VALUE* pValue2 = (KSCAMERA_EXTENDEDPROP_VALUE*)(pExtendedHeader2 + 1);
            
            // Assign proper values to the payload
            *pExtendedHeader2 =
            {
                1, // Version
                KSCAMERA_EXTENDEDPROP_FILTERSCOPE, // PinID
                getCreativeFilterPayloadSizeNeeded, // Size
                0, // Result
                KSCAMERA_WINDOWSSTUDIO_CREATIVEFILTER_ILLUSTRATED, // Flags
                0 // Capability
            };
            pValue->Value.ull = 0;

            // Send a SET payload for the CreativeFilter DDI
            hr = spKsControl->KsProperty(
                &creativeFilterSetProperty,
                sizeof(KSPROPERTY),
                spSetCreativeFilterPayload.get(),
                setCreativeFilterPayloadSizeNeeded,
                &setCreativeFilterPayloadSizeNeeded);

            if (SUCCEEDED(hr))
            {
                // we have sucessfully sent our SET payload
            }
        }
    }
~~~



# WSE custom DDI specification
The GET and SET data payload format for each of these custom WSE DDIs is covered below. 

----------
## <a id="KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_SUPPORTED"></a> KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_SUPPORTED Control
This control allows to retrieve a list of supported KSProperties intercepted and implemented specifically by the Windows Studio Effects (*WSE*) camera driver component. Think of it as exclusively a *getter* for which controls corresponding to KSPROPERTY_CAMERACONTROL_EXTENDED_\*, KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_\* and any other custom property set that are implemented directly within *WSE* driver component. It can be preemptively queried before individually proceeding to getting capabilities for each of them if interaction with exclusively the *WSE* driver is desired or simply used to correlate with an existing set of DDI capabilities already fetched to understand if their implementation is provided by *WSE* or another component that is part of the camera driver stack.

### Usage Summary
| Scope | Control | Type | GET | SET |
| ----- | ----- | ----- | ----- | ----- |
| Version 1	| Filter | Synchronous| ✔️ | ❌ |

The SET call of this control will fail.

The KSPROPERTY payload follows the same layout as traditional KSPROPERTY_CAMERACONTROL_EXTENDED_PROPERTY, comprised of a [KSCAMERA_EXTENDEDPROP_HEADER](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ksmedia/ns-ksmedia-tagkscamera_extendedprop_header) followed by a content of size of *sizeof(KSPROPERTY) \** ***n***
where ***“n”*** is the count of [KSPROPERTY](https://learn.microsoft.com/en-us/windows-hardware/drivers/stream/ksproperty-structure) reported.
#### KSCAMERA_EXTENDEDPROP_HEADER
| **Member** | **Description** |
| ----- | ----- |
| **Version** |	Must be 1 |
| **PinId**	| This must be KSCAMERA_EXTENDEDPROP_FILTERSCOPE (0xFFFFFFFF).
| **Size**	| sizeof(KSCAMERA_EXTENDEDPROP_HEADER) + n * sizeof(KSPROPERTY) where “n” is the count of KSPROPERTY reported |
| **Result** | Unused, must be 0
| **Capability** | Unused, must be 0
| **Flags** | Unused, must be 0


for example, a GET call could return the following payload to be reinterpreted: if *WSE* supports all the effects in version 1 and 2, containing a KSCAMERA_EXTENDEDPROP_HEADER struct followed by **8** KSPROPERTY struct :
| | | |
|----- | ----- | ----- |
|⬇️| [KSCAMERA_EXTENDEDPROP_HEADER](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ksmedia/ns-ksmedia-tagkscamera_extendedprop_header) | *Version* = 1, <br />*PinID* = KSCAMERA_EXTENDEDPROP_FILTERSCOPE <br />*Size* = **8** \* sizeof(KSPROPERTY) <br />*Result* = 0 <br />*Capability* = 0 <br />*Flags* = 0|
|⬇️| [KSPROPERTY](https://learn.microsoft.com/en-us/windows-hardware/drivers/stream/ksproperty-structure) | *Set* = KSPROPERTYSETID_ExtendedCameraControl <br />*Id* = KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION <br />*Flags* = KSPROPERTY_TYPE_GET |
|⬇️| [KSPROPERTY](https://learn.microsoft.com/en-us/windows-hardware/drivers/stream/ksproperty-structure) | *Set* = KSPROPERTYSETID_ExtendedCameraControl <br />*Id* = KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION <br />*Flags* = KSPROPERTY_TYPE_GET |
|⬇️| [KSPROPERTY](https://learn.microsoft.com/en-us/windows-hardware/drivers/stream/ksproperty-structure) | *Set* = KSPROPERTYSETID_ExtendedCameraControl <br />*Id* = KSPROPERTY_CAMERACONTROL_EXTENDED_DIGITALWINDOW <br />*Flags* = KSPROPERTY_TYPE_GET |
|⬇️| [KSPROPERTY](https://learn.microsoft.com/en-us/windows-hardware/drivers/stream/ksproperty-structure) | *Set* = KSPROPERTYSETID_ExtendedCameraControl <br />*Id* = KSPROPERTY_CAMERACONTROL_EXTENDED_DIGITALWINDOW_CONFIGCAPS <br />*Flags* = KSPROPERTY_TYPE_GET |
|⬇️| [KSPROPERTY](https://learn.microsoft.com/en-us/windows-hardware/drivers/stream/ksproperty-structure) | *Set* = KSPROPERTYSETID_ExtendedCameraControl <br />*Id* = KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_STAGELIGHT <br />*Flags* = KSPROPERTY_TYPE_GET |
|⬇️| [KSPROPERTY](https://learn.microsoft.com/en-us/windows-hardware/drivers/stream/ksproperty-structure) | *Set* = KSPROPERTYSETID_ExtendedCameraControl <br />*Id* = KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_CREATIVEFILTER <br />*Flags* = KSPROPERTY_TYPE_GET |
|⬇️| [KSPROPERTY](https://learn.microsoft.com/en-us/windows-hardware/drivers/stream/ksproperty-structure) | *Set* = KSPROPERTYSETID_ExtendedCameraControl <br />*Id* = KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_SETNOTIFICATION <br />*Flags* = KSPROPERTY_TYPE_GET |
|🔚| [KSPROPERTY](https://learn.microsoft.com/en-us/windows-hardware/drivers/stream/ksproperty-structure) | *Set* = KSPROPERTYSETID_ExtendedCameraControl <br />*Id* = KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_PERFORMANCEMITIGATION <br />*Flags* = KSPROPERTY_TYPE_GET |

----------
## <a id="KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_STAGELIGHT"></a> KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_STAGELIGHT Control
The Stage Light DDI allows to turn ON or OFF the application of the Stage Light effect on the frames. This effect enhances the visual appearance of a face by identifying then processing face pixels with a suite of solutions such as augmenting resolution, removing noise and illumination artifacts, brightening, etc. 

### Usage Summary
| Scope | Control | Type | GET | SET |
| ----- | ----- | ----- | ----- | ----- |
| Version 1	| Filter | Synchronous| ✔️ | ✔️ |

The KSPROPERTY payload follows the same layout as traditional KSPROPERTY_CAMERACONTROL_EXTENDED_PROPERTY, comprised of a [KSCAMERA_EXTENDEDPROP_HEADER](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ksmedia/ns-ksmedia-tagkscamera_extendedprop_header) followed by a content of size of a [KSCAMERA_EXTENDEDPROP_VALUE](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ksmedia/ns-ksmedia-tagkscamera_extendedprop_value).

~~~cpp
// Stage Light possible flags values
#define KSCAMERA_WINDOWSSTUDIO_STAGELIGHT_OFF 0x0000000000000000
#define KSCAMERA_WINDOWSSTUDIO_STAGELIGHT_ON  0x0000000000000001
~~~

#### KSCAMERA_EXTENDEDPROP_HEADER
| **Member** | **Description** |
| ----- | ----- |
| **Version** |	Must be 1 |
| **PinId**	| This must be KSCAMERA_EXTENDEDPROP_FILTERSCOPE (0xFFFFFFFF). |
| **Size**	| sizeof(KSCAMERA_EXTENDEDPROP_HEADER) + sizeof(KSCAMERA_EXTENDEDPROP_VALUE) |
| **Result** | Unused, must be 0 |
| **Capability** | Must at least contain a different valid potential flag value than KSCAMERA_WINDOWSSTUDIO_STAGELIGHT_OFF |
| **Flags** | KSCAMERA_WINDOWSSTUDIO_STAGELIGHT_OFF or KSCAMERA_WINDOWSSTUDIO_STAGELIGHT_ON |

----------
## <a id="KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_CREATIVEFILTER"></a> KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_CREATIVEFILTER Control
The Creative Filter DDI allows to toggle a pre-defined effect that alters the appearance of a primary subject in the scene. The effect modifies the facial area and potentially also the background of the main subject.

### Usage Summary
| Scope | Control | Type | GET | SET |
| ----- | ----- | ----- | ----- | ----- |
| Version 1	| Filter | Synchronous| ✔️ | ✔️ |

The KSPROPERTY payload follows the same layout as traditional KSPROPERTY_CAMERACONTROL_EXTENDED_PROPERTY, comprised of a [KSCAMERA_EXTENDEDPROP_HEADER](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ksmedia/ns-ksmedia-tagkscamera_extendedprop_header) followed by a content of size of a [KSCAMERA_EXTENDEDPROP_VALUE](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ksmedia/ns-ksmedia-tagkscamera_extendedprop_value).

~~~cpp
// Creative Filter possible flags values
#define KSCAMERA_WINDOWSSTUDIO_CREATIVEFILTER_OFF         0x0000000000000000
#define KSCAMERA_WINDOWSSTUDIO_CREATIVEFILTER_ILLUSTRATED 0x0000000000000001
#define KSCAMERA_WINDOWSSTUDIO_CREATIVEFILTER_ANIMATED    0x0000000000000002
#define KSCAMERA_WINDOWSSTUDIO_CREATIVEFILTER_WATERCOLOR  0x0000000000000004
~~~

#### KSCAMERA_EXTENDEDPROP_HEADER
| **Member** | **Description** |
| ----- | ----- |
| **Version** |	Must be 1 |
| **PinId**	| This must be KSCAMERA_EXTENDEDPROP_FILTERSCOPE (0xFFFFFFFF). |
| **Size**	| sizeof(KSCAMERA_EXTENDEDPROP_HEADER) + sizeof(KSCAMERA_EXTENDEDPROP_VALUE) |
| **Result** | Unused, must be 0 |
| **Capability** | Must at least contain a different valid potential flag value than KSCAMERA_WINDOWSSTUDIO_CREATIVEFILTER_OFF |
| **Flags** |KSCAMERA_WINDOWSSTUDIO_ CREATIVEFILTER_OFF or KSCAMERA_WINDOWSSTUDIO_CREATIVEFILTER_ILLUSTRATED or KSCAMERA_WINDOWSSTUDIO_CREATIVEFILTER_ANIMATED or KSCAMERA_WINDOWSSTUDIO_CREATIVEFILTER_WATERCOLOR |

----------
## <a id="KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_SETNOTIFICATION"></a> KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_SETNOTIFICATION Control 

This control is not meant to be used by application and camera session clients. 

It serves as a way for *WSE* driver component to inform other driver components sitting in the driver stack such as a DMFT that an effect DDI state was modified. This may trigger internal changes in these other driver components to accommodate the state change such as disabling or altering pixel processing accordingly. Therefore a driver component that wants to be notified of a Windows Studio Effects DDI state change may expose support for this KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_SETNOTIFICATION DDI.
An example of this could be when *WSE* is requested to turn ON Automatic Framing via the KSProperty KSPROPERTY_CAMERACONTROL_EXTENDED_DIGITALWINDOW; a DeviceMFT may change the way it compensates for lens distortion as to mitigate image quality artifacts when zooming into the fringes of the frame. This notification is sent from *WSE* and relies on each subsequent driver component in the chain to relay it further to the next component. 

**If a driver component intercepts this KSProperty DDI, it shall as well relay it to the subsequent component for both a SET and a GET in which case it needs to aggregate capability from the next component into its own reported capability**


### Usage Summary
| Scope | Control | Type | GET | SET |
| ----- | ----- | ----- | ----- | ----- |
| Version 1	| Filter | Synchronous| ✔️ | ✔️ |

***For GET call***: the KSPROPERTY payload follows the same layout as traditional KSPROPERTY_CAMERACONTROL_EXTENDED_PROPERTY, comprised of a [KSCAMERA_EXTENDEDPROP_HEADER](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ksmedia/ns-ksmedia-tagkscamera_extendedprop_header) followed by a [KSCAMERA_EXTENDEDPROP_VALUE](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ksmedia/ns-ksmedia-tagkscamera_extendedprop_value). 
The capability field defines which supported SET notifications to receive.

***For SET call***: the KSPROPERTY payload starts in the same layout as traditional KSPROPERTY_CAMERACONTROL_EXTENDED_PROPERTY, comprised of a [KSCAMERA_EXTENDEDPROP_HEADER](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ksmedia/ns-ksmedia-tagkscamera_extendedprop_header), but is followed by a variable payload content of **size of a single KSProperty SET payload linked to the KSCAMERA_WINDOWSSTUDIO_SETNOTIFICATION_\* flags value**.

~~~cpp
// SetNotification possible flags values
#define KSCAMERA_WINDOWSSTUDIO_SETNOTIFICATION_NONE 0x0000000000000000
#define KSCAMERA_WINDOWSSTUDIO_SETNOTIFICATION_DIGITALWINDOW 0x0000000000000001
#define KSCAMERA_WINDOWSSTUDIO_SETNOTIFICATION_EYECORRECTION 0x0000000000000002
#define KSCAMERA_WINDOWSSTUDIO_SETNOTIFICATION_BACKGROUNDSEGMENTATION 0x0000000000000004
#define KSCAMERA_WINDOWSSTUDIO_SETNOTIFICATION_STAGELIGHT 0x0000000000000008
#define KSCAMERA_WINDOWSSTUDIO_SETNOTIFICATION_CREATIVEFILTER 0x0000000000000010
~~~

#### KSCAMERA_EXTENDEDPROP_HEADER
| **Member** | **Description** |
| ----- | ----- |
| **Version** |	Must be 1 |
| **PinId**	| This must be KSCAMERA_EXTENDEDPROP_FILTERSCOPE (0xFFFFFFFF). |
| **Size**	| sizeof(KSCAMERA_EXTENDEDPROP_HEADER) + sizeof(KSCAMERA_EXTENDEDPROP_VALUE) |
| **Result** | Unused, must be 0 |
| **Capability** | ***GET call***: Bitmask of notification supported **(KSCAMERA_WINDOWSSTUDIO_ SETNOTIFICATION_\*) for this component and all the subsequent ones up to this point**. Must at least contain a different valid potential flag value than KSCAMERA_WINDOWSSTUDIO_SETNOTIFICATION_NONE. <br /> ***SET call***: Unused, must be 0
 |
| **Flags** | ***GET call***: Unused, must be 0. <br /> ***SET call***: One of the KSCAMERA_WINDOWSSTUDIO_ SETNOTIFICATION_* value other than KSCAMERA_WINDOWSSTUDIO_SETNOTIFICATION_NONE.
 |

 ----------
## <a id="KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_PERFORMANCEMITIGATION"></a>KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_PERFORMANCEMITIGATION Control
This control act as a polling mechanism to identify if the current camera opted into *WSE* can be subject to means of performance mitigation when experiencing for example a dip in framerate due to compute resource contention causing frame processing to take longer than the frame timing deadline. In effect this can take the shape of degrading slightly the quality of an effect in an attempt to lighten the computational load. Having this control allows an app to preemptively advertise this cue to the user as well as query the current state of such mitigation.

### Usage Summary
| Scope | Control | Type | GET | SET |
| ----- | ----- | ----- | ----- | ----- |
| Version 1	| Filter | Synchronous| ✔️ | ❌ |

The SET call of this control will fail.

The KSPROPERTY payload follows the same layout as traditional KSPROPERTY_CAMERACONTROL_EXTENDED_PROPERTY, comprised of a [KSCAMERA_EXTENDEDPROP_HEADER](https://learn.microsoft.com/en-us/windows-hardware/drivers/ddi/ksmedia/ns-ksmedia-tagkscamera_extendedprop_header) followed by a content of size of *sizeof(KSPROPERTY) \** ***n***
where ***“n”*** is the count of KSPROPERTY reported.

~~~cpp
// PerformanceMitigation possible flags values
#define KSCAMERA_WINDOWSSTUDIO_PERFORMANCEMITIGATION_NONE 0x0000000000000000
#define KSCAMERA_WINDOWSSTUDIO_PERFORMANCEMITIGATION_EYEGAZECORRECTION_STARE 0x0000000000000001
~~~

#### KSCAMERA_EXTENDEDPROP_HEADER
| **Member** | **Description** |
| ----- | ----- |
| **Version** |	Must be 1 |
| **PinId**	| This must be KSCAMERA_EXTENDEDPROP_FILTERSCOPE (0xFFFFFFFF).
| **Size**	| sizeof(KSCAMERA_EXTENDEDPROP_HEADER) + n * sizeof(KSPROPERTY) where “n” is the count of KSPROPERTY reported |
| **Result** | Unused, must be 0
| **Capability** | ***GET call:*** Bitmask of supported flag values (KSCAMERA_WINDOWSSTUDIO_PERFORMANCEMITIGATION_*). Must at least contain a different valid potential flag value than KSCAMERA_WINDOWSSTUDIO_PERFORMANCEMITIGATION_NONE.
| **Flags** | ***GET call:*** Bitmask of current flag value in operation **(KSCAMERA_WINDOWSSTUDIO_PERFORMANCEMITIGATION_\*)**. For example, if the flags value is KSCAMERA_WINDOWSSTUDIO_PERFORMANCEMITIGATION_NONE, then it means no performance mitigation are currently applied.