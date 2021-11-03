// Copyright (C) Microsoft Corporation. All rights reserved.

#include "pch.h"
#include "VirtualCameraProxy.h"
#include "VirtualCameraProxy.g.cpp"
#include "mfidl.h"

namespace winrt::VirtualCameraManager_WinRT::implementation
{
    // redefinition of const GUID present in mfidl.h not usualy available from UWP
    const GUID MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK = { 0x58f0aad8, 0x22bf, 0x4f8a, 0xbb, 0x3d, 0xd2, 0xc4, 0x97, 0x8c, 0x6e, 0x2f };
    const GUID MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME = { 0x60d0e559, 0x52f8, 0x4fa2, 0xbb, 0xce, 0xac, 0xdb, 0x34, 0xa8, 0xec, 0x1 };

    VirtualCameraProxy::VirtualCameraProxy(
        winrt::VirtualCameraManager_WinRT::VirtualCameraKind const& virtualCameraKind,
        winrt::VirtualCameraManager_WinRT::VirtualCameraLifetime const& lifetime,
        winrt::VirtualCameraManager_WinRT::VirtualCameraAccess const& access,
        hstring const& wrappedCameraSymbolicLink,
        wil::com_ptr_nothrow<IMFVirtualCamera>& spVirtualCamera)
        : m_virtualCameraKind(virtualCameraKind),
          m_lifetime(lifetime),
          m_access(access),
          m_wrappedCameraSymbolicLink(wrappedCameraSymbolicLink),
          m_spVirtualCamera(spVirtualCamera),
          m_isKnownInstance(true)
    {
        
    }

    VirtualCameraProxy::VirtualCameraProxy(
        winrt::VirtualCameraManager_WinRT::VirtualCameraKind const& virtualCameraKind,
        winrt::VirtualCameraManager_WinRT::VirtualCameraLifetime const& lifetime,
        winrt::VirtualCameraManager_WinRT::VirtualCameraAccess const& access,
        hstring const& friendlyName,
        hstring const& symbolicLink,
        hstring const& wrappedCameraSymbolicLink,
        wil::com_ptr_nothrow<IMFVirtualCamera>& spVirtualCamera)
        : VirtualCameraProxy(virtualCameraKind, lifetime, access, wrappedCameraSymbolicLink, spVirtualCamera)
    {
        m_friendlyName = friendlyName;
        m_symLink = symbolicLink;
    }

    VirtualCameraProxy::~VirtualCameraProxy()
    {
        m_spVirtualCamera = nullptr;
    }

    void VirtualCameraProxy::EnableVirtualCamera()
    {
        // start camera
        THROW_IF_FAILED_MSG(m_spVirtualCamera->Start(nullptr),
            "Failed to start camera");

        // get symbolic link name from the virtual camera
        wil::unique_cotaskmem_string pwszSymLink;
        UINT32 cch;
        m_spVirtualCamera->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &pwszSymLink, &cch);
        m_symLink = pwszSymLink.get();

        // get friendly name from the virtual camera
        wil::unique_cotaskmem_string pwszFriendlyName;
        m_spVirtualCamera->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &pwszFriendlyName, &cch);
        auto friendlyNameEdit = std::wstring(pwszFriendlyName.get());

        // remove the virtual camera name tag added by the OS
        std::wstring portionToRemove = L" (Windows Test Virtual Camera)";
        size_t replaceStart = friendlyNameEdit.rfind(portionToRemove);
        if (replaceStart != std::wstring::npos)
        {
            friendlyNameEdit.erase(replaceStart, portionToRemove.size());
        }
        portionToRemove = L" (Windows Virtual Camera)";
        replaceStart = friendlyNameEdit.rfind(portionToRemove);
        if (replaceStart != std::wstring::npos)
        {
            friendlyNameEdit.erase(replaceStart, portionToRemove.size());
        }
        m_friendlyName = friendlyNameEdit;
    }

    void VirtualCameraProxy::DisableVirtualCamera()
    {
        // stop camera
        THROW_IF_FAILED_MSG(m_spVirtualCamera->Stop(),
            "Failed to stop camera");
    }

    void VirtualCameraProxy::RemoveVirtualCamera()
    {
        THROW_IF_NULL_ALLOC_MSG(m_spVirtualCamera.get(), 
            "Failed to remove virtual camera: nullptr");

       // stop and remove camera
        DisableVirtualCamera();
        THROW_IF_FAILED_MSG(m_spVirtualCamera->Remove(),
            "Failed to remove virtual camera");
        THROW_IF_FAILED_MSG(m_spVirtualCamera->Shutdown(),
            "Failed to shutdown virtual camera");
    }
}
