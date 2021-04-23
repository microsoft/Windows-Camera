//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#include "pch.h"
#include "VCamUtils.h"
#include "MediaCaptureUtils.h"

const winrt::hstring _DEVPKEY_DeviceInterface_FriendlyName(L"{026e516e-b814-414b-83cd-856d6fef4822} 2");
const winrt::hstring _DEVPKEY_DeviceInterface_IsVirtualCamera(L"{6EDC630D-C2E3-43B7-B2D1-20525A1AF120} 3");
const winrt::hstring _DEVPKEY_DeviceInterface_VCamCreate_SourceId(L"{6ac1fbf7-45f7-4e06-bda7-f817ebfa04d1}, 4");
const winrt::hstring _DEVPKEY_DeviceInterface_VCamCreate_FriendlyName(L"{6ac1fbf7-45f7-4e06-bda7-f817ebfa04d1}, 5");

HRESULT VCamUtils::GetDeviceInterfaceProperty(
    _In_z_ LPCWSTR pwszDevSymLink,
    _In_ const DEVPROPKEY& Key,
    _In_ DEVPROPTYPE requiredType,
    _Out_ wistd::unique_ptr<BYTE[]>& spBuffer,
    _Out_ ULONG& cbData
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pwszDevSymLink);
    cbData = 0;

    DEVPROPTYPE     propType = 0;
    ULONG           propSize = 0;
    CONFIGRET cr = CM_Get_Device_Interface_Property(pwszDevSymLink, &Key, &propType, nullptr, &propSize, 0);

    if (cr != CR_BUFFER_SMALL)
    {
        RETURN_IF_FAILED(HRESULT_FROM_WIN32(CM_MapCrToWin32Err(cr, ERROR_INVALID_DATA)));
    }

    if (propType != requiredType)
    {
        wprintf(L"unexpected proptype: %d (expected: %d)\n", propType, requiredType);
        RETURN_IF_FAILED(HRESULT_FROM_WIN32(ERROR_INVALID_DATA));
    }

    if (cr == CR_BUFFER_SMALL)
    {
        spBuffer = wil::make_unique_nothrow<BYTE[]>(propSize);
        cbData = propSize;
        cr = CM_Get_Device_Interface_Property(pwszDevSymLink, &Key, &propType, (PBYTE)spBuffer.get(), &propSize, 0);
    }
    RETURN_IF_FAILED(HRESULT_FROM_WIN32(CM_MapCrToWin32Err(cr, ERROR_INVALID_DATA)));

    return S_OK;
}

HRESULT VCamUtils::GetDeviceInterfaceRegistryEntry(
    _In_ LPCWSTR pwszDevSymLink, 
    _In_ LPCWSTR pwszKeyName, 
    _Out_ wistd::unique_ptr<BYTE[]>& spBuffer,
    _Out_ ULONG& cbData)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pwszDevSymLink);
    RETURN_HR_IF_NULL(E_INVALIDARG, pwszKeyName);

    wil::unique_hkey hKey;
    CONFIGRET cr = CM_Open_Device_Interface_Key(
        pwszDevSymLink,
        KEY_READ,
        RegDisposition_OpenExisting,
        &hKey,
        0);
    RETURN_IF_FAILED_MSG(HRESULT_FROM_WIN32(CM_MapCrToWin32Err(cr, ERROR_INVALID_DATA)), "Failed to get device interface reg key");

    DWORD dwType = 0;
    LONG lResult = RegQueryValueEx(hKey.get(), pwszKeyName, nullptr, &dwType, nullptr, &cbData);
    RETURN_IF_FAILED_MSG(HRESULT_FROM_WIN32(lResult), "Failed to get reg key: %s", pwszKeyName);

    spBuffer = wil::make_unique_nothrow<BYTE[]>(cbData);
    lResult = RegQueryValueEx(hKey.get(), pwszKeyName, nullptr, &dwType, spBuffer.get(), &cbData);
    RETURN_IF_FAILED_MSG(HRESULT_FROM_WIN32(lResult), "Failed to get reg key: %s", pwszKeyName);

    return S_OK;
}

