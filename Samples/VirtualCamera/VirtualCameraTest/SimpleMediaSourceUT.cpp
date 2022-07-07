//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#include "pch.h"
#include "SimpleMediaSourceUT.h"
#include "VirtualCameraMediaSource.h"
#include "VCamutils.h"

namespace VirtualCameraTest::impl
{
    static const winrt::hstring strCustomControl(L" {0CE2EF73-4800-4F53-9B8E-8C06790FC0C7},0");

    HRESULT SimpleMediaSourceUT::CreateSourceAttributes(_Outptr_ IMFAttributes** ppAttributes)
    {
        RETURN_HR_IF_NULL(E_POINTER, ppAttributes);
        wil::com_ptr_nothrow<IMFAttributes> spAttributes;
        RETURN_IF_FAILED(MFCreateAttributes(&spAttributes, 1));
        RETURN_IF_FAILED(spAttributes->SetUINT32(VCAM_KIND, (UINT32)VirtualCameraKind::Synthetic));

        *ppAttributes = spAttributes.detach();

        return S_OK;
    }

    HRESULT SimpleMediaSourceUT::TestMediaSourceStream()
    {
        // SimpleMediaSource 1 stream, with 2 mediatypes
        // This test validate we can 1. access the stream, 2. stream from each mediatype
        wil::com_ptr_nothrow<IMFMediaSource> spMediaSource;
        RETURN_IF_FAILED(CoCreateAndActivateMediaSource(CLSID_VirtualCameraMediaSource, nullptr, &spMediaSource));

        wil::com_ptr_nothrow<IMFPresentationDescriptor> spPD;
        RETURN_IF_FAILED(spMediaSource->CreatePresentationDescriptor(&spPD));

        DWORD streamCount = 0;
        RETURN_IF_FAILED(spPD->GetStreamDescriptorCount(&streamCount));
        if (streamCount != 1)
        {
            LOG_ERROR_RETURN(E_TEST_FAILED, L"Unexpected stream count: %d (expected: %d)", streamCount, 1);
        }

        for (unsigned int streamIdx = 0; streamIdx < streamCount; streamIdx++)
        {
            DWORD mediaTypeCount = 0;
            BOOL selected = FALSE;
            wil::com_ptr_nothrow<IMFStreamDescriptor> spStreamDescriptor;
            wil::com_ptr_nothrow<IMFMediaTypeHandler> spMediaTypeHandler;

            RETURN_IF_FAILED(spPD->GetStreamDescriptorByIndex(streamIdx, &selected, &spStreamDescriptor));
            RETURN_IF_FAILED(spStreamDescriptor->GetMediaTypeHandler(&spMediaTypeHandler));

            RETURN_IF_FAILED(spMediaTypeHandler->GetMediaTypeCount(&mediaTypeCount));
            if (mediaTypeCount != 2)
            {
                LOG_ERROR_RETURN(E_TEST_FAILED, L"Unexpected media type count: %d (expected: %d) ", mediaTypeCount, 2);
            }
        }
        RETURN_IF_FAILED(MediaSourceUT_Common::TestMediaSourceStream(spMediaSource.get()));

        return S_OK;
    }

