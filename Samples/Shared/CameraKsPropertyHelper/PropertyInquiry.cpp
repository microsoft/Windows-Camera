// Copyright (c) Microsoft. All rights reserved.

#include "pch.h"
#include "PropertyInquiry.h"
#include "PropertyInquiry.g.cpp"
#include "KSHelper.h"
#include <winrt/Windows.Media.Devices.h>
#include "BasicExtendedPropertyPayload.h"
#include "BackgroundSegmentationPropertyPayload.h"
#include "BackgroundSegmentationMaskMetadata.h"
#include "FaceDetectionMetadata.h"
#include "DigitalWindowMetadata.h"
#include "VidCapVideoProcAmpPropetyPayload.h"
#include "VidProcExtendedPropertyPayload.h"
#include "FieldOfView2ConfigCapsPropertyPayload.h"
#include "WindowsStudioSupportedPropertyPayload.h"
#include "BasicWindowsStudioPropertyPayload.h"
using namespace winrt::Windows::Media::Devices;

namespace winrt::CameraKsPropertyHelper::implementation
{
    /// <summary>
    /// Get the data payload as an IPropertyValue associated with a Get command on an extended camera control
    /// </summary>
    /// <param name="controller"></param>
    /// <param name="controlId"></param>
    /// <returns></returns>
    Windows::Foundation::IPropertyValue GetExtendedCameraControlPayload(Windows::Media::Devices::VideoDeviceController const& controller, int controlId)
    {
        Windows::Foundation::IPropertyValue propertyValueResult = nullptr;

        KSPROPERTY prop{
            KSPROPERTYSETID_ExtendedCameraControl,
            static_cast<ULONG>(controlId) /* KSPROPERTY_CAMERACONTROL_EXTENDED_* */,
            KSPROPERTY_TYPE_GET };
        uint8_t* pProp = reinterpret_cast<uint8_t*>(&prop);

        array_view<uint8_t const> serializedProp = array_view<uint8_t const>(pProp, sizeof(KSPROPERTY));
        auto getResult = controller.GetDevicePropertyByExtendedId(serializedProp, nullptr);
        if (getResult.Status() == Windows::Media::Devices::VideoDeviceControllerGetDevicePropertyStatus::NotSupported)
        {
            return propertyValueResult;
        }
        if (getResult.Status() != Windows::Media::Devices::VideoDeviceControllerGetDevicePropertyStatus::Success)
        {
            throw hresult_invalid_argument(L"Could not retrieve device property " + winrt::to_hstring(controlId) + L" with status: " + winrt::to_hstring((int)getResult.Status()));
        }
        auto getResultValue = getResult.Value();
        propertyValueResult = getResultValue.as<Windows::Foundation::IPropertyValue>();

        return propertyValueResult;
    }

    /// <summary>
    /// Get the data payload of a Windows Studio camera control and do something with it
    /// </summary>
    /// <param name="controller"></param>
    /// <param name="controlId"></param>
    /// <returns></returns>
    Windows::Foundation::IPropertyValue GetWindowsStudioCameraControlPayload(
        Windows::Media::Devices::VideoDeviceController const& controller,
        int controlId)
    {
        Windows::Foundation::IPropertyValue propertyValueResult = nullptr;

        KSPROPERTY prop = 
        {
            KSPROPERTYSETID_WindowsCameraEffect, // Set
            (ULONG)controlId, // Id
            0x00000001 /* KSPROPERTY_TYPE_GET */ // Flags
        };
        uint8_t* pProp = reinterpret_cast<uint8_t*>(&prop);

        uint32_t maxSize = 256;
        Windows::Foundation::IReference<uint32_t> maxSizeRef(maxSize);

        array_view<uint8_t const> serializedProp = array_view<uint8_t const>(pProp, sizeof(KSPROPERTY));
        auto getResult = controller.GetDevicePropertyByExtendedId(serializedProp, maxSizeRef);
        if (getResult.Status() == Windows::Media::Devices::VideoDeviceControllerGetDevicePropertyStatus::NotSupported)
        {
            return propertyValueResult;
        }
        if (getResult.Status() != Windows::Media::Devices::VideoDeviceControllerGetDevicePropertyStatus::Success)
        {
            throw hresult_invalid_argument(L"Could not retrieve Windows Studio device property " + winrt::to_hstring(controlId) + L" with status: " + winrt::to_hstring((int)getResult.Status()));
        }
        auto getResultValue = getResult.Value();
        propertyValueResult = getResultValue.as<Windows::Foundation::IPropertyValue>();

        return propertyValueResult;
    }

