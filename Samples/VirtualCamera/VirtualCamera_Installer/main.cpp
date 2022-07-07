//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#include "pch.h"

#include "MediaCaptureUtils.h"
#include "VCamUtils.h"
#include "EVRHelper.h"
#include "VirtualCameraMediaSource.h"

#include "SimpleMediaSourceUT.h"
#include "HWMediaSourceUT.h"
#include "AugmentedMediaSourceUT.h"

using namespace VirtualCameraTest::impl;

void __stdcall WilFailureLog(_In_ const wil::FailureInfo& failure) WI_NOEXCEPT
{
    LOG_WARNING(L"%S(%d):%S, hr=0x%08X, msg=%s",
        failure.pszFile, failure.uLineNumber, failure.pszFunction,
        failure.hr, (failure.pszMessage) ? failure.pszMessage : L"");
}

HRESULT RenderMediaSource(IMFMediaSource* pMediaSource)
{
    wil::com_ptr_nothrow<IMFPresentationDescriptor> spPD;
    RETURN_IF_FAILED(pMediaSource->CreatePresentationDescriptor(&spPD));

    DWORD streamCount = 0;
    RETURN_IF_FAILED(spPD->GetStreamDescriptorCount(&streamCount));

    wil::com_ptr_nothrow<IMFSourceReader> spSourceReader;
    wil::com_ptr_nothrow<IMFAttributes> spAttributes;
    RETURN_IF_FAILED(MFCreateAttributes(&spAttributes, 1));

    RETURN_IF_FAILED(MFCreateSourceReaderFromMediaSource(pMediaSource, spAttributes.get(), &spSourceReader));
    spSourceReader->SetStreamSelection(0, FALSE);
    while (true)
    {
        LOG_COMMENT(L"Select stream index: 1 - %d", streamCount);
        DWORD selection = 0;
        std::wcin >> selection;

        if (selection <= 0 || selection > streamCount)
        {
            // invalid stream selection, exit selection
            break;
        }
        DWORD streamIdx = selection - 1;

        wil::com_ptr_nothrow<IMFStreamDescriptor> spStreamDescriptor;
        BOOL selected = FALSE;
        RETURN_IF_FAILED(spPD->GetStreamDescriptorByIndex(streamIdx, &selected, &spStreamDescriptor));

        GUID category;
        RETURN_IF_FAILED(spStreamDescriptor->GetGUID(MF_DEVICESTREAM_STREAM_CATEGORY, &category));
        if (category == PINNAME_IMAGE)
        {
            LOG_WARNING(L"Skip stream test on streamCateogry: PINNAME_IMAGE");
            continue;
        }

        DWORD streamId = 0;
        spStreamDescriptor->GetStreamIdentifier(&streamId);
        LOG_COMMENT(L"Selected Streamid %d", streamId);

        wil::com_ptr_nothrow<IMFMediaTypeHandler> spMediaTypeHandler;
        DWORD mtCount = 0;
        RETURN_IF_FAILED(spStreamDescriptor->GetMediaTypeHandler(&spMediaTypeHandler));
        RETURN_IF_FAILED(spMediaTypeHandler->GetMediaTypeCount(&mtCount));

        LOG_COMMENT(L"Select media type: 1 - %d", mtCount);
        for (unsigned int i = 0; i < mtCount; i++)
        {
            wil::com_ptr_nothrow<IMFMediaType> spMediaType;
            RETURN_IF_FAILED(spMediaTypeHandler->GetMediaTypeByIndex(i, &spMediaType));
            LOG_COMMENT(L"[%d] %s", i+1, MediaSourceUT_Common::LogMediaType(spMediaType.get()).data());
        }

        selection = 0;
        std::wcin >> selection;
        if (selection <= 0 || selection > mtCount)
        {
            // invalid media type selection, exit selection
            break;
        }
        DWORD mtIdx = selection - 1;

        wil::com_ptr_nothrow<IMFMediaType> spMediaType;
        RETURN_IF_FAILED(spMediaTypeHandler->GetMediaTypeByIndex(mtIdx, &spMediaType));

        // Test Stream
        RETURN_IF_FAILED(spSourceReader->SetStreamSelection(streamIdx, TRUE));
        RETURN_IF_FAILED(spSourceReader->SetCurrentMediaType(streamIdx, NULL, spMediaType.get()));
        RETURN_IF_FAILED_MSG(MediaSourceUT_Common::ValidateStreaming(spSourceReader.get(), streamIdx, spMediaType.get()), "Streaming validation failed");
        RETURN_IF_FAILED(spSourceReader->SetStreamSelection(streamIdx, FALSE));
        LOG_COMMENT(L"Streaming validation passed!");
    }
    return S_OK;
}

