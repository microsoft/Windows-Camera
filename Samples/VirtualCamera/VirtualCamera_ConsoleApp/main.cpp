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

#include "winrt\Windows.ApplicationModel.Core.h"

void __stdcall WilFailureLog(_In_ const wil::FailureInfo& failure) WI_NOEXCEPT
{
    LOG_ERROR(L"%S(%d):%S, hr=0x%08X, msg=%s",
        failure.pszFile, failure.uLineNumber, failure.pszFunction,
        failure.hr, (failure.pszMessage) ? failure.pszMessage : L"");
}

HRESULT ValidateStreaming(IMFSourceReader* pSourceReader, DWORD streamIdx, IMFMediaType* pMediaType)
{
    // Create EVR
    wistd::unique_ptr<EVRHelper> spEVRHelper(new (std::nothrow)EVRHelper());
    if (FAILED(spEVRHelper->Initialize(pMediaType)))
    {
        spEVRHelper = nullptr;
    }

    bool invalidStream = false;
    uint32_t validSample = 0;
    uint32_t invalidSample = 0;

    for (int i = 0; i <= 500; i++)
    {
        //wprintf(L"reading sample: %d ", i);
        DWORD actualStreamIdx = 0;
        DWORD flags = 0;
        LONGLONG llTimeStamp = 0;
        wil::com_ptr_nothrow<IMFSample> spSample;
        RETURN_IF_FAILED(pSourceReader->ReadSample(
            streamIdx,
            0,                              // Flags.
            &actualStreamIdx,               // Receives the actual stream index. 
            &flags,                         // Receives status flags.
            &llTimeStamp,                   // Receives the time stamp.
            &spSample                       // Receives the sample or NULL.
        ));

        if (i % 100 == 0)
        {
            LOG_COMMENT(L"Frame recieved: %d ...", i + 1);
        }

        if (streamIdx != actualStreamIdx)
        {
            invalidStream = true;
        }
        else if (spSample.get() != nullptr)
        {
            validSample++;
            if (spEVRHelper)
            {
                spEVRHelper->WriteSample(spSample.get());
            }
        }
        else
        {
            invalidSample++;
        }
    }

    if (validSample == 0 || invalidStream)
    {
        LOG_ERROR_RETURN(E_TEST_FAILED, L"Received valid: %d, invalid %d frames, sample from wrong stream: %s \n", validSample, invalidSample, (invalidStream ? L"true" : L"false"));
    }
    else
    {
        LOG_COMMENT(L"Received valid: %d, invalid %d frames, sample from wrong stream: %s \n", validSample, invalidSample, (invalidStream ? L"true" : L"false"));
    }

    return S_OK;
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
        LOG_COMMENT(L"Select stream index: 0 - %d", streamCount - 1);
        DWORD streamIdx = 0;
        std::wcin >> streamIdx;

        if (streamIdx >= streamCount)
        {
            // invalid stream selection, exit selection
            break;
        }

        wil::com_ptr_nothrow<IMFStreamDescriptor> spStreamDescriptor;
        BOOL selected = FALSE;
        RETURN_IF_FAILED(spPD->GetStreamDescriptorByIndex(streamIdx, &selected, &spStreamDescriptor));

        DWORD streamId = 0;
        spStreamDescriptor->GetStreamIdentifier(&streamId);
        LOG_COMMENT(L"Selected Streamid %d", streamId);

        wil::com_ptr_nothrow<IMFMediaTypeHandler> spMediaTypeHandler;
        DWORD mtCount = 0;
        RETURN_IF_FAILED(spStreamDescriptor->GetMediaTypeHandler(&spMediaTypeHandler));
        RETURN_IF_FAILED(spMediaTypeHandler->GetMediaTypeCount(&mtCount));

        LOG_COMMENT(L"Select media type: 0 - %d", mtCount);
        for (unsigned int i = 0; i < mtCount; i++)
        {
            wil::com_ptr_nothrow<IMFMediaType> spMediaType;
            RETURN_IF_FAILED(spMediaTypeHandler->GetMediaTypeByIndex(i, &spMediaType));

            GUID majorType = GUID_NULL;
            spMediaType->GetGUID(MF_MT_MAJOR_TYPE, &majorType);
            winrt::hstring strMajorType = (majorType == MFMediaType_Video) ? L"MFMediaType_Video" : winrt::to_hstring(majorType);

            GUID subtype = GUID_NULL;
            spMediaType->GetGUID(MF_MT_SUBTYPE, &subtype);

            UINT32 width, height;
            MFGetAttributeSize(spMediaType.get(), MF_MT_FRAME_SIZE, &width, &height);

            UINT32 num, den;
            MFGetAttributeRatio(spMediaType.get(), MF_MT_FRAME_RATE, &num, &den);


            LOG_COMMENT(L"%d - majorType: %s, subType: %s, framesize: %dx%d, framerate: %d/%d",
                i,
                strMajorType.data(),
                winrt::to_hstring(subtype).data(),
                width, height,
                num, den);
        }

        DWORD mtIdx = 0;
        std::wcin >> mtIdx;
        if (mtIdx >= mtCount)
        {
            // invalid media type selection, exit selection
            break;
        }

        wil::com_ptr_nothrow<IMFMediaType> spMediaType;
        RETURN_IF_FAILED(spMediaTypeHandler->GetMediaTypeByIndex(mtIdx, &spMediaType));

        // Test Stream
        RETURN_IF_FAILED(spSourceReader->SetStreamSelection(streamIdx, TRUE));
        RETURN_IF_FAILED(spSourceReader->SetCurrentMediaType(streamIdx, NULL, spMediaType.get()));
        RETURN_IF_FAILED_MSG(ValidateStreaming(spSourceReader.get(), streamIdx, spMediaType.get()), "Streaming validation failed");
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

    if (devIdx <= 0 && devIdx > vcamList.size())
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

    if (devIdx <= 0 && devIdx > camList.size())
    {
        LOG_COMMENT(L"Invalid device selection");
        return devInfo;
    }

    return camList[devIdx - 1];
}