    /// <summary>
    /// GET call issued for a VidCapVideoProcAmp
    /// </summary>
    /// <param name="controller"></param>
    /// <param name="vidCapVideoProcAmpKind"></param>
    /// <returns></returns>
    winrt::CameraKsPropertyHelper::IVidCapVideoProcAmpPropetyPayload PropertyInquiry::GetVidCapVideoProcAmp(
        winrt::Windows::Media::Devices::VideoDeviceController const& controller,
        winrt::CameraKsPropertyHelper::VidCapVideoProcAmpKind const& vidCapVideoProcAmpKind)
    {
        Windows::Foundation::IPropertyValue supportPropertyValueResult = nullptr;
        Windows::Foundation::IPropertyValue valuePropertyValueResult = nullptr;
        IVidCapVideoProcAmpPropetyPayload payload = nullptr;

        // get support
        
        {
            KSPROPERTY_VIDEOPROCAMP_S ksSupportProp
            {
                 KSPROPERTY{
                  PROPSETID_VIDCAP_VIDEOPROCAMP,
                  static_cast<ULONG>(vidCapVideoProcAmpKind),
                  KSPROPERTY_TYPE_BASICSUPPORT},
                  0
            };
            uint8_t* pksSupportProp = reinterpret_cast<uint8_t*>(&ksSupportProp);
            array_view<uint8_t const> serializedSupportProp = array_view<uint8_t const>(pksSupportProp, sizeof(KSPROPERTY_VIDEOPROCAMP_S));

            uint32_t maxSize = sizeof(VIDEOPROCAMP_MEMBERSLIST);
            Windows::Foundation::IReference<uint32_t> maxSizeRef(maxSize);

            auto getSupportResult = controller.GetDevicePropertyByExtendedId(serializedSupportProp, maxSizeRef);
            if (getSupportResult.Status() == Windows::Media::Devices::VideoDeviceControllerGetDevicePropertyStatus::NotSupported)
            {
                return payload;
            }
            if (getSupportResult.Status() != Windows::Media::Devices::VideoDeviceControllerGetDevicePropertyStatus::Success)
            {
                throw hresult_invalid_argument(L"Could not retrieve device property " + winrt::to_hstring((int)vidCapVideoProcAmpKind) + L" support set with status: " + winrt::to_hstring((int)getSupportResult.Status()));
            }
            auto getSupportResultValue = getSupportResult.Value();
            supportPropertyValueResult = getSupportResultValue.as<Windows::Foundation::IPropertyValue>();
            if (supportPropertyValueResult == nullptr)
            {
                return payload;
            }
        }
        // get current value
        //
        {
            KSPROPERTY_VIDEOPROCAMP_S ksValueProp
            {
                KSPROPERTY{
                 PROPSETID_VIDCAP_VIDEOPROCAMP,
                 static_cast<uint32_t>(vidCapVideoProcAmpKind),
                 KSPROPERTY_TYPE_GET },
                 0
            };
            
            uint8_t* pksValueProp = reinterpret_cast<uint8_t*>(&ksValueProp);
            array_view<uint8_t const> serializedValueProp = array_view<uint8_t const>(pksValueProp, sizeof(KSPROPERTY_VIDEOPROCAMP_S));

            uint32_t maxSize = sizeof(KSPROPERTY_VIDEOPROCAMP_S);
            Windows::Foundation::IReference<uint32_t> maxSizeRef(maxSize);

            auto getValueResult = controller.GetDevicePropertyByExtendedId(serializedValueProp, maxSizeRef);
            if (getValueResult.Status() == Windows::Media::Devices::VideoDeviceControllerGetDevicePropertyStatus::NotSupported)
            {
                return payload;
            }
            if (getValueResult.Status() != Windows::Media::Devices::VideoDeviceControllerGetDevicePropertyStatus::Success)
            {
                throw hresult_invalid_argument(L"Could not retrieve device property " + winrt::to_hstring((int)vidCapVideoProcAmpKind) + L" current values with status: " + winrt::to_hstring((int)getValueResult.Status()));
            }
            auto getValueResultValue = getValueResult.Value();
            valuePropertyValueResult = getValueResultValue.as<Windows::Foundation::IPropertyValue>();
            if (valuePropertyValueResult == nullptr)
            {
                return payload;
            }
        }

        payload = make<VidCapVideoProcAmpPropetyPayload>(supportPropertyValueResult, valuePropertyValueResult, vidCapVideoProcAmpKind);

        return payload;
    }

