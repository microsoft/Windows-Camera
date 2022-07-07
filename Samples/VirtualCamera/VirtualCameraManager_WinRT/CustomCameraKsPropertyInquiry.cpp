// Copyright (C) Microsoft Corporation. All rights reserved.

#include "pch.h"
#include "Common.h"
#include "CustomCameraKsPropertyInquiry.h"
#include "CustomCameraKsPropertyInquiry.g.cpp"
#include "SimpleCustomPropertyPayload.h"
#include "BasicAugmentedMediaSourceCustomPropertyPayload.h"


namespace winrt::VirtualCameraManager_WinRT::implementation
{
    /// <summary>
    /// Getting a custom control payload
    /// </summary>
    /// <param name="customControlKind"></param>
    /// <param name="controller"></param>
    /// <returns></returns>
    winrt::VirtualCameraManager_WinRT::ISimpleCustomPropertyPayload CustomCameraKsPropertyInquiry::GetSimpleCustomControl(
        winrt::VirtualCameraManager_WinRT::SimpleCustomControlKind const& customControlKind, 
        winrt::Windows::Media::Devices::VideoDeviceController const& controller)
    {
        winrt::VirtualCameraManager_WinRT::ISimpleCustomPropertyPayload resultPayload = nullptr;

        uint32_t maxSize = 65536;

        hstring propertyId = L"";
        UINT controlId = 0;
        switch (customControlKind)
        {
            case winrt::VirtualCameraManager_WinRT::SimpleCustomControlKind::Coloring:
                propertyId = winrt::to_hstring(PROPSETID_SIMPLEMEDIASOURCE_CUSTOMCONTROL);
                controlId = static_cast<int>(customControlKind);
                break;
            default:
                throw winrt::hresult_invalid_argument(L"Attempting to get a custom control unsupported in this sample");
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
            throw hresult_invalid_argument(L"Could not retrieve custom device control " 
                + winrt::to_hstring(controlId) + L" with status: " + winrt::to_hstring((int)getResult.Status()));
        }

        // if we succeed in retrieving the specified control, encapsulate the property payload and return it
        auto result = getResult.Value();
        Windows::Foundation::IPropertyValue property = nullptr;
        property = result.as<Windows::Foundation::IPropertyValue>();

        resultPayload = make<SimpleCustomPropertyPayload>(property, customControlKind);

        return resultPayload;
    }

    /// <summary>
    /// Setting a custom control value
    /// </summary>
    /// <param name="customControlKind"></param>
    /// <param name="controller"></param>
    /// <param name="flags"></param>
    void CustomCameraKsPropertyInquiry::SetSimpleCustomControlFlags(
        winrt::VirtualCameraManager_WinRT::SimpleCustomControlKind const& customControlKind, 
        winrt::Windows::Media::Devices::VideoDeviceController const& controller, 
        uint32_t flags)
    {
        hstring propertyId = L"";
        UINT controlId = 0;
        uint8_t* pValue = nullptr;
        array_view<BYTE> serializedPropertyValue;
        KSPROPERTY_SIMPLEMEDIASOURCE_CUSTOMCONTROL_COLORMODE_S payloadValue{ flags };

        switch (customControlKind)
        {
        case winrt::VirtualCameraManager_WinRT::SimpleCustomControlKind::Coloring:
        {
            propertyId = winrt::to_hstring(PROPSETID_SIMPLEMEDIASOURCE_CUSTOMCONTROL);
            controlId = static_cast<int>(customControlKind);
            pValue = reinterpret_cast<BYTE*>(&payloadValue);
            serializedPropertyValue = array_view<BYTE>(pValue, sizeof(KSPROPERTY_SIMPLEMEDIASOURCE_CUSTOMCONTROL_COLORMODE_S));

            break;
        }

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
            throw hresult_invalid_argument(L"Could not set custom device property flags " 
                + winrt::to_hstring(controlId) + L" with status: " + winrt::to_hstring((int)setResult));
        }
    }