bool VCamUtils::IsVirtualCamera(winrt::hstring const& strDevSymLink)
{
    // ref: https://docs.microsoft.com/en-us/windows/uwp/devices-sensors/device-information-properties
    //custom GUID format{744e3bed-3684-4e16-9f8a-07953a8bf2ab} 7   
    
    bool isVCam = false;
    winrt::Windows::Foundation::IReference<bool> val;
    if (SUCCEEDED(CMediaCaptureUtils::GetDeviceProperty(strDevSymLink, _DEVPKEY_DeviceInterface_IsVirtualCamera, DeviceInformationKind::DeviceInterface, val)))
    {
        if (val != nullptr)
        {
            isVCam = val.Value();
        }
    }
    return isVCam;
}

HRESULT VCamUtils::RegisterVirtualCamera(
    _In_ winrt::hstring const& strMediaSource, 
    _In_ winrt::hstring const& strFriendlyName, 
    _In_ winrt::hstring const& strPhysicalCamSymLink,
    _In_opt_ IMFAttributes* pAttributes,
    _Outptr_ IMFVirtualCamera** ppVirtualCamera)
{
    RETURN_HR_IF_NULL(E_POINTER, ppVirtualCamera);

    LOG_COMMENT(L"Register virtual camera...\n");
    LOG_COMMENT(L"device string: %s \n", strMediaSource.data());
    LOG_COMMENT(L"friendly name: %s \n", strFriendlyName.data());

    MFVirtualCameraType vcamType = MFVirtualCameraType_SoftwareCameraSource;
    MFVirtualCameraLifetime vcamLifetime = MFVirtualCameraLifetime_System;
    MFVirtualCameraAccess vcamAccess = MFVirtualCameraAccess_CurrentUser;

    wil::com_ptr_nothrow<IMFVirtualCamera> spVirtualCamera;
    RETURN_IF_FAILED(MFCreateVirtualCamera(
        vcamType,
        vcamLifetime, 
        vcamAccess,  
        strFriendlyName.data(),
        strMediaSource.data(),
        nullptr, /*Catetgorylist*/
        0,       /*CatetgoryCount*/
        &spVirtualCamera));
    RETURN_HR_IF_NULL_MSG(E_POINTER, spVirtualCamera.get(), "Fail to create virtual camera");

    if(!strPhysicalCamSymLink.empty())
    {
        LOG_COMMENT(L"Add physical cam: %s \n", strPhysicalCamSymLink.data());
        RETURN_IF_FAILED(spVirtualCamera->AddDeviceSourceInfo(strPhysicalCamSymLink.data()));
    }
    
    if (pAttributes)
    {
        uint32_t cItems = 0;
        RETURN_IF_FAILED(pAttributes->GetCount(&cItems));
        LOG_COMMENT(L"Set attributes on virtual cameras: %d items \n", cItems);
        for (uint32_t i = 0; i < cItems; i++)
        {
            wil::unique_prop_variant prop;
            GUID guidKey = GUID_NULL;
            RETURN_IF_FAILED(pAttributes->GetItemByIndex(i, &guidKey, &prop));
            LOG_COMMENT(L"%i - %s \n", i, winrt::to_hstring(guidKey).data());
            RETURN_IF_FAILED(spVirtualCamera->SetItem(guidKey, prop));
        }
    }
    LOG_COMMENT(L"Succeeded (%p)! \n", spVirtualCamera.get());

    try 
    {
        AppInfo appInfo = winrt::Windows::ApplicationModel::AppInfo::Current();
        LOG_COMMENT(L"AppInfo: %p ", appInfo);
        if (appInfo != nullptr)
        {
            LOG_COMMENT(L"PFN: %s \n", appInfo.PackageFamilyName().data());
        }
    }
    catch (...) { LOG_WARN(L"not running in app package\n"); }


    HRESULT hr = spVirtualCamera->AddProperty(
        &DEVPKEY_DeviceInterface_VCamCreate_SourceId, 
        DEVPROP_TYPE_STRING, 
        (BYTE*)strMediaSource.data(), 
        sizeof(winrt::hstring::value_type)*(strMediaSource.size()+1));

    if (hr == HRESULT_FROM_WIN32(ERROR_ACCESS_DENIED))
    {
        LOG_WARN(L"Please run application in elevated mode");
        return hr;
    }
    RETURN_IF_FAILED_MSG(hr, "Fail to addproperty - DEVPKEY_DeviceInterface_VCamCreate_SourceId \n");

    RETURN_IF_FAILED_MSG(spVirtualCamera->AddProperty(
        &DEVPKEY_DeviceInterface_VCamCreate_FriendlyName,
        DEVPROP_TYPE_STRING,
        (BYTE*)strFriendlyName.data(),
        sizeof(winrt::hstring::value_type) * (strFriendlyName.size() + 1)), 
        "Fail to addproperty - DEVPKEY_DeviceInterface_VCamCreate_FriendlyName \n");

    LOG_COMMENT(L"Start camera ");
    RETURN_IF_FAILED(spVirtualCamera->Start(nullptr));
    LOG_COMMENT(L"Succeeded! \n");

    *ppVirtualCamera = spVirtualCamera.detach();

    return S_OK;
}

