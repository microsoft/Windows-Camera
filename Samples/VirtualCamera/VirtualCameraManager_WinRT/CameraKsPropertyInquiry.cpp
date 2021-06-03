// Copyright (C) Microsoft Corporation. All rights reserved.

#include "pch.h"
#include "Common.h"
#include "CameraKsPropertyInquiry.h"
#include "CameraKsPropertyInquiry.g.cpp"
#include "BasicExtendedPropertyPayload.h"
#include "BasicCustomPropertyPayload.h"


namespace winrt::VirtualCameraManager_WinRT::implementation
{
    /// <summary>
    /// Getting an extended control payload
    /// </summary>
    /// <param name="extendedControlKind"></param>
    /// <param name="controller"></param>
    /// <returns></returns>
    winrt::VirtualCameraManager_WinRT::IExtendedPropertyPayload CameraKsPropertyInquiry::GetExtendedControl(
        winrt::VirtualCameraManager_WinRT::ExtendedControlKind const& extendedControlKind, 
        winrt::Windows::Media::Devices::VideoDeviceController const& controller)
    {
        winrt::VirtualCameraManager_WinRT::IExtendedPropertyPayload resultPayload = nullptr;

        uint32_t maxSize = 65536;

        UINT controlId = 0;
        switch (extendedControlKind)
        {
        case winrt::VirtualCameraManager_WinRT::ExtendedControlKind::EyeGazeCorrection:
            controlId = static_cast<int>(extendedControlKind);
            break;
        default:
            throw winrt::hresult_invalid_argument(L"Attempting to get an extendewd control unhandled in this sample");
        }

        KsProperty prop(
            { 0x1CB79112, 0xC0D2, 0x4213, {0x9C, 0xA6, 0xCD, 0x4F, 0xDB, 0x92, 0x79, 0x72} } /* KSPROPERTYSETID_ExtendedCameraControl */,
            controlId /* KSPROPERTY_CAMERACONTROL_EXTENDED_* */,
            0x00000001 /* KSPROPERTY_TYPE_GET */);
        uint8_t* pProp = reinterpret_cast<uint8_t*>(&prop);

        array_view<uint8_t const> serializedProp = array_view<uint8_t const>(pProp, sizeof(KsProperty));

        // get the control payload
        auto getResult = controller.GetDevicePropertyByExtendedId(serializedProp, nullptr);

        // if the control is not supported, return null
        if (getResult.Status() == Windows::Media::Devices::VideoDeviceControllerGetDevicePropertyStatus::NotSupported)
        {
            return resultPayload;
        }

        // if getting the control fails, throw
        if (getResult.Status() != Windows::Media::Devices::VideoDeviceControllerGetDevicePropertyStatus::Success)
        {
            throw hresult_invalid_argument(L"Could not retrieve extended device control " + winrt::to_hstring(controlId) + L" with status: " + winrt::to_hstring((int)getResult.Status()));
        }

        // if we succeed in retrieving the specified control, encapsulate the property payload and return it
        auto result = getResult.Value();
        Windows::Foundation::IPropertyValue property = nullptr;
        property = result.as<Windows::Foundation::IPropertyValue>();

        resultPayload = make<BasicExtendedPropertyPayload>(property, extendedControlKind);

        return resultPayload;
    }

    /// <summary>
    /// Setting an extended control value
    /// </summary>
    /// <param name="extendedControlKind"></param>
    /// <param name="controller"></param>
    /// <param name="flags"></param>
    void CameraKsPropertyInquiry::SetExtendedControlFlags(
        winrt::VirtualCameraManager_WinRT::ExtendedControlKind const& extendedControlKind, 
        winrt::Windows::Media::Devices::VideoDeviceController const& controller, uint64_t flags)
    {
        winrt::VirtualCameraManager_WinRT::IExtendedPropertyPayload resultPayload = nullptr;

        uint32_t maxSize = 65536;

        UINT controlId = 0;
        switch (extendedControlKind)
        {
        case winrt::VirtualCameraManager_WinRT::ExtendedControlKind::EyeGazeCorrection:
            controlId = static_cast<int>(extendedControlKind);
            break;
        default:
            throw winrt::hresult_invalid_argument(L"Attempting to get an extendewd control unhandled in this sample");
        }

        KsProperty prop(
            { 0x1CB79112, 0xC0D2, 0x4213, {0x9C, 0xA6, 0xCD, 0x4F, 0xDB, 0x92, 0x79, 0x72} } /* KSPROPERTYSETID_ExtendedCameraControl */,
            controlId /* KSPROPERTY_CAMERACONTROL_EXTENDED_* */,
            0x00000002 /* KSPROPERTY_TYPE_SET */);
        uint8_t* pProp = reinterpret_cast<uint8_t*>(&prop);

        array_view<uint8_t const> serializedProp = array_view<uint8_t const>(pProp, sizeof(KsProperty));

        KsCameraExtendedPropHeader value =
        {
            1, // Version
            1, // PinId = KSCAMERA_EXTENDEDPROP_FILTERSCOPE
            sizeof(KsCameraExtendedPropHeader) + sizeof(KsCameraExtendedPropValue), // Size
            0, // Result
            flags, // Flags
            0 // Capability                
        };
        uint8_t* pValue = reinterpret_cast<uint8_t*>(&value);
        array_view<uint8_t const> serializedValue = array_view<uint8_t const>(pValue, sizeof(KsCameraExtendedPropHeader) + sizeof(KsCameraExtendedPropValue));

        // set the control payload
        auto setResult = controller.SetDevicePropertyByExtendedId(serializedProp, serializedValue);

        // if setting the control fails, throw
        if (setResult != Windows::Media::Devices::VideoDeviceControllerSetDevicePropertyStatus::Success)
        {
            throw hresult_invalid_argument(L"Could not set extended device property flags " + winrt::to_hstring(controlId) + L" with status: " + winrt::to_hstring((int)setResult));
        }
    }

