//
// Copyright (C) Microsoft Corporation. All rights reserved.
//
#include "pch.h"

namespace winrt::WindowsSample::implementation
{
    // IMFActivate
    IFACEMETHODIMP VirtualCameraMediaSourceActivate::ActivateObject(REFIID riid, void** ppv)
    {
        RETURN_HR_IF_NULL(E_POINTER, ppv);
        *ppv = nullptr;

        bool bIsWrappingCamera = false;
        wil::unique_cotaskmem_string spwszPhysicalSymLink;
        UINT32 vCamKind = 0;
        UINT32 cch = 0;
        wil::com_ptr_nothrow<IMFCollection> spCollection;
        wil::com_ptr_nothrow<IMFMediaSource> spMediaSource;

        if (m_spActivateAttributes)
        {
            if (SUCCEEDED(m_spActivateAttributes->GetUINT32(VCAM_KIND, &vCamKind)))
            {
                DEBUG_MSG(L"Set VirtualCameraKind: %i", vCamKind);
            }
            if (SUCCEEDED(m_spActivateAttributes->GetUnknown(MF_VIRTUALCAMERA_ASSOCIATED_CAMERA_SOURCES, IID_PPV_ARGS(&spCollection))))
            {
                DEBUG_MSG(L"Initialize using the MF_VIRTUALCAMERA_ASSOCIATED_CAMERA_SOURCES");

                DWORD cElementCount = 0;
                wil::com_ptr_nothrow<IMFActivate> spActivate;
                wil::com_ptr_nothrow<IUnknown> spUnknown;

                // check if there is 1 and only 1 physical camera we are wrapping
                RETURN_IF_FAILED(spCollection->GetElementCount(&cElementCount));
                RETURN_HR_IF(MF_E_UNEXPECTED, cElementCount != 1);

                RETURN_IF_FAILED(spCollection->GetElement(0, &spUnknown));
                RETURN_IF_FAILED(spUnknown->QueryInterface(IID_PPV_ARGS(&spActivate)));
                RETURN_IF_FAILED(spActivate->ActivateObject(IID_PPV_ARGS(&spMediaSource)));
                bIsWrappingCamera = true;
            }
            else if(SUCCEEDED(m_spActivateAttributes->GetAllocatedString(VCAM_DEVICE_INFO, &spwszPhysicalSymLink, &cch)))
            {
                DEBUG_MSG(L"Initialize using the explicitly provided deviceSymLink: %s", spwszPhysicalSymLink.get());

                // look at hardware camera specified and extract stream descriptors
                wil::com_ptr_nothrow<IMFAttributes> spDeviceAttributes;
                RETURN_IF_FAILED(MFCreateAttributes(&spDeviceAttributes, 2));
                RETURN_IF_FAILED(spDeviceAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID));
                RETURN_IF_FAILED(spDeviceAttributes->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, spwszPhysicalSymLink.get()));
                RETURN_IF_FAILED(MFCreateDeviceSource(spDeviceAttributes.get(), &spMediaSource));
                bIsWrappingCamera = true;
            }
        }

        if (bIsWrappingCamera)
        {
            if (vCamKind == (UINT32)VirtualCameraKind::BasicCameraWrapper)
            {
                DEBUG_MSG(L"Activate HWMediaSource");
                m_spHWMediaSrc = winrt::make_self<winrt::WindowsSample::implementation::HWMediaSource>();
                RETURN_IF_FAILED(m_spHWMediaSrc->Initialize(this, spMediaSource.get()));
                RETURN_IF_FAILED(m_spHWMediaSrc->QueryInterface(riid, ppv));
            }
            else if (vCamKind == (UINT32)VirtualCameraKind::AugmentedCameraWrapper)
            {
                DEBUG_MSG(L"Activate AugmentedMediaSource");
                m_spAugmentedMediaSrc = winrt::make_self<winrt::WindowsSample::implementation::AugmentedMediaSource>();
                RETURN_IF_FAILED(m_spAugmentedMediaSrc->Initialize(this, spMediaSource.get()));
                RETURN_IF_FAILED(m_spAugmentedMediaSrc->QueryInterface(riid, ppv));
            }
            else
            {
                DEBUG_MSG(L"Attempting to activate virtual camera with an unknown virtual camera kind to wrap an existing camera");
                return ERROR_BAD_CONFIGURATION;
            }
        }
        else if (vCamKind == (UINT32)VirtualCameraKind::Synthetic)
        {
            DEBUG_MSG(L"Activate SimpleMediaSource");
            m_spSimpleMediaSrc = winrt::make_self<winrt::WindowsSample::implementation::SimpleMediaSource>();
            RETURN_IF_FAILED(m_spSimpleMediaSrc->Initialize(this));
            RETURN_IF_FAILED(m_spSimpleMediaSrc->QueryInterface(riid, ppv));
        }
        else
        {
            return ERROR_BAD_CONFIGURATION;
        }
        return S_OK;
    }

    IFACEMETHODIMP VirtualCameraMediaSourceActivate::ShutdownObject()
    {
        return S_OK;
    }

    IFACEMETHODIMP VirtualCameraMediaSourceActivate::DetachObject()
    {
        m_spSimpleMediaSrc = nullptr;
        m_spHWMediaSrc = nullptr;
        m_spAugmentedMediaSrc = nullptr;

        return S_OK;
    }

    // IMFAttributes (inherits by IMFActivate)
    IFACEMETHODIMP VirtualCameraMediaSourceActivate::GetItem(_In_ REFGUID guidKey, _Inout_opt_  PROPVARIANT* pValue)
    {
        if (!m_spActivateAttributes)
            RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
        return m_spActivateAttributes->GetItem(guidKey, pValue);
    }

    IFACEMETHODIMP VirtualCameraMediaSourceActivate::GetItemType(_In_ REFGUID guidKey, _Out_ MF_ATTRIBUTE_TYPE* pType)
    {
        if (!m_spActivateAttributes)
            RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
        return m_spActivateAttributes->GetItemType(guidKey, pType);
    }

    IFACEMETHODIMP VirtualCameraMediaSourceActivate::CompareItem(_In_ REFGUID guidKey, _In_ REFPROPVARIANT Value, _Out_ BOOL* pbResult)
    {
        if (!m_spActivateAttributes)
            RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
        return m_spActivateAttributes->CompareItem(guidKey, Value, pbResult);
    }

    IFACEMETHODIMP VirtualCameraMediaSourceActivate::Compare(_In_ IMFAttributes* pTheirs, _In_ MF_ATTRIBUTES_MATCH_TYPE MatchType, _Out_ BOOL* pbResult)
    {
        if (!m_spActivateAttributes)
            RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
        return m_spActivateAttributes->Compare(pTheirs, MatchType, pbResult);
    }

    IFACEMETHODIMP VirtualCameraMediaSourceActivate::GetUINT32(_In_ REFGUID guidKey, _Out_ UINT32* punValue)
    {
        if (!m_spActivateAttributes)
            RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
        return m_spActivateAttributes->GetUINT32(guidKey, punValue);
    }

    IFACEMETHODIMP VirtualCameraMediaSourceActivate::GetUINT64(_In_ REFGUID guidKey, _Out_ UINT64* punValue)
    {
        if (!m_spActivateAttributes)
            RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
        return m_spActivateAttributes->GetUINT64(guidKey, punValue);
    }

    IFACEMETHODIMP VirtualCameraMediaSourceActivate::GetDouble(_In_ REFGUID guidKey, _Out_ double* pfValue)
    {
        if (!m_spActivateAttributes)
            RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
        return m_spActivateAttributes->GetDouble(guidKey, pfValue);
    }
    IFACEMETHODIMP VirtualCameraMediaSourceActivate::GetGUID(_In_ REFGUID guidKey, _Out_ GUID* pguidValue)
    {
        if (!m_spActivateAttributes)
            RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
        return m_spActivateAttributes->GetGUID(guidKey, pguidValue);
    }

    IFACEMETHODIMP VirtualCameraMediaSourceActivate::GetStringLength(_In_ REFGUID guidKey, _Out_ UINT32* pcchLength)
    {
        if (!m_spActivateAttributes)
            RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
        return m_spActivateAttributes->GetStringLength(guidKey, pcchLength);
    }

    IFACEMETHODIMP VirtualCameraMediaSourceActivate::GetString(_In_ REFGUID guidKey, _Out_writes_(cchBufSize) LPWSTR pwszValue, _In_ UINT32 cchBufSize, _Inout_opt_ UINT32* pcchLength)
    {
        if (!m_spActivateAttributes)
            RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
        return m_spActivateAttributes->GetString(guidKey, pwszValue, cchBufSize, pcchLength);
    }

    IFACEMETHODIMP VirtualCameraMediaSourceActivate::GetAllocatedString(_In_ REFGUID guidKey, _Out_writes_(*pcchLength + 1) LPWSTR* ppwszValue, _Inout_  UINT32* pcchLength)
    {
        if (!m_spActivateAttributes)
            RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
        return m_spActivateAttributes->GetAllocatedString(guidKey, ppwszValue, pcchLength);
    }

    IFACEMETHODIMP VirtualCameraMediaSourceActivate::GetBlobSize(_In_ REFGUID guidKey, _Out_ UINT32* pcbBlobSize)
    {
        if (!m_spActivateAttributes)
            RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
        return m_spActivateAttributes->GetBlobSize(guidKey, pcbBlobSize);
    }

    IFACEMETHODIMP VirtualCameraMediaSourceActivate::GetBlob(_In_ REFGUID  guidKey, _Out_writes_(cbBufSize) UINT8* pBuf, UINT32 cbBufSize, _Inout_  UINT32* pcbBlobSize)
    {
        if (!m_spActivateAttributes)
            RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
        return m_spActivateAttributes->GetBlob(guidKey, pBuf, cbBufSize, pcbBlobSize);
    }

    IFACEMETHODIMP VirtualCameraMediaSourceActivate::GetAllocatedBlob(__RPC__in REFGUID guidKey, __RPC__deref_out_ecount_full_opt(*pcbSize) UINT8** ppBuf, __RPC__out UINT32* pcbSize)
    {
        if (!m_spActivateAttributes)
            RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
        return m_spActivateAttributes->GetAllocatedBlob(guidKey, ppBuf, pcbSize);
    }

    IFACEMETHODIMP VirtualCameraMediaSourceActivate::GetUnknown(__RPC__in REFGUID guidKey, __RPC__in REFIID riid, __RPC__deref_out_opt LPVOID* ppv)
    {
        if (!m_spActivateAttributes)
            RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
        return m_spActivateAttributes->GetUnknown(guidKey, riid, ppv);
    }

    IFACEMETHODIMP VirtualCameraMediaSourceActivate::SetItem(_In_ REFGUID guidKey, _In_ REFPROPVARIANT Value)
    {
        if (!m_spActivateAttributes)
            RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
        return m_spActivateAttributes->SetItem(guidKey, Value);
    }

    IFACEMETHODIMP VirtualCameraMediaSourceActivate::DeleteItem(_In_ REFGUID guidKey)
    {
        if (!m_spActivateAttributes)
            RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
        return m_spActivateAttributes->DeleteItem(guidKey);
    }

    IFACEMETHODIMP VirtualCameraMediaSourceActivate::DeleteAllItems()
    {
        if (!m_spActivateAttributes)
            RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
        return m_spActivateAttributes->DeleteAllItems();
    }

    IFACEMETHODIMP VirtualCameraMediaSourceActivate::SetUINT32(_In_ REFGUID guidKey, _In_ UINT32  unValue)
    {
        if (!m_spActivateAttributes)
            RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
        return m_spActivateAttributes->SetUINT32(guidKey, unValue);
    }

    IFACEMETHODIMP VirtualCameraMediaSourceActivate::SetUINT64(_In_ REFGUID guidKey, _In_ UINT64  unValue)
    {
        if (!m_spActivateAttributes)
            RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
        return m_spActivateAttributes->SetUINT64(guidKey, unValue);
    }

    IFACEMETHODIMP VirtualCameraMediaSourceActivate::SetDouble(_In_ REFGUID guidKey, _In_ double  fValue)
    {
        if (!m_spActivateAttributes)
            RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
        return m_spActivateAttributes->SetDouble(guidKey, fValue);
    }

    IFACEMETHODIMP VirtualCameraMediaSourceActivate::SetGUID(_In_ REFGUID guidKey, _In_ REFGUID guidValue)
    {
        if (!m_spActivateAttributes)
            RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
        return m_spActivateAttributes->SetGUID(guidKey, guidValue);
    }

    IFACEMETHODIMP VirtualCameraMediaSourceActivate::SetString(_In_ REFGUID guidKey, _In_ LPCWSTR wszValue)
    {
        if (!m_spActivateAttributes)
            RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
        return m_spActivateAttributes->SetString(guidKey, wszValue);
    }

    IFACEMETHODIMP VirtualCameraMediaSourceActivate::SetBlob(_In_ REFGUID guidKey, _In_reads_(cbBufSize) const UINT8* pBuf, UINT32 cbBufSize)
    {
        if (!m_spActivateAttributes)
            RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
        return m_spActivateAttributes->SetBlob(guidKey, pBuf, cbBufSize);
    }

    IFACEMETHODIMP VirtualCameraMediaSourceActivate::SetUnknown(_In_ REFGUID guidKey, _In_ IUnknown* pUnknown)
    {
        if (!m_spActivateAttributes)
            RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
        return m_spActivateAttributes->SetUnknown(guidKey, pUnknown);
    }

    IFACEMETHODIMP VirtualCameraMediaSourceActivate::LockStore()
    {
        if (!m_spActivateAttributes)
            RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
        return m_spActivateAttributes->LockStore();
    }

    IFACEMETHODIMP VirtualCameraMediaSourceActivate::UnlockStore()
    {
        if (!m_spActivateAttributes)
            RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
        return m_spActivateAttributes->UnlockStore();
    }

    IFACEMETHODIMP VirtualCameraMediaSourceActivate::GetCount(_Out_ UINT32* pcItems)
    {
        if (!m_spActivateAttributes)
            RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
        return m_spActivateAttributes->GetCount(pcItems);
    }

    IFACEMETHODIMP VirtualCameraMediaSourceActivate::GetItemByIndex(UINT32 unIndex, _Out_ GUID* pguidKey, _Inout_ PROPVARIANT* pValue)
    {
        if (!m_spActivateAttributes)
            RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
        return m_spActivateAttributes->GetItemByIndex(unIndex, pguidKey, pValue);
    }

    IFACEMETHODIMP VirtualCameraMediaSourceActivate::CopyAllItems(_In_ IMFAttributes* pDest)
    {
        if (!m_spActivateAttributes)
            RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 1));
        return m_spActivateAttributes->CopyAllItems(pDest);
    }

    // non interface method
    HRESULT VirtualCameraMediaSourceActivate::Initialize()
    {
        RETURN_IF_FAILED(MFCreateAttributes(&m_spActivateAttributes, 3));
        RETURN_IF_FAILED(m_spActivateAttributes->SetUINT32(MF_VIRTUALCAMERA_PROVIDE_ASSOCIATED_CAMERA_SOURCES, 1));
        return S_OK;
    }
}