    /// <summary>
    /// SET call issued for a VidCapVideoProcAmp
    /// </summary>
    /// <param name="controller"></param>
    /// <param name="vidCapVideoProcAmpKind"></param>
    /// <param name="value"></param>
    void PropertyInquiry::SetVidCapVideoProcAmpValue(
        winrt::Windows::Media::Devices::VideoDeviceController const& controller,
        winrt::CameraKsPropertyHelper::VidCapVideoProcAmpKind const& vidCapVideoProcAmpKind,
        double value)
    {
        int controlId = static_cast<int>(vidCapVideoProcAmpKind);

        KSPROPERTY_VIDEOPROCAMP_S ksValueProp
        {
            KSPROPERTY{
              PROPSETID_VIDCAP_VIDEOPROCAMP,
              static_cast<ULONG>(vidCapVideoProcAmpKind),
              KSPROPERTY_TYPE_SET },
              (LONG)value,
              KSPROPERTY_VIDEOPROCAMP_FLAGS_MANUAL
        };

        uint8_t* pksValueProp = reinterpret_cast<uint8_t*>(&ksValueProp);
        array_view<uint8_t const> serializedValueProp = array_view<uint8_t const>(pksValueProp, sizeof(KSPROPERTY_VIDEOPROCAMP_S));

        auto setResult = controller.SetDevicePropertyByExtendedId(serializedValueProp, serializedValueProp);

        if (setResult != Windows::Media::Devices::VideoDeviceControllerSetDevicePropertyStatus::Success)
        {
            throw hresult_invalid_argument(L"Could not set device property flags " + winrt::to_hstring(controlId) + L" with status: " + winrt::to_hstring((int)setResult));
        }
    }

