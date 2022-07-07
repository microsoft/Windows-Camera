//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#include "pch.h"
#include "MediaSourceUT_Common.h"
#include "VCamUtils.h"
#include "MediaCaptureutils.h"
#include "EVRHelper.h"
#include "VirtualCameraMediaSource.h"

namespace VirtualCameraTest::impl
{
    HRESULT MediaSourceUT_Common::TestMediaSource()
    {
        wil::com_ptr_nothrow<IMFAttributes> spAttributes;
        RETURN_IF_FAILED(CreateSourceAttributes(&spAttributes));
        RETURN_IF_FAILED(TestMediaSourceRegistration(m_clsid, spAttributes.get()));

        return S_OK;
    }

    /// <summary>
    /// 1. Test Validate MediaSource dll is register correctly, by running CoCreateInstance to create
    /// the media source.
    /// 2. Validate the MediaSource can be activate
    /// 3. Validate the MediaSource implements all required interface
    /// 4. Validate media source frame are produce using sourcereader.
    /// </summary>
    /// <param name="clsid"></param>
    /// <param name="pAttributes"></param>
    /// <returns></returns>
    HRESULT MediaSourceUT_Common::TestMediaSourceRegistration(GUID clsid, IMFAttributes* pAttributes)
    {
        wil::com_ptr_nothrow<IMFMediaSource> spMediaSource;
        RETURN_IF_FAILED(CoCreateAndActivateMediaSource(clsid, pAttributes, &spMediaSource));
        RETURN_IF_FAILED(ValidateInterfaces(spMediaSource.get()));

        return S_OK;
    }

    HRESULT MediaSourceUT_Common::ValidateVirtualCamera(_In_ IMFVirtualCamera* spVirtualCamera)
    {
        LOG_COMMENT(L"Get mediasource");
        wil::com_ptr_nothrow<IMFMediaSource> spMediaSrc;
        RETURN_IF_FAILED(spVirtualCamera->GetMediaSource(&spMediaSrc));
        LOG_COMMENT(L"Succeeded (%p)!", spMediaSrc.get());

        wil::unique_cotaskmem_string pwszSymLink;
        UINT32 cch;
        LOG_COMMENT(L"Virtual camera Symboliclinkname: ");
        RETURN_IF_FAILED(spVirtualCamera->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, &pwszSymLink, &cch));
        LOG_COMMENT(L"%s ", pwszSymLink.get());

