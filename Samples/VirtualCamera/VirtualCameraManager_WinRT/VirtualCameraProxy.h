// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once
#include "VirtualCameraProxy.g.h"
#include <mfvirtualcamera.h>
#include <wil\com.h>

namespace winrt::VirtualCameraManager_WinRT::implementation
{
    struct VirtualCameraProxy : VirtualCameraProxyT<VirtualCameraProxy>
    {        
        VirtualCameraProxy(
            winrt::VirtualCameraManager_WinRT::VirtualCameraKind const& virtualCameraKind,
            winrt::VirtualCameraManager_WinRT::VirtualCameraLifetime const& lifetime,
            winrt::VirtualCameraManager_WinRT::VirtualCameraAccess const& access,
            hstring const& wrappedCameraSymbolicLink,
            wil::com_ptr_nothrow<IMFVirtualCamera>& spVirtualCamera);

        VirtualCameraProxy(
            winrt::VirtualCameraManager_WinRT::VirtualCameraKind const& virtualCameraKind,
            winrt::VirtualCameraManager_WinRT::VirtualCameraLifetime const& lifetime,
            winrt::VirtualCameraManager_WinRT::VirtualCameraAccess const& access,
            hstring const& friendlyName,
            hstring const& symbolicLink,
            hstring const& wrappedCameraSymbolicLink,
            wil::com_ptr_nothrow<IMFVirtualCamera>& spVirtualCamera);

        ~VirtualCameraProxy();

        void EnableVirtualCamera();
        void DisableVirtualCamera();
        void RemoveVirtualCamera();

        VirtualCameraAccess Access() { return m_access; }
        hstring FriendlyName() { return m_friendlyName; }
        hstring WrappedCameraSymbolicLink() { return m_wrappedCameraSymbolicLink; }
        bool IsKnownInstance() { return m_isKnownInstance; }
        VirtualCameraLifetime Lifetime() { return m_lifetime; }
        hstring SymbolicLink() { return m_symLink; }
        VirtualCameraKind VirtualCameraKind() { return m_virtualCameraKind; }

    protected:
        VirtualCameraLifetime m_lifetime;
        VirtualCameraAccess m_access;
        winrt::hstring m_friendlyName;
        winrt::hstring m_symLink;
        bool m_isKnownInstance;
        wil::com_ptr_nothrow<IMFVirtualCamera> m_spVirtualCamera;
        winrt::hstring m_wrappedCameraSymbolicLink;
        winrt::VirtualCameraManager_WinRT::VirtualCameraKind m_virtualCameraKind;
    };
}