    winrt::VirtualCameraManager_WinRT::IAugmentedMediaSourceCustomPropertyPayload CustomCameraKsPropertyInquiry::GetAugmentedMediaSourceCustomControl(
        winrt::VirtualCameraManager_WinRT::AugmentedMediaSourceCustomControlKind const& customControlKind, 
        winrt::Windows::Media::Devices::VideoDeviceController const& controller)
    {
        winrt::VirtualCameraManager_WinRT::IAugmentedMediaSourceCustomPropertyPayload resultPayload = nullptr;

        uint32_t maxSize = 65536;

        hstring propertyId = L"";
        UINT controlId = 0;
        switch (customControlKind)
        {
            case winrt::VirtualCameraManager_WinRT::AugmentedMediaSourceCustomControlKind::KSPROPERTY_AUGMENTEDMEDIASOURCE_CUSTOMCONTROL_CUSTOMFX:
            {
                propertyId = winrt::to_hstring(PROPSETID_AUGMENTEDMEDIASOURCE_CUSTOMCONTROL);
                controlId = static_cast<int>(customControlKind);
                break;
            }
            default:
                throw winrt::hresult_invalid_argument(L"Attempting to get an augmented media source custom control unsupported in this sample");
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
            throw hresult_invalid_argument(L"Could not retrieve augmented media source custom device control " 
                + winrt::to_hstring(controlId) + L" with status: " + winrt::to_hstring((int)getResult.Status()));
        }

        // if we succeed in retrieving the specified control, encapsulate the property payload and return it
        auto result = getResult.Value();
        Windows::Foundation::IPropertyValue property = nullptr;
        property = result.as<Windows::Foundation::IPropertyValue>();

        resultPayload = make<BasicAugmentedMediaSourceCustomPropertyPayload>(property, customControlKind);

        return resultPayload;
    }

    void CustomCameraKsPropertyInquiry::SetAugmentedMediaSourceCustomControlFlags(
        winrt::VirtualCameraManager_WinRT::AugmentedMediaSourceCustomControlKind const& customControlKind,
        winrt::Windows::Media::Devices::VideoDeviceController const& controller,
        uint64_t flags)
    {
        hstring propertyId = L"";
        UINT controlId = 0;
        uint8_t* pValue = nullptr;
        array_view<uint8_t const> serializedPropertyValue;
        KSPROPERTY_AUGMENTEDMEDIASOURCE_CUSTOMCONTROL_FX value;

        switch (customControlKind)
        {
            case winrt::VirtualCameraManager_WinRT::AugmentedMediaSourceCustomControlKind::KSPROPERTY_AUGMENTEDMEDIASOURCE_CUSTOMCONTROL_CUSTOMFX:
            {
                propertyId = winrt::to_hstring(PROPSETID_AUGMENTEDMEDIASOURCE_CUSTOMCONTROL);
                controlId = static_cast<int>(customControlKind);

                value.Flags = (ULONG)flags;
                pValue = reinterpret_cast<uint8_t*>(&value);
                serializedPropertyValue = array_view<uint8_t const>(pValue, sizeof(KSPROPERTY_AUGMENTEDMEDIASOURCE_CUSTOMCONTROL_FX));

                break;
            }

        default:
            throw winrt::hresult_invalid_argument(L"Attempting to set an augmented media source custom control unhandled in this sample");
        }

        // set the control payload
        auto setResult = controller.SetDevicePropertyById(
            propertyId + L" " + winrt::to_hstring(controlId),
            winrt::Windows::Foundation::PropertyValue::CreateUInt8Array(serializedPropertyValue));

        // if setting the control fails, throw
        if (setResult != Windows::Media::Devices::VideoDeviceControllerSetDevicePropertyStatus::Success)
        {
            throw hresult_invalid_argument(L"Could not set augmented media source custom device property flags " 
                + winrt::to_hstring(controlId) + L" with status: " + winrt::to_hstring((int)setResult));
        }
    }
}
