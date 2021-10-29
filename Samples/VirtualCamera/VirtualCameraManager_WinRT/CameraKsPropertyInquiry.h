// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once
#include "CameraKsPropertyInquiry.g.h"


namespace winrt::VirtualCameraManager_WinRT::implementation
{
    struct CameraKsPropertyInquiry : CameraKsPropertyInquiryT<CameraKsPropertyInquiry>
    {
        CameraKsPropertyInquiry() = default;

        static winrt::VirtualCameraManager_WinRT::IExtendedPropertyPayload GetExtendedControl(winrt::VirtualCameraManager_WinRT::ExtendedControlKind const& extendedControlKind, winrt::Windows::Media::Devices::VideoDeviceController const& controller);
        static void SetExtendedControlFlags(winrt::VirtualCameraManager_WinRT::ExtendedControlKind const& extendedControlKind, winrt::Windows::Media::Devices::VideoDeviceController const& controller, uint64_t flags);
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
    struct CameraKsPropertyInquiry : CameraKsPropertyInquiryT<CameraKsPropertyInquiry, implementation::CameraKsPropertyInquiry>
    {
    };
}
