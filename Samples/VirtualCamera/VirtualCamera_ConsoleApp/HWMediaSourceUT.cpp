#include "pch.h"
#include "HWMediaSourceUT.h"
#include "VirtualCameraWin32.h"
//#include "MediaCaptureUtils.h"
#include "VCamutils.h"

HRESULT HWMediaSourceUT::TestMediaSource()
{
    wil::com_ptr_nothrow<IMFAttributes> spAttributes;
    RETURN_IF_FAILED(MFCreateAttributes(&spAttributes, 1));
    RETURN_IF_FAILED(spAttributes->SetString(VCAM_DEVICE_INFO, m_devSymlink.data()));

    RETURN_IF_FAILED(TestMediaSourceRegistration(CLSID_SimpleMediaSourceWin32, spAttributes.get()));

    return S_OK;
}

HRESULT HWMediaSourceUT::TestMediaSourceStream()
{
    wil::com_ptr_nothrow<IMFAttributes> spAttributes;
    RETURN_IF_FAILED(MFCreateAttributes(&spAttributes, 1));
    RETURN_IF_FAILED(spAttributes->SetString(VCAM_DEVICE_INFO, m_devSymlink.data()));

    wil::com_ptr_nothrow<IMFMediaSource> spMediaSource;
    RETURN_IF_FAILED(_CoCreateAndActivateMediaSource(CLSID_SimpleMediaSourceWin32, spAttributes.get(), &spMediaSource));

    wil::com_ptr_nothrow<IMFMediaSource> spCameraSource;
    RETURN_IF_FAILED(_CreateCameraDeviceSource(m_devSymlink, &spCameraSource));

    // get the stream count from the actual camrea.
    wil::com_ptr_nothrow<IMFPresentationDescriptor> spCameraPD;
    DWORD cameraStreamCount = 0;
    RETURN_IF_FAILED(spCameraSource->CreatePresentationDescriptor(&spCameraPD));
    RETURN_IF_FAILED(spCameraPD->GetStreamDescriptorCount(&cameraStreamCount));

    // Validate the stream count from HWMediaSource matches the actual camera.
    wil::com_ptr_nothrow<IMFPresentationDescriptor> spPD;
    DWORD streamCount = 0;
    RETURN_IF_FAILED(spMediaSource->CreatePresentationDescriptor(&spPD));
    RETURN_IF_FAILED(spPD->GetStreamDescriptorCount(&streamCount));
    if (streamCount != cameraStreamCount)
    {
        LOG_ERROR(L"Unexpected stream count: %d (expected: %d) \n", streamCount, cameraStreamCount);
        RETURN_HR(E_TEST_FAILED);
    }

    // Validate Streaming with sourcereader
    RETURN_IF_FAILED(MFCreateAttributes(&spAttributes, 1));
    wil::com_ptr_nothrow<IMFSourceReader> spSourceReader;
    RETURN_IF_FAILED(MFCreateSourceReaderFromMediaSource(spMediaSource.get(), spAttributes.get(), &spSourceReader));

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
            LOG_COMMENT(L"Skip stream test on streamCateogry: PINNAME_IMAGE");
            continue;
        }

        RETURN_IF_FAILED(spStreamDescriptor->GetMediaTypeHandler(&spMediaTypeHandler));
        RETURN_IF_FAILED(spMediaTypeHandler->GetMediaTypeCount(&mediaTypeCount));

        for (unsigned int mtIdx = 0; mtIdx <= 0; mtIdx++)
        {
            RETURN_IF_FAILED(spSourceReader->SetStreamSelection(streamIdx, true));

            wil::com_ptr_nothrow<IMFMediaType> spMediaType;
            RETURN_IF_FAILED(spMediaTypeHandler->GetMediaTypeByIndex(mtIdx, &spMediaType));

            LOG_COMMENT(L"Set mediatype on stream: %d / %d ", mtIdx, mediaTypeCount);
            LogMediaType(spMediaType.get());

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

HRESULT HWMediaSourceUT::TestCreateVirtualCamera()
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

    RETURN_IF_FAILED(CreateVirutalCamera(L"test", &spVirtualCamera));

}

////////////////////////////////////////////////////////////////////
// helper function
HRESULT HWMediaSourceUT::CreateVirutalCamera(winrt::hstring const& postfix, IMFVirtualCamera** ppVirtualCamera)
{
    wchar_t friendlyname[MAX_PATH];
    int cch = swprintf(friendlyname, MAX_PATH, L"HWVCamMediaSource-%s", postfix.data());
    if (cch == -1)
    {
        return HRESULT_FROM_WIN32(ERROR_INVALID_DATA);
    }
    
    wil::com_ptr_nothrow<IMFAttributes> spAttributes;
    RETURN_IF_FAILED(MFCreateAttributes(&spAttributes, 1));
    RETURN_IF_FAILED(spAttributes->SetString(VCAM_DEVICE_INFO, m_devSymlink.data()));

    RETURN_IF_FAILED(VCamUtils::RegisterVirtualCamera(SIMPLEMEDIASOURCE_WIN32, friendlyname, m_devSymlink, spAttributes.get(), ppVirtualCamera));

    return S_OK;
}

////////////////////////////////////////////////////////////////////
// 