        wil::unique_cotaskmem_string pwszFriendlyName;
        LOG_COMMENT(L"Virtual camera Friendlyname: ");
        RETURN_IF_FAILED(spVirtualCamera->GetAllocatedString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, &pwszFriendlyName, &cch));
        LOG_COMMENT(L"%s ", pwszFriendlyName.get());

        LOG_COMMENT(L"Get interface class guid");
        wistd::unique_ptr<BYTE[]> spBuffer;
        ULONG cbBufferSize = 0;

        RETURN_IF_FAILED(VCamUtils::GetDeviceInterfaceProperty(
            pwszSymLink.get(),
            DEVPKEY_DeviceInterface_ClassGuid,
            DEVPROP_TYPE_GUID,
            spBuffer,
            cbBufferSize));

        LOG_COMMENT(L"DeviceInterfaceClassGuid: %s ", winrt::to_hstring((GUID&)(spBuffer)).data());

        LOG_COMMENT(L"Validate VirtualCamera ConfigAppPfn ... ");
        winrt::hstring strDEVPKEY_DeviceInterface_FSM_VirtualCamera_ConfigAppPfn(L"{b3e34238-3393-4bd0-adfc-39dc9e32bcf1} 16");
        winrt::Windows::Foundation::IReference<winrt::hstring> val;
        CMediaCaptureUtils::GetDeviceProperty(pwszSymLink.get(), strDEVPKEY_DeviceInterface_FSM_VirtualCamera_ConfigAppPfn, DeviceInformationKind::DeviceInterface, val);

        if (val == nullptr || val.Value().empty())
        {
            LOG_WARNING(L"MediaSource Attribute is missing ConfigAppPfn");
        }
        else
        {
            LOG_COMMENT(L"ConfigAppPfn: %s ", val.Value().data());
        }

        LOG_COMMENT(L"Validate virtual camera present in device enumeration ...");
        wil::com_ptr_nothrow<IMFActivate> spActivate;
        RETURN_IF_FAILED_MSG(VCamUtils::GetCameraActivate(pwszSymLink.get(), &spActivate), "Fail to find VCam from camera enumeration");
        LOG_COMMENT(L"succeeded!");

        return S_OK;
    }

    HRESULT MediaSourceUT_Common::CoCreateAndActivateMediaSource(GUID clsid, IMFAttributes* pAttributes, IMFMediaSource** ppMediaSource)
    {
        RETURN_HR_IF_NULL(E_POINTER, ppMediaSource);
        *ppMediaSource = nullptr;

        LOG_COMMENT(L"CoCreate mediasource: %s ... ", winrt::to_hstring(clsid).data());
        wil::com_ptr_nothrow<IMFActivate> spMFActivate;
        RETURN_IF_FAILED(CoCreateInstance(
            clsid,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&spMFActivate)));
        LOG_COMMENT(L"CoCreate mediasource succeeded ");

        // Copy attributes into Activate object
        if (pAttributes)
        {
            uint32_t cItems = 0;
            RETURN_IF_FAILED(pAttributes->GetCount(&cItems));
            LOG_COMMENT(L"Set activate attributes: %d ", cItems);
            for (uint32_t i = 0; i < cItems; i++)
            {
                wil::unique_prop_variant prop;
                GUID guidKey = GUID_NULL;
                RETURN_IF_FAILED(pAttributes->GetItemByIndex(i, &guidKey, &prop));
                LOG_COMMENT(L"%i - %s ", i, winrt::to_hstring(guidKey).data());
                RETURN_IF_FAILED(spMFActivate->SetItem(guidKey, prop));
            }
        }

        LOG_COMMENT(L"Activate media source ... ");
        RETURN_IF_FAILED(spMFActivate->ActivateObject(IID_PPV_ARGS(ppMediaSource)));
        LOG_COMMENT(L"activate mediasource with succeeded! \n");

        return S_OK;
    }

    HRESULT MediaSourceUT_Common::CreateCameraDeviceSource(winrt::hstring const& devSymlink, IMFMediaSource** ppMediaSource)
    {
        RETURN_HR_IF_NULL(E_POINTER, ppMediaSource);
        *ppMediaSource = nullptr;

        wil::com_ptr_nothrow<IMFAttributes> spCreateAttribute;
        RETURN_IF_FAILED(MFCreateAttributes(&spCreateAttribute, 2));
        RETURN_IF_FAILED(spCreateAttribute->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID));
        RETURN_IF_FAILED(spCreateAttribute->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, devSymlink.data()));

        RETURN_IF_FAILED(MFCreateDeviceSource(spCreateAttribute.get(), ppMediaSource));
        return S_OK;
    }

    HRESULT MediaSourceUT_Common::TestMediaSourceStream(IMFMediaSource* pMediaSource)
    {
        wil::com_ptr_nothrow<IMFPresentationDescriptor> spPD;
        DWORD streamCount = 0;
        RETURN_IF_FAILED(pMediaSource->CreatePresentationDescriptor(&spPD));
        RETURN_IF_FAILED(spPD->GetStreamDescriptorCount(&streamCount));

        // Validate Streaming with sourcereader
        wil::com_ptr_nothrow<IMFAttributes> spAttributes;
        RETURN_IF_FAILED(MFCreateAttributes(&spAttributes, 1));

        wil::com_ptr_nothrow<IMFSourceReader> spSourceReader;
        RETURN_IF_FAILED(MFCreateSourceReaderFromMediaSource(pMediaSource, spAttributes.get(), &spSourceReader));

        for (unsigned int streamIdx = 0; streamIdx < streamCount; streamIdx++)
        {
            DWORD mediaTypeCount = 0;
            BOOL selected = FALSE;
            wil::com_ptr_nothrow<IMFStreamDescriptor> spStreamDescriptor;
            wil::com_ptr_nothrow<IMFMediaTypeHandler> spMediaTypeHandler;

            LOG_COMMENT(L"Test streaming from stream index: %d / %d", streamIdx, streamCount);
            RETURN_IF_FAILED(spPD->GetStreamDescriptorByIndex(streamIdx, &selected, &spStreamDescriptor));

            GUID category;
            RETURN_IF_FAILED(spStreamDescriptor->GetGUID(MF_DEVICESTREAM_STREAM_CATEGORY, &category));
            if (category == PINNAME_IMAGE)
            {
                LOG_WARNING(L"Skip stream test on streamCateogry: PINNAME_IMAGE");
                continue;
            }

            // Set sampe allocator 
            DWORD dwStreamId;
            RETURN_IF_FAILED(spStreamDescriptor->GetStreamIdentifier(&dwStreamId));

            wil::com_ptr_nothrow<IMFSampleAllocatorControl> spAllocatorControl;
            RETURN_IF_FAILED(pMediaSource->QueryInterface(IID_PPV_ARGS(&spAllocatorControl)));

            MFSampleAllocatorUsage eUsage;
            DWORD dwInputStreamId;
            RETURN_IF_FAILED(spAllocatorControl->GetAllocatorUsage(dwStreamId, &dwInputStreamId, &eUsage));
            if (eUsage == MFSampleAllocatorUsage_UsesProvidedAllocator)
            {
                wil::com_ptr_nothrow<IMFVideoSampleAllocator>spSampleAllocator;
                RETURN_IF_FAILED(MFCreateVideoSampleAllocatorEx(IID_PPV_ARGS(&spSampleAllocator)));
                wil::com_ptr_nothrow<IUnknown> spUnknown;
                RETURN_IF_FAILED(spSampleAllocator->QueryInterface(&spUnknown));
                RETURN_IF_FAILED(spAllocatorControl->SetDefaultAllocator(dwStreamId, spUnknown.get()));
            }

            RETURN_IF_FAILED(spStreamDescriptor->GetMediaTypeHandler(&spMediaTypeHandler));
            RETURN_IF_FAILED(spMediaTypeHandler->GetMediaTypeCount(&mediaTypeCount));

            for (unsigned int mtIdx = 0; mtIdx < mediaTypeCount; mtIdx++)
            {
                RETURN_IF_FAILED(spSourceReader->SetStreamSelection(streamIdx, true));

                wil::com_ptr_nothrow<IMFMediaType> spMediaType;
                RETURN_IF_FAILED(spMediaTypeHandler->GetMediaTypeByIndex(mtIdx, &spMediaType));

                LOG_COMMENT(L"Set mediatype on stream: %d / %d ", mtIdx, mediaTypeCount);
                LOG_COMMENT(LogMediaType(spMediaType.get()).data());

                RETURN_IF_FAILED(spSourceReader->SetCurrentMediaType(streamIdx, NULL, spMediaType.get()));
                RETURN_IF_FAILED(ValidateStreaming(spSourceReader.get(), streamIdx, spMediaType.get()));

                wil::com_ptr_nothrow<IMFMediaType> spSetMediaType;
                RETURN_IF_FAILED(spSourceReader->GetCurrentMediaType(streamIdx, &spSetMediaType));

                if (MFCompareFullToPartialMediaType(spMediaType.get(), spSetMediaType.get()))
                {
                    LOG_COMMENT(L"Media type set succeeded.");
                }
                else
                {
                    LOG_ERROR(L"Media type set failed.");
                }
                RETURN_IF_FAILED(spSourceReader->SetStreamSelection(streamIdx, false));  // trigger SetStreamState(stop)
            }
        }
        return S_OK;
    }

    HRESULT MediaSourceUT_Common::ValidateKsControl_UnSupportedControl(IKsControl* pKsControl)
    {
        LOG_COMMENT(L"Validate KSProperty return ERROR_SET_NOT_FOUND when control is not suported ...");
        KSPROPERTY ksProperty = { 0 };
        ksProperty.Flags = KSPROPERTY_TYPE_GET;
        winrt::com_array<BYTE> arr(8);
        ULONG byteReturns = 0;

        HRESULT hr = pKsControl->KsProperty(
            &ksProperty,
            sizeof(KSPROPERTY),
            arr.data(),
            arr.size(),
            &byteReturns);

        if (hr == HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND))
        {
            LOG_SUCCESS(L"Function return ERROR_SET_NOT_FOUND on invalid control");
        }
        else if (FAILED(hr))
        {
            LOG_WARNING(L"Function didn't return the expected error: 0x%08x (expected: 0x%08x)", hr, HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND));
        }
        else
        {
            LOG_ERROR_RETURN(E_TEST_FAILED, L"Function call didn't fail with expected error (or any error): 0x%08x (expected: 0x%08x)", hr, HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND));
        }

        return S_OK;
    }

    ////////////////////////////////////////////////////// 
    // private functions
    //
    HRESULT MediaSourceUT_Common::ValidateInterfaces(IMFMediaSource* pMediaSource)
    {
        // Validate all require interface 
        LOG_COMMENT(L"Validate required interfaces from activated mediasource ...");
        wil::com_ptr_nothrow<IMFMediaSourceEx> spMediaSourceEx;
        RETURN_IF_FAILED_MSG(pMediaSource->QueryInterface(IID_PPV_ARGS(&spMediaSourceEx)), "QI IMFMediaSourceEx failed!\n");
        LOG_SUCCESS(L"QI IMFMediaSourceEx Succeeded! ");

        wil::com_ptr_nothrow<IMFMediaEventGenerator> spMediaEventGenerator;
        RETURN_IF_FAILED_MSG(pMediaSource->QueryInterface(IID_PPV_ARGS(&spMediaEventGenerator)), "QI IMFMediaEventGenerator failed! \n");
        LOG_SUCCESS(L"QI IMFMediaEventGenerator Succeeded ");

        wil::com_ptr_nothrow<IMFGetService> spMFGetService;
        RETURN_IF_FAILED_MSG(pMediaSource->QueryInterface(IID_PPV_ARGS(&spMFGetService)), "QI IMFGetService failed! \n");
        LOG_SUCCESS(L"QI IMFGetService Succeeded ");

        wil::com_ptr_nothrow<IKsControl> spKsControl;
        RETURN_IF_FAILED_MSG(pMediaSource->QueryInterface(IID_PPV_ARGS(&spKsControl)), "QI IKsControl failed! \n");
        LOG_SUCCESS(L"QI IKsControl Succeeded ");

        wil::com_ptr_nothrow<IMFSampleAllocatorControl> spMFSampleAllocatorControl;
        RETURN_IF_FAILED_MSG(pMediaSource->QueryInterface(IID_PPV_ARGS(&spMFSampleAllocatorControl)), "QI IMFSampleAllocatorControl failed! \n");
        LOG_SUCCESS(L"QI IMFSampleAllocatorControl Succeeded \n");

        return S_OK;
    }

    HRESULT MediaSourceUT_Common::ValidateStreaming(IMFSourceReader* pSourceReader, DWORD streamIdx, IMFMediaType* pMediaType)
    {
        uint32_t validSample = 0;
        uint32_t invalidSample = 0;
        bool invalidStream = false;

        wistd::unique_ptr<EVRHelper> spEVRHelper(new (std::nothrow)EVRHelper());
        if (FAILED(spEVRHelper->Initialize(pMediaType)))
        {
            // Unsupported rendering type without color conversion
            // validate without visual rendering.
            spEVRHelper = nullptr;
        }

        LOG_COMMENT(L"Read 30 samples from stream index: %d ", streamIdx);
        for (uint32_t i = 0; i < 30; i++)
        {
            DWORD actualStreamIdx = 0;
            DWORD flags = 0;
            LONGLONG llTimeStamp = 0;
            wil::com_ptr_nothrow<IMFSample> spSample;

            RETURN_IF_FAILED(pSourceReader->ReadSample(
                streamIdx,                      // Stream index.
                0,                              // Flags.
                &actualStreamIdx,               // Receives the actual stream index. 
                &flags,                         // Receives status flags.
                &llTimeStamp,                   // Receives the time stamp.
                &spSample                       // Receives the sample or NULL.
            ));

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
            LOG_ERROR_RETURN(E_TEST_FAILED, L"Received valid: %d, invalid %d frames, sample from wrong stream: %s ", validSample, invalidSample, (invalidStream ? L"true" : L"false"));
        }
        else
        {
            LOG_SUCCESS(L"Received valid: %d, invalid %d frames, sample from wrong stream: %s ", validSample, invalidSample, (invalidStream ? L"true" : L"false"));
        }

        return S_OK;
    }

    winrt::hstring MediaSourceUT_Common::LogMediaType(IMFMediaType* pMediaType)
    {
        if (pMediaType)
        {
            GUID majorType = GUID_NULL;
            pMediaType->GetGUID(MF_MT_MAJOR_TYPE, &majorType);
            winrt::hstring strMajorType = (majorType == MFMediaType_Video) ? L"MFMediaType_Video" : winrt::to_hstring(majorType);

            GUID subtype = GUID_NULL;
            pMediaType->GetGUID(MF_MT_SUBTYPE, &subtype);

            UINT32 width, height;
            MFGetAttributeSize(pMediaType, MF_MT_FRAME_SIZE, &width, &height);

            UINT32 num, den;
            MFGetAttributeRatio(pMediaType, MF_MT_FRAME_RATE, &num, &den);

            return winrt::StringFormat(L"majorType: %s, subType %s, framesize: %dx%d, framerate: %d:%d",
                strMajorType.data(),
                winrt::to_hstring(subtype).data(),
                width, height,
                num, den);
        }
        else
        {
            return winrt::hstring(L"MediaType: nullptr");
        }
    }
}

