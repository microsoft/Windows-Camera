//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#include "pch.h"
#include "CustomMediaSourceUT.h"
#include "VirtualCameraMediaSource.h"
#include "VCamutils.h"

namespace VirtualCameraTest::impl
{
    HRESULT CustomMediaSourceUT::CreateSourceAttributes(_Outptr_ IMFAttributes** ppAttributes)
    {
        RETURN_HR_IF_NULL(E_POINTER, ppAttributes);
        wil::com_ptr_nothrow<IMFAttributes> spAttributes;
        RETURN_IF_FAILED(MFCreateAttributes(&spAttributes, 1));

        *ppAttributes = spAttributes.detach();

        return S_OK;
    }

    HRESULT CustomMediaSourceUT::TestMediaSourceStream()
    {
        wil::com_ptr_nothrow<IMFAttributes> spAttributes;
        RETURN_IF_FAILED(CreateSourceAttributes(&spAttributes));

        wil::com_ptr_nothrow<IMFMediaSource> spMediaSource;
        RETURN_IF_FAILED(CoCreateAndActivateMediaSource(m_clsid, spAttributes.get(), &spMediaSource));


        wil::com_ptr_nothrow<IMFPresentationDescriptor> spPD;
        DWORD streamCount = 0;
        RETURN_IF_FAILED(spMediaSource->CreatePresentationDescriptor(&spPD));
        RETURN_IF_FAILED(spPD->GetStreamDescriptorCount(&streamCount));
        if (streamCount != m_streamCount)
        {
            LOG_ERROR_RETURN(E_TEST_FAILED, L"Unexpected stream count: %d (expected: %d) \n", streamCount, m_streamCount);
        }

        RETURN_IF_FAILED(MediaSourceUT_Common::TestMediaSourceStream(spMediaSource.get()));

        return S_OK;
    }

    HRESULT CustomMediaSourceUT::TestKsControl()
    {
        wil::com_ptr_nothrow<IMFAttributes> spAttributes;
        RETURN_IF_FAILED(MFCreateAttributes(&spAttributes, 1));

        wil::com_ptr_nothrow<IMFMediaSource> spMediaSource;
        RETURN_IF_FAILED(CoCreateAndActivateMediaSource(m_clsid, spAttributes.get(), &spMediaSource));

        wil::com_ptr_nothrow<IKsControl> spKsControl;
        RETURN_IF_FAILED(spMediaSource->QueryInterface(IID_PPV_ARGS(&spKsControl)));

        RETURN_IF_FAILED(ValidateKsControl_UnSupportedControl(spKsControl.get()));

        return S_OK;
    }

    HRESULT CustomMediaSourceUT::TestCreateVirtualCamera()
    {
        wil::com_ptr_nothrow<IMFVirtualCamera> spVirtualCamera;
        auto cleanup = wil::scope_exit_log(WI_DIAGNOSTICS_INFO, [&]() {
            if (spVirtualCamera)
            {
                wil::unique_cotaskmem_string pwszSymLink;
                UINT32 cch;
                if (SUCCEEDED(spVirtualCamera->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &pwszSymLink, &cch)))
                {
                    spVirtualCamera->Shutdown();
                    VCamUtils::UnInstallDeviceWin32(pwszSymLink.get());
                }
                else
                {
                    spVirtualCamera->Shutdown();
                }
            }
            });

        RETURN_IF_FAILED(VCamUtils::RegisterVirtualCamera(
            VIRTUALCAMERAMEDIASOURCE_CLSID,
            m_vcamFriendlyName,
            L"",
            MFVirtualCameraLifetime_System,
            MFVirtualCameraAccess_CurrentUser,
            nullptr,
            &spVirtualCamera));

        RETURN_IF_FAILED(ValidateVirtualCamera(spVirtualCamera.get()));

        return S_OK;
    }
}
