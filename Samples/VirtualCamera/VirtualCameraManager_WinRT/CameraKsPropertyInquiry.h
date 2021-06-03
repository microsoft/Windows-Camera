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
        static winrt::VirtualCameraManager_WinRT::ICustomPropertyPayload GetCustomControl(winrt::VirtualCameraManager_WinRT::CustomControlKind const& customControlKind, winrt::Windows::Media::Devices::VideoDeviceController const& controller);
        static void SetCustomControlFlags(winrt::VirtualCameraManager_WinRT::CustomControlKind const& customControlKind, winrt::Windows::Media::Devices::VideoDeviceController const& controller, uint64_t flags);
    };
}
namespace winrt::VirtualCameraManager_WinRT::factory_implementation
{
    struct CameraKsPropertyInquiry : CameraKsPropertyInquiryT<CameraKsPropertyInquiry, implementation::CameraKsPropertyInquiry>
    {
    };
}