HRESULT VCamUtils::UnInstallVirtualCamera(
    _In_ winrt::hstring const& strDevSymLink)
{
    LOG_COMMENT(L"Removing device: %s\n", strDevSymLink.data());

    winrt::Windows::Foundation::IReference<winrt::hstring> friendlyName;
    RETURN_IF_FAILED(CMediaCaptureUtils::GetDeviceProperty(strDevSymLink, _DEVPKEY_DeviceInterface_VCamCreate_FriendlyName, DeviceInformationKind::DeviceInterface, friendlyName));
    RETURN_HR_IF_NULL(E_INVALIDARG, friendlyName);
    LOG_COMMENT(L"friendlyname: %s\n", friendlyName.Value().data());

    winrt::Windows::Foundation::IReference<winrt::hstring> sourceId;
    RETURN_IF_FAILED(CMediaCaptureUtils::GetDeviceProperty(strDevSymLink, _DEVPKEY_DeviceInterface_VCamCreate_SourceId, DeviceInformationKind::DeviceInterface, sourceId));
    RETURN_HR_IF_NULL(E_INVALIDARG, sourceId);
    LOG_COMMENT(L"sourceId: %s\n", sourceId.Value().data());

    wil::com_ptr_nothrow<IMFVirtualCamera> spVirtualCamera;
    RETURN_IF_FAILED(MFCreateVirtualCamera(
        MFVirtualCameraType_SoftwareCameraSource,
        MFVirtualCameraLifetime_System,    /*MFVirtualCameraLifetime_Session,*/
        MFVirtualCameraAccess_CurrentUser, /*MFVirtualCameraAccess_System,*/ // access_denied on enabled.
        friendlyName.Value().data(),
        sourceId.Value().data(),
        nullptr, /*Catetgorylist*/
        0,       /*CatetgoryCount*/
        &spVirtualCamera));
    RETURN_HR_IF_NULL_MSG(E_FAIL, spVirtualCamera.get(), "Fail to create virtual camera");

    LOG_COMMENT(L"remove virtual camera ");
    RETURN_IF_FAILED(spVirtualCamera->Remove());
    RETURN_IF_FAILED(spVirtualCamera->Shutdown());
    LOG_COMMENT(L"succeeded! \n");

    return S_OK;
}

HRESULT VCamUtils::UnInstallDevice(IMFVirtualCamera* pVirutalCamera)
{
    wil::unique_cotaskmem_string spwszSymLink;
    UINT32 cch = 0;
    RETURN_IF_FAILED(pVirutalCamera->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &spwszSymLink, &cch));
    RETURN_IF_FAILED(UnInstallDeviceWin32(spwszSymLink.get()));

    return S_OK;
}

