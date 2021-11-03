// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once
#include "VirtualCameraRegistrar.g.h"
#include "winrt/Windows.Devices.Enumeration.h"

namespace winrt::VirtualCameraManager_WinRT::implementation
{
    struct VirtualCameraRegistrar : VirtualCameraRegistrarT<VirtualCameraRegistrar>
    {
        VirtualCameraRegistrar() = delete;

        static winrt::VirtualCameraManager_WinRT::VirtualCameraProxy RegisterNewVirtualCamera(
            winrt::VirtualCameraManager_WinRT::VirtualCameraKind const& virtualCameraKind, 
            winrt::VirtualCameraManager_WinRT::VirtualCameraLifetime const& lifetime, 
            winrt::VirtualCameraManager_WinRT::VirtualCameraAccess const& access, 
            hstring const& friendlyName,
            hstring const& wrappedCameraSymbolicLink);

        static winrt::VirtualCameraManager_WinRT::VirtualCameraProxy RetakeExistingVirtualCamera(
            VirtualCameraKind virtualCameraKind,
            VirtualCameraLifetime lifetime,
            VirtualCameraAccess access,
            hstring const& friendlyName,
            hstring const& symbolicLink,
            hstring const& wrappedCameraSymbolicLink);

        static winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Devices::Enumeration::DeviceInformation> GetExistingVirtualCameraDevices();
    };
}

namespace winrt::VirtualCameraManager_WinRT::factory_implementation
{
    struct VirtualCameraRegistrar : VirtualCameraRegistrarT<VirtualCameraRegistrar, implementation::VirtualCameraRegistrar>
    {
    };
}