    /// <summary>
    /// Return the result of a GET command of a supported extended control onto the specified VideoDeviceController
    /// </summary>
    /// <param name="controller"></param>
    /// <param name="extendedControlKind"></param>
    /// <returns></returns>
    winrt::CameraKsPropertyHelper::IExtendedPropertyHeader PropertyInquiry::GetExtendedControl(winrt::Windows::Media::Devices::VideoDeviceController const& controller, winrt::CameraKsPropertyHelper::ExtendedControlKind const& extendedControlKind)
    {
        auto propertyValueResult = GetExtendedCameraControlPayload(controller, static_cast<int>(extendedControlKind));
        if (propertyValueResult == nullptr)
        {
            return nullptr;
        }

        IExtendedPropertyHeader payload = nullptr;

        switch (extendedControlKind)
        {
            case ExtendedControlKind::KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION:
            {
                auto container = winrt::com_array<uint8_t>(sizeof(KSCAMERA_EXTENDEDPROP_HEADER));
                propertyValueResult.GetUInt8Array(container);
                KSCAMERA_EXTENDEDPROP_HEADER* payloadHeader = reinterpret_cast<KSCAMERA_EXTENDEDPROP_HEADER*>(&container[0]);
                if ((payloadHeader->Capability & KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_MASK) != 0)
                {
                    payload = make<BackgroundSegmentationPropertyPayload>(propertyValueResult);
                    break;
                }
                // else fallthrough, BackgroundSegmentation control not implementing mask metadata may return a basic property payload per first draft of the DDI spec
            }  

            case ExtendedControlKind::KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION: // fallthrough
            case ExtendedControlKind::KSPROPERTY_CAMERACONTROL_EXTENDED_FIELDOFVIEW2:
            {
                payload = make<BasicExtendedPropertyPayload>(propertyValueResult, extendedControlKind);
                break;
            }
            
            case ExtendedControlKind::KSPROPERTY_CAMERACONTROL_EXTENDED_FRAMERATE_THROTTLE:
            {
                payload = make<VidProcExtendedPropertyPayload>(propertyValueResult, extendedControlKind);
                break;
            }

            case ExtendedControlKind::KSPROPERTY_CAMERACONTROL_EXTENDED_FIELDOFVIEW2_CONFIGCAPS:
            {
                payload = make<FieldOfView2ConfigCapsPropertyPayload>(propertyValueResult);
                break;
            }

            default:
                throw hresult_invalid_argument(L"unexpected extendedControlKind passed: " + to_hstring((int32_t)extendedControlKind));
        }

        return payload;
    }

    /// <summary>
    /// Send a SET command of a supported extended control onto the specified VideoDeviceController with the specified flags value
    /// </summary>
    /// <param name="controller"></param>
    /// <param name="extendedControlKind"></param>
    /// <param name="flags"></param>
    void PropertyInquiry::SetExtendedControlFlags(
        Windows::Media::Devices::VideoDeviceController const& controller,
        winrt::CameraKsPropertyHelper::ExtendedControlKind const& extendedControlKind,
        uint64_t flags)
    {
        uint32_t controlId = static_cast<uint32_t>(extendedControlKind);

        KSPROPERTY prop{
            KSPROPERTYSETID_ExtendedCameraControl,
            controlId /* KSPROPERTY_CAMERACONTROL_EXTENDED_* */,
            KSPROPERTY_TYPE_SET };
        uint8_t* pProp = reinterpret_cast<uint8_t*>(&prop);
        array_view<uint8_t const> serializedProp = array_view<uint8_t const>(pProp, sizeof(KSPROPERTY));
        array_view<uint8_t const> serializedHeader;
        VideoDeviceControllerSetDevicePropertyStatus setResult;

        if (extendedControlKind == ExtendedControlKind::KSPROPERTY_CAMERACONTROL_EXTENDED_FRAMERATE_THROTTLE)
        {
            // retrieve the GET payload and modify the part we want to send a SET with
            // if we are here we assume user has check for support first
            auto  getPayload =
                GetExtendedCameraControlPayload(
                    controller,
                    static_cast<int>(extendedControlKind));
            auto pVidProcGetPayload = PropertyValuePayloadHolder<KsVidProcCameraExtendedPropPayload>(getPayload).Payload();

            pVidProcGetPayload->header.Flags = flags;

            uint8_t* pPayload = reinterpret_cast<uint8_t*>(pVidProcGetPayload);
            serializedHeader = array_view<uint8_t const>(pPayload, pVidProcGetPayload->header.Size);            
            setResult = controller.SetDevicePropertyByExtendedId(serializedProp, serializedHeader);
        }
        else
        {
            KSCAMERA_EXTENDEDPROP_HEADER header =
            {
                1, // Version
                KSCAMERA_EXTENDEDPROP_FILTERSCOPE,
                sizeof(KSCAMERA_EXTENDEDPROP_HEADER) + sizeof(KSCAMERA_EXTENDEDPROP_VALUE), // Size
                0, // Result
                flags, // Flags
                0 // Capability                
            };
            uint8_t* pHeader = reinterpret_cast<uint8_t*>(&header);
            serializedHeader = array_view<uint8_t const>(pHeader, header.Size);
            setResult = controller.SetDevicePropertyByExtendedId(serializedProp, serializedHeader);
        }

        if (setResult != Windows::Media::Devices::VideoDeviceControllerSetDevicePropertyStatus::Success)
        {
            throw hresult_invalid_argument(L"Could not set device property flags " + winrt::to_hstring(controlId) + L" with status: " + winrt::to_hstring((int)setResult));
        }
    }