//
// CONSOLE
//
winrt::hstring SelectVirtualCamera()
{
    winrt::hstring strSymLink;

    std::vector<DeviceInformation> vcamList;
    if (FAILED(VCamUtils::GetVirtualCamera(vcamList)) || (vcamList.size() == 0))
    {
        LOG_COMMENT(L"No Virtual Camera found ");
        return strSymLink;
    }

    for (uint32_t i = 0; i < vcamList.size(); i++)
    {
        auto dev = vcamList[i];
        LOG_COMMENT(L"[%d] %s (%s) ", i + 1, dev.Id().data(), dev.Name().data());
    }
    LOG_COMMENT(L"select device ");
    uint32_t devIdx = 0;
    std::wcin >> devIdx;

    if (devIdx <= 0 || devIdx > vcamList.size())
    {
        LOG_COMMENT(L"Invalid device selection ");
        return strSymLink;
    }

    strSymLink = vcamList[devIdx - 1].Id();
    return strSymLink;
}

DeviceInformation SelectPhysicalCamera()
{
    winrt::hstring strSymLink;
    DeviceInformation devInfo{ nullptr };

    std::vector<DeviceInformation> camList;
    if (FAILED(VCamUtils::GetPhysicalCameras(camList)) || (camList.size() == 0))
    {
        LOG_COMMENT(L"No physical Camera found");
        return devInfo;
    }

    for (uint32_t i = 0; i < camList.size(); i++)
    {
        auto dev = camList[i];
        LOG_COMMENT(L"[%d] %s \n", i + 1, dev.Id().data());
    }
    LOG_COMMENT(L"select device");
    uint32_t devIdx = 0;
    std::wcin >> devIdx;

    if (devIdx <= 0 || devIdx > camList.size())
    {
        LOG_COMMENT(L"Invalid device selection");
        return devInfo;
    }

    return camList[devIdx - 1];
}

HRESULT SelectLifetimeAndAccess(_Out_ MFVirtualCameraLifetime* pLifetime, _Out_ MFVirtualCameraAccess* pAccess)
{
    LOG_COMMENT(L"\n select VCam lifetime: \n 1 - System \n 2 - Session \n 3 - quit \n");
    uint32_t select = 0;
    std::wcin >> select;

    wil::com_ptr_nothrow<IMFVirtualCamera> spVirtualCamera;
    switch (select)
    {
    case 1:
    {
        *pLifetime = MFVirtualCameraLifetime_System;
        break;
    }
    case 2:
    {
        *pLifetime = MFVirtualCameraLifetime_Session;
        break;
    }

    default:
        return E_FAIL;
    }

    LOG_COMMENT(L"\n select VCam user access: \n 1 - Current User \n 2 - All  \n 3 - quit \n");
    std::wcin >> select;
    switch (select)
    {
    case 1:
    {
        *pAccess = MFVirtualCameraAccess_CurrentUser;
        break;
    }
    case 2:
    {
        *pAccess = MFVirtualCameraAccess_AllUsers;
        break;
    }

    default:
        return E_FAIL;
    }

    return S_OK;
}

HRESULT SelectRegisterVirtualCamera(_Outptr_ IMFVirtualCamera** ppVirtualCamera)
{
    LOG_COMMENT(L"\n select option: \n 1 - VCam-SimpleMediaSource \n 2 - VCam-HWMediaSource \n 3 - VCam-AugmentedMediaSource \n 4 - quit \n");
    uint32_t select = 0;
    std::wcin >> select;
    MFVirtualCameraLifetime lifetime;
    MFVirtualCameraAccess access;

    wil::com_ptr_nothrow<IMFVirtualCamera> spVirtualCamera;
    switch (select)
    {
        case 1:
        {
            SimpleMediaSourceUT test;
            RETURN_IF_FAILED(SelectLifetimeAndAccess(&lifetime, &access));
            RETURN_IF_FAILED(test.CreateVirtualCamera(lifetime, access, ppVirtualCamera));
            break;
        }
        case 2: 
        {
            auto devInfo = SelectPhysicalCamera();
            
            HWMediaSourceUT test(devInfo.Id());
            RETURN_IF_FAILED(SelectLifetimeAndAccess(&lifetime, &access));
            RETURN_IF_FAILED(test.CreateVirtualCamera(devInfo.Name(), lifetime, access, ppVirtualCamera));
            break;
        }
        case 3:
        {
            auto devInfo = SelectPhysicalCamera();

            AugmentedMediaSourceUT test(devInfo.Id());
            RETURN_IF_FAILED(SelectLifetimeAndAccess(&lifetime, &access));
            RETURN_IF_FAILED(test.CreateVirtualCamera(devInfo.Name(), lifetime, access, ppVirtualCamera));
            break;
        }

        default:
            return E_FAIL;
    }

    return S_OK;
}

