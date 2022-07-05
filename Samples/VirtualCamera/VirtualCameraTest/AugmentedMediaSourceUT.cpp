//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#include "pch.h"
#include "AugmentedMediaSourceUT.h"
#include "VCamutils.h"

namespace VirtualCameraTest::impl
{
    HRESULT AugmentedMediaSourceUT::CreateSourceAttributes(_Outptr_ IMFAttributes** ppAttributes)
    {
        RETURN_HR_IF_NULL(E_POINTER, ppAttributes);
        wil::com_ptr_nothrow<IMFAttributes> spAttributes;
        RETURN_IF_FAILED(MFCreateAttributes(&spAttributes, 2));
        RETURN_IF_FAILED(spAttributes->SetString(VCAM_DEVICE_INFO, m_devSymlink.data()));
        RETURN_IF_FAILED(spAttributes->SetUINT32(VCAM_KIND, (UINT32)VirtualCameraKind::AugmentedCameraWrapper));

        *ppAttributes = spAttributes.detach();

        return S_OK;
    }

    HRESULT AugmentedMediaSourceUT::TestMediaSourceStream()
    {
        wil::com_ptr_nothrow<IMFAttributes> spAttributes;
        RETURN_IF_FAILED(CreateSourceAttributes(&spAttributes));

        wil::com_ptr_nothrow<IMFMediaSource> spMediaSource;
        RETURN_IF_FAILED(CoCreateAndActivateMediaSource(CLSID_VirtualCameraMediaSource, spAttributes.get(), &spMediaSource));

        // Validate the stream count from AugmentedMediaSource matches expectation of 1 stream only.
        wil::com_ptr_nothrow<IMFPresentationDescriptor> spPD;
        DWORD streamCount = 0;
        RETURN_IF_FAILED(spMediaSource->CreatePresentationDescriptor(&spPD));
        RETURN_IF_FAILED(spPD->GetStreamDescriptorCount(&streamCount));
        if (1 != streamCount)
        {
            LOG_ERROR_RETURN(E_TEST_FAILED, L"Unexpected stream count: %d (expected: 1) \n", streamCount);
        }

        RETURN_IF_FAILED(MediaSourceUT_Common::TestMediaSourceStream(spMediaSource.get()));

        return S_OK;
    }
    HRESULT AugmentedMediaSourceUT::TestKsControl()
    {
        wil::com_ptr_nothrow<IMFAttributes> spAttributes;
        RETURN_IF_FAILED(CreateSourceAttributes(&spAttributes));

        wil::com_ptr_nothrow<IMFMediaSource> spMediaSource;
        RETURN_IF_FAILED(CoCreateAndActivateMediaSource(CLSID_VirtualCameraMediaSource, spAttributes.get(), &spMediaSource));

        wil::com_ptr_nothrow<IKsControl> spKsControl;
        RETURN_IF_FAILED(spMediaSource->QueryInterface(IID_PPV_ARGS(&spKsControl)));

        RETURN_IF_FAILED(ValidateKsControl_UnSupportedControl(spKsControl.get()));

        return S_OK;
    }

    HRESULT AugmentedMediaSourceUT::TestCreateVirtualCamera()
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

        RETURN_IF_FAILED(CreateVirtualCamera(L"test", MFVirtualCameraLifetime_System, MFVirtualCameraAccess_CurrentUser, &spVirtualCamera));
        RETURN_IF_FAILED(ValidateVirtualCamera(spVirtualCamera.get()));

        return S_OK;
    }

    ////////////////////////////////////////////////////////////////////
    // helper function
    HRESULT AugmentedMediaSourceUT::CreateVirtualCamera(winrt::hstring const& postfix, MFVirtualCameraLifetime vcamLifetime, MFVirtualCameraAccess vcamAccess, IMFVirtualCamera** ppVirtualCamera)
    {
        wchar_t friendlyname[MAX_PATH];
        int cch = swprintf(friendlyname, MAX_PATH, L"AugmentedMediaSource-%s", postfix.data());
        if (cch == -1)
        {
            return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
        }

        wil::com_ptr_nothrow<IMFAttributes> spAttributes;
        RETURN_IF_FAILED(CreateSourceAttributes(&spAttributes));

        RETURN_IF_FAILED(VCamUtils::RegisterVirtualCamera(
            VIRTUALCAMERAMEDIASOURCE_CLSID,
            friendlyname,
            m_devSymlink,
            vcamLifetime,
            vcamAccess,
            spAttributes.get(),
            ppVirtualCamera));

        return S_OK;
    }
}