    HRESULT SimpleMediaSourceUT::TestKsControl()
    {
        wil::com_ptr_nothrow<IMFMediaSource> spMediaSource;
        RETURN_IF_FAILED(CoCreateAndActivateMediaSource(CLSID_VirtualCameraMediaSource, nullptr, &spMediaSource));

        wil::com_ptr_nothrow<IKsControl> spKsControl;
        RETURN_IF_FAILED(spMediaSource->QueryInterface(IID_PPV_ARGS(&spKsControl)));

        RETURN_IF_FAILED(ValidateKsControl_UnSupportedControl(spKsControl.get()));
        
        // Validate Supported Control
        {
            LOG_COMMENT(L"Validate KSProperty return data size ...");
            KSPROPERTY ksProperty;
            ksProperty.Set = PROPSETID_SIMPLEMEDIASOURCE_CUSTOMCONTROL;
            ksProperty.Id = KSPROPERTY_SIMPLEMEDIASOURCE_CUSTOMCONTROL_COLORING;
            ksProperty.Flags = KSPROPERTY_TYPE_GET;

            ULONG byteReturns = 0;
            HRESULT hr = spKsControl->KsProperty(
                &ksProperty,
                sizeof(KSPROPERTY),
                nullptr,
                0,
                &byteReturns);

            if (hr == HRESULT_FROM_WIN32(ERROR_MORE_DATA))
            {
                if (byteReturns == sizeof(KSPROPERTY_SIMPLEMEDIASOURCE_CUSTOMCONTROL_COLORMODE_S))
                {
                    LOG_COMMENT(L"Function returns expected hr: 0x%08x, byteReturn:%d)", hr, byteReturns);
                }
                else
                {
                    LOG_ERROR_RETURN(E_TEST_FAILED, L"Function returns incorrect byteReturn:%d (expected: %d)", byteReturns, sizeof(KSPROPERTY_SIMPLEMEDIASOURCE_CUSTOMCONTROL_COLORMODE_S));
                }
            }
            else
            {
                LOG_ERROR_RETURN(E_TEST_FAILED, L"Function returns unexpected hr: 0x%08x (expected: 0x%08x)", hr, HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND));
            }

            LOG_COMMENT(L"Validate KSProperty return error on invalid data size ...")
                winrt::com_array<BYTE> arr(sizeof(KSPROPERTY_SIMPLEMEDIASOURCE_CUSTOMCONTROL_COLORMODE_S));
            hr = spKsControl->KsProperty(
                &ksProperty,
                sizeof(KSPROPERTY),
                arr.data(),
                0,
                &byteReturns);
            if (FAILED(hr))
            {
                LOG_SUCCESS(L"Function returns failed hr: 0x%08x on incorrect datalength)", hr);
            }
            else
            {
                LOG_ERROR_RETURN(E_TEST_FAILED, L"Function did not return failed hr on incorrect datalength, hr:0x%08x", hr);
            }

            // Validate Get Control
            ksProperty.Flags |= KSPROPERTY_TYPE_GET;
            hr = spKsControl->KsProperty(
                &ksProperty,
                sizeof(KSPROPERTY),
                arr.data(),
                arr.size(),
                &byteReturns);

            if (FAILED(hr))
            {
                LOG_ERROR_RETURN(hr, L"Function failed to get support control hr: 0x%08x)", hr);
            }
            else if (byteReturns != sizeof(KSPROPERTY_SIMPLEMEDIASOURCE_CUSTOMCONTROL_COLORMODE_S))
            {
                LOG_ERROR_RETURN(E_TEST_FAILED, L"Function didn't return the correct byteReturn: %d (expected: %d)", byteReturns, sizeof(KSPROPERTY_SIMPLEMEDIASOURCE_CUSTOMCONTROL_COLORMODE_S));
            }
            else
            {
                LOG_SUCCESS(L"Get Support control succeeded, byteReturn: %d, colorMode: 0x%08x", byteReturns, ((PKSPROPERTY_SIMPLEMEDIASOURCE_CUSTOMCONTROL_COLORMODE_S)arr.data())->ColorMode);
            }

            ksProperty.Flags |= KSPROPERTY_TYPE_SET;
            hr = spKsControl->KsProperty(
                &ksProperty,
                sizeof(KSPROPERTY),
                arr.data(),
                arr.size(),
                &byteReturns);

            if (FAILED(hr))
            {
                LOG_ERROR_RETURN(hr, L"Function failed to get support control hr: 0x%08x)", hr);
            }
            else
            {
                LOG_SUCCESS(L"Set Support control succeeded, byteReturn: %d, colorMode: 0x%08x", byteReturns, ((PKSPROPERTY_SIMPLEMEDIASOURCE_CUSTOMCONTROL_COLORMODE_S)arr.data())->ColorMode);
            }
        }