HRESULT SelectUnInstallVirtualCamera()
{
    winrt::hstring strSymlink = SelectVirtualCamera();
    if (!strSymlink.empty())
    {
        RETURN_IF_FAILED(VCamUtils::UnInstallVirtualCamera(strSymlink));
    }

    return S_OK;
}

void VCamAppUnInstall()
{
    LOG_COMMENT(L"Uninstall mode");
    VCamUtils::MSIUninstall(CLSID_VirtualCameraMediaSource);
}

HRESULT VCamApp()
{
    while (true)
    {
        LOG_COMMENT(L"\n select option: \n 1 - register \n 2 - remove \n 3 - TestVCam  \n 4 - TestCustomControl \n 5 - quit \n");
        int select = 0;
        std::wcin >> select;

        switch (select)
        {
            case 1: // Interactive install of virtual camera
            {
                wil::com_ptr_nothrow<IMFVirtualCamera> spVirtualCamera;
                RETURN_IF_FAILED_MSG(SelectRegisterVirtualCamera(&spVirtualCamera), "Register Virtual Camera failed");
                RETURN_IF_FAILED(spVirtualCamera->Shutdown());
                break;
            }

            case 2: // Interactive uninstall of virtual camera
            {
                HRESULT hr = SelectUnInstallVirtualCamera();
                if (FAILED(hr))
                {
                    LOG_ERROR(L"UnInstall Virtual Camera failed: 0x%08x", hr);
                }
                break;
            }
            

            case 3: // Stream test of selected virtual camera
            {
                winrt::hstring strSymlink = SelectVirtualCamera();

                if (!strSymlink.empty())
                {
                    wil::com_ptr_nothrow<IMFMediaSource> spMediaSource;
                    RETURN_IF_FAILED(VCamUtils::InitializeVirtualCamera(strSymlink.data(), &spMediaSource));
                    RETURN_IF_FAILED(RenderMediaSource(spMediaSource.get()));
                }
                break;
            }

            case 4: // TestCustomControl (implemented by SimpleMediaSource only, all other camera will failed)
            {
                winrt::hstring strSymlink = SelectVirtualCamera();
                if (!strSymlink.empty())
                {
                    wil::com_ptr_nothrow<IMFMediaSource> spMediaSource;
                    RETURN_IF_FAILED(VCamUtils::InitializeVirtualCamera(strSymlink.data(), &spMediaSource));
                    uint32_t colorMode = 0;
                    RETURN_IF_FAILED(SimpleMediaSourceUT::GetColorMode(spMediaSource.get(), &colorMode));
                    LOG_COMMENT(L"Current color mode: 0x%08x", colorMode);

                    LOG_COMMENT(L"Select color mode: ");
                    LOG_COMMENT(L" 1 - Red \n 2 - Green - \n 3 - Blue \n 4 - Gray");
                    uint32_t colorSelect = 0;
                    std::wcin >> colorSelect;
                    switch(colorSelect)
                    {
                    case 1:
                        colorMode = KSPROPERTY_SIMPLEMEDIASOURCE_CUSTOMCONTROL_COLORMODE_RED;
                        break;
                    case 2: 
                        colorMode = KSPROPERTY_SIMPLEMEDIASOURCE_CUSTOMCONTROL_COLORMODE_GREEN;
                        break;
                    case 3: 
                        colorMode = KSPROPERTY_SIMPLEMEDIASOURCE_CUSTOMCONTROL_COLORMODE_BLUE;
                        break;
                    case 4: 
                        colorMode = KSPROPERTY_SIMPLEMEDIASOURCE_CUSTOMCONTROL_COLORMODE_GRAYSCALE;
                        break;
                    default: 
                        colorMode = 0;
                        LOG_WARNING(L"Invalid color mode!");
                        break;
                    }
                    if (colorMode != 0)
                    {
                        RETURN_IF_FAILED(SimpleMediaSourceUT::SetColorMode(spMediaSource.get(), colorMode));
                    }
                }
                break;
            }

            default:
                return S_OK;
        }
    }

    return S_OK;
}

int wmain(int argc, wchar_t* argv[])
{
    winrt::init_apartment();
    EnableVTMode();
    wil::SetResultLoggingCallback(WilFailureLog);

    LOG_COMMENT(L"Virtual Camera simple application !");
    RETURN_IF_FAILED(MFStartup(MF_VERSION));

    if (argc == 2)
    {
        if (_wcsicmp(argv[1], L"/Uninstall") == 0)
        {
            // MSI Uninstall mode
            VCamAppUnInstall();
            return 0;
        }
        else if (_wcsicmp(argv[1], L"/?") == 0)
        {
            LOG_COMMENT(L"\n default - Simple application to install//test//remove VirtualCamera, \n run  /uninstall  to test MIS uninstallation function.  ");
        }
    }
    else
    {
        // VCam application
        RETURN_IF_FAILED(VCamApp());
    }

    return S_OK;
}