HRESULT VCamUtils::UnInstallDeviceWin32(const wchar_t* pwszDeviceSymLink)
{
    LOG_COMMENT(L"Uninstalling device: %s \n", pwszDeviceSymLink);

    wistd::unique_ptr<BYTE[]> spBuffer;
    ULONG cbBufferSize = 0;
    RETURN_IF_FAILED_MSG(VCamUtils::GetDeviceInterfaceProperty(
        pwszDeviceSymLink,
        DEVPKEY_Device_InstanceId,
        DEVPROP_TYPE_STRING,
        spBuffer,
        cbBufferSize), 
        "Fail to get device instanceId");

    DEVINST hDevInstance;
    CONFIGRET cr = CM_Locate_DevNode(&hDevInstance, (wchar_t*)spBuffer.get(), CM_LOCATE_DEVNODE_NORMAL | CM_LOCATE_DEVNODE_PHANTOM);
    RETURN_IF_FAILED_MSG(HRESULT_FROM_WIN32(CM_MapCrToWin32Err(cr, ERROR_INVALID_DATA)), "Failed to locate devNode");

    wchar_t vetoString[MAX_PATH] = { 0 };
    PNP_VETO_TYPE vetoType = PNP_VetoTypeUnknown;
    cr = CM_Query_And_Remove_SubTree(hDevInstance, &vetoType, vetoString, MAX_PATH, CM_REMOVE_NO_RESTART);
    RETURN_IF_FAILED_MSG(HRESULT_FROM_WIN32(CM_MapCrToWin32Err(cr, ERROR_INVALID_DATA)), "Failed to CM_Query_And_Remove_SubTree");

    cr = CM_Uninstall_DevNode(hDevInstance, 0);
    RETURN_IF_FAILED_MSG(HRESULT_FROM_WIN32(CM_MapCrToWin32Err(cr, ERROR_INVALID_DATA)), "Failed to CM_Uninstall_DevNode");

    LOG_COMMENT(L"succeeded \n");

    return S_OK;
}

HRESULT VCamUtils::MSIUninstall(GUID const& clsid)
{
    wil::com_ptr_nothrow<IMFAttributes> spAttributes;
    IMFActivate** ppActivates;
    wil::unique_cotaskmem_array_ptr<wil::com_ptr_nothrow<IMFActivate>> spActivates;
    UINT32 numActivates = 0;

    RETURN_IF_FAILED(MFCreateAttributes(&spAttributes, 1));
    RETURN_IF_FAILED(spAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID));
    RETURN_IF_FAILED(spAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_CATEGORY, KSCATEGORY_VIDEO_CAMERA));
    RETURN_IF_FAILED(MFEnumDeviceSources(spAttributes.get(), &spActivates, &numActivates));

    for (unsigned int i = 0; i < numActivates; i++)
    {
        wil::com_ptr_nothrow<IMFActivate>spActivate = spActivates[i];
        wil::unique_cotaskmem_string spwszSymLink;
        UINT32 cch = 0;
        RETURN_IF_FAILED(spActivate->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &spwszSymLink, &cch));

        wistd::unique_ptr<BYTE[]> spBuffer;
        ULONG cbBufferSize = 0;
        if (SUCCEEDED(VCamUtils::GetDeviceInterfaceRegistryEntry(
            spwszSymLink.get(),
            L"CustomCaptureSourceClsid",
            spBuffer,
            cbBufferSize)))
        {
            if (_wcsicmp((LPCWSTR)spBuffer.get(), winrt::to_hstring(clsid).data()) == 0)
            {
                VCamUtils::UnInstallDeviceWin32(spwszSymLink.get());
            }
        }
    }
    return S_OK;
}