    /// <summary>
    /// Send a SET command of a supported extended control onto the specified VideoDeviceController with the specified flags value
    /// </summary>
    /// <param name="controller"></param>
    /// <param name="extendedControlKind"></param>
    /// <param name="flags"></param>
    /// <param name="value"></param>
    void PropertyInquiry::SetExtendedControlFlagsAndValue(
        Windows::Media::Devices::VideoDeviceController const& controller,
        winrt::CameraKsPropertyHelper::ExtendedControlKind const& extendedControlKind,
        uint64_t flags,
        uint64_t value)
    {
        int controlId = static_cast<int>(extendedControlKind);

        KSPROPERTY prop = 
        {
            KSPROPERTYSETID_ExtendedCameraControl, // Set
            (ULONG)controlId /* Id = KSPROPERTY_CAMERACONTROL_EXTENDED_* */,
            0x00000002 /* Flags = KSPROPERTY_TYPE_SET */
        };
        uint8_t* pProp = reinterpret_cast<uint8_t*>(&prop);
        array_view<uint8_t const> serializedProp = array_view<uint8_t const>(pProp, sizeof(KSPROPERTY));
        array_view<uint8_t const> serializedHeader;
        Windows::Media::Devices::VideoDeviceControllerSetDevicePropertyStatus setResult;

        switch (extendedControlKind)
        {
        case ExtendedControlKind::KSPROPERTY_CAMERACONTROL_EXTENDED_FRAMERATE_THROTTLE:
        {
            // retrieve the GET payload and modify the part we want to send a SET with
            // if we are here we assume user has check for support first
            auto  getPayload =
                GetExtendedCameraControlPayload(
                    controller,
                    static_cast<int>(extendedControlKind));
            auto pVidProcGetPayload = PropertyValuePayloadHolder<KsVidProcCameraExtendedPropPayload>(getPayload).Payload();

            pVidProcGetPayload->header.Flags = flags;
            pVidProcGetPayload->vidProcSetting.VideoProc.Value.ul = (ULONG)value;
            uint8_t* pPayload = reinterpret_cast<uint8_t*>(pVidProcGetPayload);
            serializedHeader = array_view<uint8_t const>(pPayload, pVidProcGetPayload->header.Size);
            setResult = controller.SetDevicePropertyByExtendedId(serializedProp, serializedHeader);
            break;
        }
        case ExtendedControlKind::KSPROPERTY_CAMERACONTROL_EXTENDED_FIELDOFVIEW2:
        {
            KsBasicCameraExtendedPropPayload payload;
            payload.header = {
                1,          // Version
                0xFFFFFFFF, // PinId = KSCAMERA_EXTENDEDPROP_FILTERSCOPE
                sizeof(KsBasicCameraExtendedPropPayload), // Size
                0, // Result
                0, // Flags
                0  // Capability
            };
            payload.value.Value.ul = (ULONG)value;
            uint8_t* pPayload = reinterpret_cast<uint8_t*>(&payload);
            serializedHeader = array_view<uint8_t const>(pPayload, payload.header.Size);
            setResult = controller.SetDevicePropertyByExtendedId(serializedProp, serializedHeader);
            break;
        }
        default:
        {
            throw hresult_invalid_argument(L"Unsupported extendedControlKind, use SetExtendedControlFlags() or implement new handler");
        }
        }

        if (setResult != Windows::Media::Devices::VideoDeviceControllerSetDevicePropertyStatus::Success)
        {
            throw hresult_invalid_argument(
                L"Could not set extended device property flags=" + winrt::to_hstring(flags) + L" and value=" +
                winrt::to_hstring(value) + L" for controlId=" +
                winrt::to_hstring(controlId) +
                L" with status: " + winrt::to_hstring((int)setResult));
        }
    }

