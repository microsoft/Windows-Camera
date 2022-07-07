// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once
#include "CustomCameraKsPropertyInquiry.g.h"


namespace winrt::VirtualCameraManager_WinRT::implementation
{
    struct CustomCameraKsPropertyInquiry : CustomCameraKsPropertyInquiryT<CustomCameraKsPropertyInquiry>
    {
        CustomCameraKsPropertyInquiry() = default;

        static winrt::VirtualCameraManager_WinRT::ISimpleCustomPropertyPayload GetSimpleCustomControl(winrt::VirtualCameraManager_WinRT::SimpleCustomControlKind const& customControlKind, winrt::Windows::Media::Devices::VideoDeviceController const& controller);
        static void SetSimpleCustomControlFlags(winrt::VirtualCameraManager_WinRT::SimpleCustomControlKind const& customControlKind, winrt::Windows::Media::Devices::VideoDeviceController const& controller, uint32_t flags);
        static winrt::VirtualCameraManager_WinRT::IAugmentedMediaSourceCustomPropertyPayload GetAugmentedMediaSourceCustomControl(
            winrt::VirtualCameraManager_WinRT::AugmentedMediaSourceCustomControlKind const& customControlKind, 
            winrt::Windows::Media::Devices::VideoDeviceController const& controller);
        static void SetAugmentedMediaSourceCustomControlFlags(
            winrt::VirtualCameraManager_WinRT::AugmentedMediaSourceCustomControlKind const& customControlKind, 
            winrt::Windows::Media::Devices::VideoDeviceController const& controller, 
            uint64_t flags);
    };
}
namespace winrt::VirtualCameraManager_WinRT::factory_implementation
{
    struct CustomCameraKsPropertyInquiry : CustomCameraKsPropertyInquiryT<CustomCameraKsPropertyInquiry, implementation::CustomCameraKsPropertyInquiry>
    {
    };
}