HRESULT VCamUtils::GetCameraActivate(const wchar_t* pwszSymLink, IMFActivate** ppActivate)
{
    wil::com_ptr_nothrow<IMFAttributes> spAttributes;
    IMFActivate** ppActivates;
    wil::unique_cotaskmem_array_ptr<wil::com_ptr_nothrow<IMFActivate>> spActivates;
    UINT32 numActivates = 0;

    RETURN_HR_IF_NULL(E_POINTER, ppActivate);
    RETURN_IF_FAILED(MFCreateAttributes(&spAttributes, 1));
    RETURN_IF_FAILED(spAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID));
    RETURN_IF_FAILED(spAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_CATEGORY, KSCATEGORY_VIDEO_CAMERA));
    RETURN_IF_FAILED(MFEnumDeviceSources(spAttributes.get(), &ppActivates, &numActivates));
    spActivates.reset(ppActivates, numActivates);

    for (unsigned int i = 0; i < numActivates; i++)
    {
        wil::com_ptr_nothrow<IMFActivate>spActivate = spActivates[i];
        wil::unique_cotaskmem_string spszSymLink;
        UINT32 cch = 0;
        RETURN_IF_FAILED(spActivate->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &spszSymLink, &cch));

        if (_wcsicmp(spszSymLink.get(), pwszSymLink) == 0)
        {
            RETURN_IF_FAILED(spActivate.copy_to(ppActivate));
            return S_OK;
        }
    }

    return MF_E_UNSUPPORTED_CAPTURE_DEVICE_PRESENT;
}

HRESULT VCamUtils::InitializeVirtualCamera(const wchar_t* pwszSymLink, IMFMediaSource** ppMediaSource)
{
    /*wil::com_ptr_nothrow<IMFAttributes> spCreateAttribute;
    RETURN_IF_FAILED(MFCreateAttributes(&spCreateAttribute, 2));
    RETURN_IF_FAILED(spCreateAttribute->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID));
    RETURN_IF_FAILED(spCreateAttribute->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, pwszSymLink));

    wil::com_ptr_nothrow<IMFMediaSource> spMediaSource;
    RETURN_IF_FAILED(MFCreateDeviceSource(spCreateAttribute.get(), &spMediaSource));*/

    wil::com_ptr_nothrow<IMFActivate> spActivate;
    RETURN_IF_FAILED(GetCameraActivate(pwszSymLink, &spActivate));
    LOG_COMMENT(L"Activate virtualcamera %s ", pwszSymLink);
    RETURN_IF_FAILED(spActivate->ActivateObject(IID_PPV_ARGS(ppMediaSource)));

    return S_OK;
}

//
// Return list of installed VirtualCamera
//
HRESULT VCamUtils::GetVirtualCamera(std::vector<DeviceInformation>& vcamList)
{
    vcamList.clear();

    winrt::Windows::Devices::Enumeration::DeviceInformationCollection devList{ nullptr };
    RETURN_IF_FAILED(CMediaCaptureUtils::GetDeviceList(MediaDevice::GetVideoCaptureSelector(), devList));

    for (uint32_t i = 0; i < devList.Size(); i++)
    {
        auto dev = devList.GetAt(i);
        if (IsVirtualCamera(dev.Id().data()))
        {
            vcamList.push_back(dev);
        }
    }
    return S_OK;
}

HRESULT VCamUtils::GetPhysicalCameras(std::vector<DeviceInformation>& physicalCamList)
{
    physicalCamList.clear();

    winrt::Windows::Devices::Enumeration::DeviceInformationCollection devList{ nullptr };
    RETURN_IF_FAILED(CMediaCaptureUtils::GetDeviceList(MediaDevice::GetVideoCaptureSelector(), devList));

    for (uint32_t i = 0; i < devList.Size(); i++)
    {
        auto dev = devList.GetAt(i);
        if (!IsVirtualCamera(dev.Id().data()))
        {
            physicalCamList.push_back(dev);
        }
    }
    return S_OK;
}