        return S_OK;
    }

    HRESULT SimpleMediaSourceUT::TestCreateVirtualCamera()
    {
        wil::com_ptr_nothrow<IMFVirtualCamera> spVirtualCamera;
        auto exit = wil::scope_exit_log(WI_DIAGNOSTICS_INFO, [&]
            {
                if (spVirtualCamera.get() != nullptr)
                {
                    VCamUtils::UnInstallDevice(spVirtualCamera.get());
                }
            });
        RETURN_IF_FAILED(CreateVirtualCamera(MFVirtualCameraLifetime_System, MFVirtualCameraAccess_CurrentUser, &spVirtualCamera));
        RETURN_IF_FAILED(ValidateVirtualCamera(spVirtualCamera.get()));

        return S_OK;
    }

    ////////////////////////////////////////////////////////////////////
    // helper function
    HRESULT SimpleMediaSourceUT::CreateVirtualCamera(MFVirtualCameraLifetime vcamLifetime, MFVirtualCameraAccess vcamAccess, IMFVirtualCamera** ppVirtualCamera)
    {
        winrt::hstring physicalCamSymLink;
        RETURN_IF_FAILED(VCamUtils::RegisterVirtualCamera(
            VIRTUALCAMERAMEDIASOURCE_CLSID,
            L"SWVCamMediaSource",
            physicalCamSymLink,
            vcamLifetime,
            vcamAccess,
            nullptr,
            ppVirtualCamera));

        return S_OK;
    }

    HRESULT SimpleMediaSourceUT::GetColorMode(IMFMediaSource* pMediaSource, uint32_t* pColorMode)
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, pMediaSource);
        RETURN_HR_IF_NULL(E_POINTER, pColorMode);
        *pColorMode = 0;

        wil::com_ptr_nothrow<IKsControl> spKsControl;
        RETURN_IF_FAILED(pMediaSource->QueryInterface(IID_PPV_ARGS(&spKsControl)));

        KSPROPERTY ksProperty;
        ksProperty.Flags |= KSPROPERTY_TYPE_GET;
        ksProperty.Set = PROPSETID_SIMPLEMEDIASOURCE_CUSTOMCONTROL;
        ksProperty.Id = KSPROPERTY_SIMPLEMEDIASOURCE_CUSTOMCONTROL_COLORING;

        wil::unique_cotaskmem_ptr<BYTE[]> arr = wil::make_unique_cotaskmem_nothrow<BYTE[]>(sizeof(KSPROPERTY_SIMPLEMEDIASOURCE_CUSTOMCONTROL_COLORMODE_S));
        RETURN_IF_NULL_ALLOC(arr.get());

        ULONG byteReturns = 0;

        // Validate Get Control
        RETURN_IF_FAILED(spKsControl->KsProperty(
            &ksProperty,
            sizeof(KSPROPERTY),
            arr.get(),
            sizeof(KSPROPERTY_SIMPLEMEDIASOURCE_CUSTOMCONTROL_COLORMODE_S),
            &byteReturns));

        *pColorMode = ((PKSPROPERTY_SIMPLEMEDIASOURCE_CUSTOMCONTROL_COLORMODE_S)arr.get())->ColorMode;

        return S_OK;
    }

    HRESULT SimpleMediaSourceUT::SetColorMode(IMFMediaSource* pMediaSource, uint32_t colorMode)
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, pMediaSource);

        wil::com_ptr_nothrow<IKsControl> spKsControl;
        RETURN_IF_FAILED(pMediaSource->QueryInterface(IID_PPV_ARGS(&spKsControl)));

        KSPROPERTY ksProperty;
        ksProperty.Flags |= KSPROPERTY_TYPE_SET;
        ksProperty.Set = PROPSETID_SIMPLEMEDIASOURCE_CUSTOMCONTROL;
        ksProperty.Id = KSPROPERTY_SIMPLEMEDIASOURCE_CUSTOMCONTROL_COLORING;

        KSPROPERTY_SIMPLEMEDIASOURCE_CUSTOMCONTROL_COLORMODE_S data;
        data.ColorMode = colorMode;

        ULONG byteReturns = 0;

        // Validate Set Control
        RETURN_IF_FAILED(spKsControl->KsProperty(
            &ksProperty,
            sizeof(KSPROPERTY),
            &data,
            sizeof(data),
            &byteReturns));

        return S_OK;
    }
}