    /// <summary>
    /// Extract the frame metadata of a supported kind from the specified MediaFrameReference
    /// </summary>
    /// <param name="frame"></param>
    /// <param name="metadataKind"></param>
    /// <returns></returns>
    winrt::CameraKsPropertyHelper::IMetadataPayload PropertyInquiry::ExtractFrameMetadata(
        winrt::Windows::Media::Capture::Frames::MediaFrameReference const& frame,
        winrt::CameraKsPropertyHelper::FrameMetadataKind const& metadataKind)
    {
        // redefine here GUIDs present in mfapi.h in case they are not defined in the SDK of the minimum Windows target this project is built against
        static guid captureMetadataGuid = guid(0x2EBE23A8, 0xFAF5, 0x444A, { 0xA6, 0xA2, 0xEB, 0x81, 0x08, 0x80, 0xAB, 0x5D }); // MFSampleExtension_CaptureMetadata
        static guid digitalWindowGuid = guid(0x276f72a2, 0x59c8, 0x4f69, { 0x97, 0xb4, 0x6, 0x8b, 0x8c, 0xe, 0xc0, 0x44 });// MF_CAPTURE_METADATA_DIGITALWINDOW
        static guid backgroundSegmentationMaskGuid = guid(0x3f14dd3, 0x75dd, 0x433a, { 0xa8, 0xe2, 0x1e, 0x3f, 0x5f, 0x2a, 0x50, 0xa0 }); // MF_CAPTURE_METADATA_FRAME_BACKGROUND_MASK
        static guid faceROIsGuid = guid(0x864f25a6, 0x349f, 0x46b1, { 0xa3, 0xe, 0x54, 0xcc, 0x22, 0x92, 0x8a, 0x47 }); // MF_CAPTURE_METADATA_FACEROIS

        CameraKsPropertyHelper::IMetadataPayload result = nullptr;

        if (frame == nullptr)
        {
            throw hresult_invalid_argument(L"null frame");
        }
        auto properties = frame.Properties();
        if (properties != nullptr)
        {
            auto captureMetadata = properties.TryLookup(captureMetadataGuid);
            if (captureMetadata != nullptr)
            {
                auto captureMetadataPropSet = captureMetadata.as<Windows::Foundation::Collections::IMapView<guid, winrt::Windows::Foundation::IInspectable>>();
                if (metadataKind == FrameMetadataKind::MetadataId_DigitalWindow)
                {
                    auto metadata = captureMetadataPropSet.TryLookup(digitalWindowGuid);

                    if (metadata != nullptr)
                    {
                        auto metadataPropVal = metadata.as<Windows::Foundation::IPropertyValue>();
                        result = make<DigitalWindowMetadata>(metadataPropVal);
                    }
                }
                else if (metadataKind == FrameMetadataKind::MetadataId_BackgroundSegmentationMask)
                {
                    auto metadata = captureMetadataPropSet.TryLookup(backgroundSegmentationMaskGuid);

                    if (metadata != nullptr)
                    {
                        auto metadataPropVal = metadata.as<Windows::Foundation::IPropertyValue>();
                        result = make<BackgroundSegmentationMaskMetadata>(metadataPropVal);
                    }
                }
                else if (metadataKind == FrameMetadataKind::MetadataId_FaceROIs)
                {
                    auto metadata = captureMetadataPropSet.TryLookup(faceROIsGuid);

                    if (metadata != nullptr)
                    {
                        auto metadataPropVal = metadata.as<Windows::Foundation::IPropertyValue>();
                        result = make<FaceDetectionMetadata>(metadataPropVal);
                    }
                }
                else
                {
                    throw hresult_not_implemented(L"no handler for FrameMetadataKind = " + to_hstring((int)metadataKind));
                }
            }
            else
            {
                // no metadata present
                return result;
            }
        }
        // else there is no frame metadata present
        return result;
    }