    /// <summary>
    /// Getting a custom control payload
    /// </summary>
    /// <param name="customControlKind"></param>
    /// <param name="controller"></param>
    /// <returns></returns>
    winrt::VirtualCameraManager_WinRT::ICustomPropertyPayload CameraKsPropertyInquiry::GetCustomControl(
        winrt::VirtualCameraManager_WinRT::CustomControlKind const& customControlKind, 
        winrt::Windows::Media::Devices::VideoDeviceController const& controller)
    {
        winrt::VirtualCameraManager_WinRT::ICustomPropertyPayload resultPayload = nullptr;

        uint32_t maxSize = 65536;

        hstring propertyId = L"";
        UINT controlId = 0;
        switch (customControlKind)
        {
            case winrt::VirtualCameraManager_WinRT::CustomControlKind::ColorMode:
                propertyId = winrt::to_hstring(PROPSETID_SIMPLEMEDIASOURCE_CUSTOMCONTROL);
                controlId = static_cast<int>(customControlKind);
                break;
            default:
                throw winrt::hresult_invalid_argument(L"Attempting to get a custom control unhandled in this sample");
        }

        // get the control payload
        auto getResult = controller.GetDevicePropertyById(propertyId + L" " + winrt::to_hstring(controlId), maxSize);

        // if the control is not supported, return null
        if (getResult.Status() == Windows::Media::Devices::VideoDeviceControllerGetDevicePropertyStatus::NotSupported)
        {
            return resultPayload;
        }

        // if getting the control fails, throw
        if (getResult.Status() != Windows::Media::Devices::VideoDeviceControllerGetDevicePropertyStatus::Success)
        {
            throw hresult_invalid_argument(L"Could not retrieve custom device control " + winrt::to_hstring(controlId) + L" with status: " + winrt::to_hstring((int)getResult.Status()));
        }

        // if we succeed in retrieving the specified control, encapsulate the property payload and return it
        auto result = getResult.Value();
        Windows::Foundation::IPropertyValue property = nullptr;
        property = result.as<Windows::Foundation::IPropertyValue>();

        resultPayload = make<BasicCustomPropertyPayload>(property, customControlKind);

        return resultPayload;
    }

    /// <summary>
    /// Setting a custom control value
    /// </summary>
    /// <param name="customControlKind"></param>
    /// <param name="controller"></param>
    /// <param name="flags"></param>
    void CameraKsPropertyInquiry::SetCustomControlFlags(
        winrt::VirtualCameraManager_WinRT::CustomControlKind const& customControlKind, 
        winrt::Windows::Media::Devices::VideoDeviceController const& controller, 
        uint64_t flags)
    {
        hstring propertyId = L"";
        UINT controlId = 0;
        uint8_t* pValue = nullptr;
        array_view<uint8_t const> serializedPropertyValue;

        switch (customControlKind)
        {
        case winrt::VirtualCameraManager_WinRT::CustomControlKind::ColorMode:
            propertyId = winrt::to_hstring(PROPSETID_SIMPLEMEDIASOURCE_CUSTOMCONTROL);
            controlId = static_cast<int>(customControlKind);

            KSPROPERTY_SIMPLEMEDIASOURCE_CUSTOMCONTROL_COLORMODE_S value;
            value.ColorMode = flags;
            pValue = reinterpret_cast<uint8_t*>(&value);
            serializedPropertyValue = array_view<uint8_t const>(pValue, sizeof(KSPROPERTY_SIMPLEMEDIASOURCE_CUSTOMCONTROL_COLORMODE_S));

            break;

        default:
            throw winrt::hresult_invalid_argument(L"Attempting to set a custom control unhandled in this sample");
        }

        // set the control payload
        auto setResult = controller.SetDevicePropertyById(
            propertyId + L" " + winrt::to_hstring(controlId),
            winrt::Windows::Foundation::PropertyValue::CreateUInt8Array(serializedPropertyValue));

        // if setting the control fails, throw
        if (setResult != Windows::Media::Devices::VideoDeviceControllerSetDevicePropertyStatus::Success)
        {
            throw hresult_invalid_argument(L"Could not set custom device property flags " + winrt::to_hstring(controlId) + L" with status: " + winrt::to_hstring((int)setResult));
        }
    }
}