HRESULT SelectRegisterVirtualCamera(_Outptr_ IMFVirtualCamera** ppVirtualCamera)
{
    while (true)
    {
        LOG_COMMENT(L"\n select opitions: \n 1 - VCam-SimpleMediaSource \n 2 - VCam-HWMediaSource \n 3 - quit \n");
        uint32_t select = 0;
        std::wcin >> select;

        wil::com_ptr_nothrow<IMFVirtualCamera> spVirtualCamera;
        switch (select)
        {
        case 1:
        {
            SimpleMediaSourceUT test;
            RETURN_IF_FAILED(test.CreateVirtualCamera(ppVirtualCamera));
            break;
        }
        case 2: 
        {
            auto devInfo = SelectPhysicalCamera();
            
            HWMediaSourceUT test(devInfo.Id());
            RETURN_IF_FAILED(test.CreateVirutalCamera(devInfo.Name(), ppVirtualCamera));
            break;
        }

        default:
            return E_FAIL;
        }
        break;
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
        LOG_COMMENT(L"\n select opitions: \n 1 - register \n 2 - remove \n 3 - TestVCam  \n 4 - TestCustomControl \n 5 - quit \n");
        int select = 0;
        std::wcin >> select;

        switch (select)
        {
            case 1: // Interactive install of virutal camera
            {
                wil::com_ptr_nothrow<IMFVirtualCamera> spVirtualCamera;
                RETURN_IF_FAILED_MSG(SelectRegisterVirtualCamera(&spVirtualCamera), "Register Virutal Camera failed");
                RETURN_IF_FAILED(spVirtualCamera->Shutdown());
                break;
            }
            case 2: // Interactive uninstall of virutal camera
            {
                HRESULT hr = SelectUnInstallVirtualCamera();
                if (FAILED(hr))
                {
                    LOG_ERROR(L"UnInstall Virutal Camera failed: 0x%08x", hr);
                }
            }
            break;

            case 3: // Stream test of selected virutal camera
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
                    uint32_t select = 0;
                    std::wcin >> select;
                    switch(select)
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
                        LOG_WARN(L"Invalid color mode!");
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
                return  S_OK;
        }
    }

    return S_OK;
}

HRESULT TestMode()
{
    while (true)
    {
        LOG_COMMENT(L"\n select opitions: \n 1 - HWMediaSourceUT  \n 2 - SimpleMediaSourceUT \n 3 - quit \n");
        int select = 0;
        std::wcin >> select;

        switch (select)
        {
        case 1:  // Run unit test on HWMediaSource
        {
            auto devInfo = SelectPhysicalCamera();
            if (devInfo)
            {
                HWMediaSourceUT test(devInfo.Id());
                RETURN_IF_FAILED(test.TestMediaSource());
                RETURN_IF_FAILED(test.TestMediaSourceStream());
            }
            break;
        }

        case 2: // Run unit test on SimpleMediaSource
        {
            SimpleMediaSourceUT test;
            RETURN_IF_FAILED(test.TestMediaSource());
            RETURN_IF_FAILED(test.TestMediaSourceStream());
            RETURN_IF_FAILED(test.TestKSProperty());
            RETURN_IF_FAILED(test.TestVirtualCamera());
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

    LOG_COMMENT(L"Hello, start...%d !\n", argc);
    RETURN_IF_FAILED(MFStartup(MF_VERSION));

    if (argc == 2)
    {
        if (_wcsicmp(argv[1], L"/Test") == 0)
        {
            // MediaSource / VCam test mode
            RETURN_IF_FAILED(TestMode());
        }
        else if (_wcsicmp(argv[1], L"/Uninstall") == 0)
        {
            // MSI Uninstall mode
            VCamAppUnInstall();
            return 0;
        }
    }
    else
    {
        // VCam application
        RETURN_IF_FAILED(VCamApp());
    }

    return S_OK;
}
