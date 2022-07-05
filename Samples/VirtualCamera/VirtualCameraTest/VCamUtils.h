//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#pragma once

#ifndef VCAMUTILS_H
#define VCAMUTILS_H

static const size_t GUID_STRING_LENGTH = 39;
// Custom DEVPKEYs
// {6AC1FBF7-45F7-4E06-BDA7-F817EBFA04D1}
DEFINE_DEVPROPKEY(DEVPKEY_DeviceInterface_VCamCreate_SourceId,   0x6ac1fbf7, 0x45f7, 0x4e06, 0xbd, 0xa7, 0xf8, 0x17, 0xeb, 0xfa, 0x4, 0xd1, 4); /* DEVPROP_TYPE_STRING */
DEFINE_DEVPROPKEY(DEVPKEY_DeviceInterface_VCamCreate_FriendlyName, 0x6ac1fbf7, 0x45f7, 0x4e06, 0xbd, 0xa7, 0xf8, 0x17, 0xeb, 0xfa, 0x4, 0xd1, 5); /* DEVPROP_TYPE_STRING */
DEFINE_DEVPROPKEY(DEVPKEY_DeviceInterface_VCamCreate_Lifetime, 0x6ac1fbf7, 0x45f7, 0x4e06, 0xbd, 0xa7, 0xf8, 0x17, 0xeb, 0xfa, 0x4, 0xd1, 6); /* DEVPROP_TYPE_INT32 */
DEFINE_DEVPROPKEY(DEVPKEY_DeviceInterface_VCamCreate_Access, 0x6ac1fbf7, 0x45f7, 0x4e06, 0xbd, 0xa7, 0xf8, 0x17, 0xeb, 0xfa, 0x4, 0xd1, 7); /* DEVPROP_TYPE_INT32 */

class VCamUtils
{
public:
    
    static HRESULT GetDeviceInterfaceProperty(
        _In_z_ LPCWSTR pwszDevSymLink,
        _In_ const DEVPROPKEY& Key,
        _In_ DEVPROPTYPE requiredType,
        _Out_ wistd::unique_ptr<BYTE[]>& spBuffer,
        _Out_ ULONG& cbData);

    static HRESULT GetDeviceInterfaceRegistryEntry(
        _In_ LPCWSTR pwszDevSymLink,
        _In_ LPCWSTR pwszKeyName,
        _Out_ wistd::unique_ptr<BYTE[]>& spBuffer,
        _Out_ ULONG& cbData);

    static bool IsVirtualCamera(winrt::hstring const& strDevSymLink);

    static HRESULT RegisterVirtualCamera(
        _In_ winrt::hstring const& strMediaSource, 
        _In_ winrt::hstring const& strFriendlyName,
        _In_ winrt::hstring const& strPhysicalCamSymLink,
        _In_ MFVirtualCameraLifetime vcamLifetime,
        _In_ MFVirtualCameraAccess vcamAccess,
        _In_opt_ IMFAttributes* pAttributes,
        _Outptr_ IMFVirtualCamera** ppVirtualCamera);
        
    static HRESULT UnInstallVirtualCamera(_In_ winrt::hstring const& strDevSymLink);
    static HRESULT UnInstallDevice(IMFVirtualCamera* pVirtualCamera);
    static HRESULT UnInstallDeviceWin32(const wchar_t* pwszDeviceSymLink);
    static HRESULT MSIUninstall(GUID const& clsid);

    static HRESULT GetCameraActivate(const wchar_t* pwszSymLink, IMFActivate** ppActivate);
    static HRESULT InitializeVirtualCamera(const wchar_t* pwszSymLink, IMFMediaSource** ppMediaSource);
    static HRESULT GetVirtualCamera(std::vector<DeviceInformation>& vcamList);
    static HRESULT GetPhysicalCameras(std::vector<DeviceInformation>& physicalCamList);
};

#endif