    winrt::CameraKsPropertyHelper::IWindowsStudioPropertyPayload
        PropertyInquiry::GetWindowsStudioControl(
            winrt::Windows::Media::Devices::VideoDeviceController const& controller,
            winrt::CameraKsPropertyHelper::WindowsStudioControlKind const& windowsStudioControlKind)
    {
        auto propertyValueResult = GetWindowsStudioCameraControlPayload(controller, static_cast<int>(windowsStudioControlKind));
        if (propertyValueResult == nullptr)
        {
            return nullptr;
        }

        IWindowsStudioPropertyPayload payload = nullptr;

        switch (windowsStudioControlKind)
        {
        case WindowsStudioControlKind::KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_SUPPORTED:
        {
            auto container = winrt::com_array<uint8_t>(sizeof(KSCAMERA_EXTENDEDPROP_HEADER));
            propertyValueResult.GetUInt8Array(container);
            payload = make<WindowsStudioSupportedPropertyPayload>(propertyValueResult, windowsStudioControlKind);
            break;
        }

        case WindowsStudioControlKind::KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_STAGELIGHT: //fallthrough
        case WindowsStudioControlKind::KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_CREATIVEFILTER: //fallthrough
        case WindowsStudioControlKind::KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_SETNOTIFICATION: // fallthrough
        case WindowsStudioControlKind::KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_PERFORMANCEMITIGATION:
        {
            payload = make<BasicWindowsStudioPropertyPayload>(propertyValueResult, windowsStudioControlKind);
            break;
        }


        default:
            throw hresult_invalid_argument(L"unexpected windowsStudioControlKind passed: " + to_hstring((int32_t)windowsStudioControlKind));
        }

        return payload;
    }

    void PropertyInquiry::SetWindowsStudioControlFlags(
        winrt::Windows::Media::Devices::VideoDeviceController const& controller,
        winrt::CameraKsPropertyHelper::WindowsStudioControlKind const& windowsStudioControlKind,
        uint64_t flags)
    {
        int controlId = static_cast<int>(windowsStudioControlKind);

        KSPROPERTY prop = 
        {
            KSPROPERTYSETID_WindowsCameraEffect, // Set
            (ULONG)controlId /* KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_* */, // Id
            0x00000002 /* KSPROPERTY_TYPE_SET */ // Flags
        };
        uint8_t* pProp = reinterpret_cast<uint8_t*>(&prop);
        array_view<uint8_t const> serializedProp = array_view<uint8_t const>(pProp, sizeof(KSPROPERTY));

        KSCAMERA_EXTENDEDPROP_HEADER header =
        {
            1, // Version
            0xFFFFFFFF, // PinId = KSCAMERA_EXTENDEDPROP_FILTERSCOPE
            sizeof(KSCAMERA_EXTENDEDPROP_HEADER) + sizeof(KSCAMERA_EXTENDEDPROP_VALUE), // Size
            0, // Result
            flags, // Flags
            0 // Capability
        };
        uint8_t* pHeader = reinterpret_cast<uint8_t*>(&header);
        array_view<uint8_t const> serializedHeader = array_view<uint8_t const>(pHeader, header.Size);

        auto setResult = controller.SetDevicePropertyByExtendedId(serializedProp, serializedHeader);

        if (setResult != Windows::Media::Devices::VideoDeviceControllerSetDevicePropertyStatus::Success)
        {
            throw hresult_invalid_argument(L"Could not set Windows Studio device property flags for control id=" + winrt::to_hstring(controlId) + L" with status: " + winrt::to_hstring((int)setResult));
        }
    }
}
