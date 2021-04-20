#pragma once

#ifndef VCAMUTILS_H
#define VCAMUTILS_H

//#include "pch.h"
#include <winrt\Windows.Security.Cryptography.h>
#include <winrt\Windows.Security.Cryptography.Core.h>
#include <winrt\Windows.Storage.Streams.h>

using namespace winrt::Windows::Security::Cryptography;
using namespace winrt::Windows::Security::Cryptography::Core;
using namespace winrt::Windows::Storage::Streams;

static const size_t GUID_STRING_LENGTH = 39;
// {6AC1FBF7-45F7-4E06-BDA7-F817EBFA04D1}
DEFINE_DEVPROPKEY(DEVPKEY_DeviceInterface_VCamCreate_Type,       0x6ac1fbf7, 0x45f7, 0x4e06, 0xbd, 0xa7, 0xf8, 0x17, 0xeb, 0xfa, 0x4, 0xd1, 1); /* DEVPROP_TYPE_UINT32 */
DEFINE_DEVPROPKEY(DEVPKEY_DeviceInterface_VCamCreate_Lifetime,   0x6ac1fbf7, 0x45f7, 0x4e06, 0xbd, 0xa7, 0xf8, 0x17, 0xeb, 0xfa, 0x4, 0xd1, 2); /* DEVPROP_TYPE_UINT32 */
DEFINE_DEVPROPKEY(DEVPKEY_DeviceInterface_VCamCreate_Access,     0x6ac1fbf7, 0x45f7, 0x4e06, 0xbd, 0xa7, 0xf8, 0x17, 0xeb, 0xfa, 0x4, 0xd1, 3); /* DEVPROP_TYPE_UINT32 */
DEFINE_DEVPROPKEY(DEVPKEY_DeviceInterface_VCamCreate_SourceId,   0x6ac1fbf7, 0x45f7, 0x4e06, 0xbd, 0xa7, 0xf8, 0x17, 0xeb, 0xfa, 0x4, 0xd1, 4); /* DEVPROP_TYPE_STRING */
DEFINE_DEVPROPKEY(DEVPKEY_DeviceInterface_VCamCreate_FriendlyName, 0x6ac1fbf7, 0x45f7, 0x4e06, 0xbd, 0xa7, 0xf8, 0x17, 0xeb, 0xfa, 0x4, 0xd1, 5); /* DEVPROP_TYPE_STRING */
DEFINE_DEVPROPKEY(DEVPKEY_DeviceInterface_VCamCreate_Categories, 0x6ac1fbf7, 0x45f7, 0x4e06, 0xbd, 0xa7, 0xf8, 0x17, 0xeb, 0xfa, 0x4, 0xd1, 6); /* DEVPROP_TYPEMOD_ARRAY | DEVPROP_TYPE_BYTE */


// {33B382A2-3BE6-4EF7-B621-FC45962B16DD}
static const GUID VCAMCreate_FriendlyName = { 0x33b382a2, 0x3be6, 0x4ef7, { 0xb6, 0x21, 0xfc, 0x45, 0x96, 0x2b, 0x16, 0xdd } };

// {957FD7E6-310F-4688-8C11-7595693917E5}
static const GUID VCAMCreate_SourceId = { 0x957fd7e6, 0x310f, 0x4688, { 0x8c, 0x11, 0x75, 0x95, 0x69, 0x39, 0x17, 0xe5 } };

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
        _In_ winrt::hstring const& strRealCamSymLink, 
        _In_opt_ IMFAttributes* pAttributes,
        _Outptr_ IMFVirtualCamera** ppVirtualCamera);
        
    static HRESULT UnInstallVirtualCamera(_In_ winrt::hstring const& strDevSymLink);
    static HRESULT UnInstallDevice(IMFVirtualCamera* pVirutalCamera);
    static HRESULT UnInstallDeviceWin32(const wchar_t* pwszDeviceSymLink);
    static HRESULT MSIUninstall(GUID const& clsid);

    static HRESULT GetCameraActivate(const wchar_t* pwszSymLink, IMFActivate** ppActivate);
    static HRESULT InitializeVirtualCamera(const wchar_t* pwszSymLink, IMFMediaSource** ppMediaSource);
    static HRESULT GetVirtualCamera(std::vector<DeviceInformation>& vcamList);
    static HRESULT GetRealCameras(std::vector<DeviceInformation>& realCamList);

};

#endif

