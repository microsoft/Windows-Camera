// Copyright (c) Microsoft. All rights reserved.

#include "pch.h"
#include "PropertyInquiry.h"
#include "PropertyInquiry.g.cpp"
#include "KSHelper.h"
#include <winrt/Windows.Media.Devices.h>
#include "BasicExtendedPropertyPayload.h"
#include "BackgroundSegmentationPropertyPayload.h"
#include "BackgroundSegmentationMaskMetadata.h"


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

        KSPROPERTY prop(
            { 0x1CB79112, 0xC0D2, 0x4213, {0x9C, 0xA6, 0xCD, 0x4F, 0xDB, 0x92, 0x79, 0x72} } /* KSPROPERTYSETID_ExtendedCameraControl */,
            controlId /* KSPROPERTY_CAMERACONTROL_EXTENDED_* */,
            0x00000001 /* KSPROPERTY_TYPE_GET */);
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
    /// Return the result of a GET command of a supported extended control onto the specified VideoDeviceController
    /// </summary>
    /// <param name="controller"></param>
    /// <param name="extendedControlKind"></param>
    /// <returns></returns>
    winrt::CameraKsPropertyHelper::IExtendedPropertyPayload PropertyInquiry::GetExtendedControl(winrt::Windows::Media::Devices::VideoDeviceController const& controller, winrt::CameraKsPropertyHelper::ExtendedControlKind const& extendedControlKind)
    {
        auto propertyValueResult = GetExtendedCameraControlPayload(controller, static_cast<int>(extendedControlKind));
        if (propertyValueResult == nullptr)
        {
            return nullptr;
        }

        winrt::com_array<uint8_t> container;
        IExtendedPropertyPayload payload = nullptr;

        switch (extendedControlKind)
        {
            case ExtendedControlKind::KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION:
            {
                auto container = winrt::com_array<uint8_t>(sizeof(KSCAMERA_EXTENDEDPROP_HEADER));
                propertyValueResult.GetUInt8Array(container);
                KSCAMERA_EXTENDEDPROP_HEADER* payloadHeader = reinterpret_cast<KSCAMERA_EXTENDEDPROP_HEADER*>(&container[0]);
                if ((payloadHeader->Capability & (ULONGLONG)BackgroundSegmentationCapabilityKind::KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_MASK) != 0)
                {
                    payload = make<BackgroundSegmentationPropertyPayload>(propertyValueResult);
                    break;
                }
                // else fallthrough, BackgroundSegmentation control not implementing mask metadata may return a basic property payload per first draft of the DDI spec
            }  

            case ExtendedControlKind::KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION:
            {
                payload = make<BasicExtendedPropertyPayload>(propertyValueResult, extendedControlKind);
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
        int controlId = static_cast<int>(extendedControlKind);
        
        KSPROPERTY prop(
            { 0x1CB79112, 0xC0D2, 0x4213, {0x9C, 0xA6, 0xCD, 0x4F, 0xDB, 0x92, 0x79, 0x72} } /* KSPROPERTYSETID_ExtendedCameraControl */,
            controlId /* KSPROPERTY_CAMERACONTROL_EXTENDED_* */,
            0x00000002 /* KSPROPERTY_TYPE_SET */);
        uint8_t* pProp = reinterpret_cast<uint8_t*>(&prop);
        array_view<uint8_t const> serializedProp = array_view<uint8_t const>(pProp, sizeof(KSPROPERTY));

        KSCAMERA_EXTENDEDPROP_HEADER header =
        {
            1, // Version
            1, // PinId = KSCAMERA_EXTENDEDPROP_FILTERSCOPE
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
            throw hresult_invalid_argument(L"Could not set device property flags " + winrt::to_hstring(controlId) + L" with status: " + winrt::to_hstring((int)setResult));
        }
    }

    /// <summary>
    /// Extract the frame metadata of a supported kind from the specified MediaFrameReference
    /// </summary>
    /// <param name="frame"></param>
    /// <param name="metadataKind"></param>
    /// <returns></returns>
    winrt::CameraKsPropertyHelper::IMetadataPayload PropertyInquiry::ExtractFrameMetadata(winrt::Windows::Media::Capture::Frames::MediaFrameReference const& frame, winrt::CameraKsPropertyHelper::FrameMetadataKind const& metadataKind)
    {
        // redefine here GUIDs present in mfapi.h in case they are not defined in the SDK of the minimum Windows target this project is built against
        static guid captureMetadataGuid = guid(0x2EBE23A8, 0xFAF5, 0x444A, { 0xA6, 0xA2, 0xEB, 0x81, 0x08, 0x80, 0xAB, 0x5D }); // MFSampleExtension_CaptureMetadata
        static guid backgroundSegmentationMaskGuid = guid(0x3f14dd3, 0x75dd, 0x433a, { 0xa8, 0xe2, 0x1e, 0x3f, 0x5f, 0x2a, 0x50, 0xa0 }); // MF_CAPTURE_METADATA_FRAME_BACKGROUND_MASK
        
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
                auto captureMetadata = properties.TryLookup(captureMetadataGuid);
                if (captureMetadata != nullptr)
                {
                    auto captureMetadataPropSet = captureMetadata.as<Windows::Foundation::Collections::IMapView<guid, winrt::Windows::Foundation::IInspectable>>();

                    auto metadata = captureMetadataPropSet.TryLookup(backgroundSegmentationMaskGuid);

                    if (metadata != nullptr)
                    {
                        auto metadataPropVal = metadata.as<Windows::Foundation::IPropertyValue>();
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
}
