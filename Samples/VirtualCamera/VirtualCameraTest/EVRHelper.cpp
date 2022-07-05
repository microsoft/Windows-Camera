//
// Copyright (C) Microsoft Corporation. All rights reserved.
//
#include "pch.h"
#include "EVRHelper.h"

LRESULT CALLBACK WndProc(HWND hwnd,
    UINT  uMsg,
    WPARAM wParam,
    LPARAM lParam)
{
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

EVRHelper::~EVRHelper()
{
    if (m_spEVRSinkWriter)
    {
        m_spEVRSinkWriter->Flush((DWORD)MF_SINK_WRITER_ALL_STREAMS);
        m_spEVRSinkWriter = nullptr;
    }
}

HRESULT EVRHelper::Initialize(IMFMediaType* pMediaType)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pMediaType);

    GUID subType = GUID_NULL;
    RETURN_IF_FAILED(pMediaType->GetGUID(MF_MT_SUBTYPE, &subType));

    UINT32 width = 0;
    UINT32 height = 0;
    RETURN_IF_FAILED(MFGetAttributeRatio(pMediaType, MF_MT_FRAME_SIZE, &width, &height));

    wil::com_ptr_nothrow<IMFMediaType>spRenderMT;
    RETURN_IF_FAILED(MFCreateMediaType(&spRenderMT));

    // default
    RETURN_IF_FAILED(spRenderMT->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
    RETURN_IF_FAILED(spRenderMT->SetUINT32(MF_MT_REALTIME_CONTENT, TRUE));
    spRenderMT->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    spRenderMT->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
    MFSetAttributeRatio(spRenderMT.get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

    RETURN_IF_FAILED(spRenderMT->SetGUID(MF_MT_SUBTYPE, subType));
    RETURN_IF_FAILED(MFSetAttributeSize(spRenderMT.get(), MF_MT_FRAME_SIZE, width, height));

    RETURN_IF_FAILED(_CreateRenderWindow());
    RETURN_IF_FAILED(_CreateEVRSink(spRenderMT.get()));

    return S_OK;
}

HRESULT EVRHelper::WriteSample(IMFSample* pSample)
{
    RETURN_HR_IF_NULL(MF_E_INVALIDREQUEST, m_spEVRSinkWriter);

    if (m_spCopyMFT == nullptr)
    {
        RETURN_IF_FAILED(MFCreateSampleCopierMFT(&m_spCopyMFT));
        RETURN_IF_FAILED(m_spCopyMFT->SetInputType(0, m_spOutputMediaType.get(), 0));
        RETURN_IF_FAILED(m_spCopyMFT->SetOutputType(0, m_spOutputMediaType.get(), 0));
        RETURN_IF_FAILED(m_spCopyMFT->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0));
    }

    if (SUCCEEDED(_OffsetTimestamp(pSample)))
    {
        MFT_OUTPUT_DATA_BUFFER buffer = {};
        wil::com_ptr_nothrow<IMFSample> spOutputSample;
        RETURN_IF_FAILED(m_spSampleAllocator->AllocateSample(&spOutputSample));
        // Process the frame.
        buffer.pSample = spOutputSample.get();
        RETURN_IF_FAILED(m_spCopyMFT->ProcessInput(0, pSample, 0));
        DWORD dwStatus;
        RETURN_IF_FAILED(m_spCopyMFT->ProcessOutput(0, 1, &buffer, &dwStatus));

        RETURN_IF_FAILED(m_spEVRSinkWriter->WriteSample(0, spOutputSample.get()));
    }
    return S_OK;
}

HRESULT EVRHelper::_CreateRenderWindow()
{
    // create preview Windows
    WNDCLASS            wc = { 0 };
    ATOM                atom;

    wc.lpfnWndProc = (WNDPROC)WndProc;
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = L"Capture Preview";
    wc.lpszMenuName = NULL;

    atom = RegisterClass(&wc);
    HRESULT hr = HRESULT_FROM_WIN32(GetLastError());
    RETURN_HR_IF(hr, ((atom == NULL) && FAILED(hr) && (hr != HRESULT_FROM_WIN32(ERROR_CLASS_ALREADY_EXISTS))));

    m_PreviewHwnd = wil::unique_hwnd(CreateWindow(
        L"Capture Preview",
        L"Capture Preview",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        640,
        480,
        NULL,
        NULL,
        GetModuleHandle(NULL),
        NULL
    ));

    RETURN_HR_IF(HRESULT_FROM_WIN32(GetLastError()), (m_PreviewHwnd.get() == NULL));
    ShowWindow(m_PreviewHwnd.get(), SW_SHOWNORMAL);

    return S_OK;
}

HRESULT EVRHelper::_CreateEVRSink(IMFMediaType* pMediaType)
{
    // render with EVR? https://stackoverflow.com/questions/32739558/media-foundation-evr-no-video-displaying
    // EVR uses own allocator, https://docs.microsoft.com/en-us/windows/win32/api/mfidl/nf-mfidl-mfcreatesamplecopiermft

    wil::com_ptr_nothrow<IMFActivate> spActivate;
    RETURN_IF_FAILED(MFCreateVideoRendererActivate(m_PreviewHwnd.get(), &spActivate));

    wil::com_ptr<IMFMediaSink> spEVRSink;
    RETURN_IF_FAILED(spActivate->ActivateObject(IID_PPV_ARGS(&spEVRSink)));

    // Configure the mediatype for the SVR.
    wil::com_ptr_nothrow<IMFStreamSink> spStreamSink;
    RETURN_IF_FAILED(spEVRSink->GetStreamSinkByIndex(0, &spStreamSink));

    wil::com_ptr_nothrow<IMFMediaTypeHandler> spTypeHandler;
    RETURN_IF_FAILED(spStreamSink->GetMediaTypeHandler(&spTypeHandler));
    RETURN_IF_FAILED(spTypeHandler->SetCurrentMediaType(pMediaType));

    //get IMFVideoSampleAllocator service
    RETURN_IF_FAILED(MFGetService(spStreamSink.get(), MR_VIDEO_ACCELERATION_SERVICE, IID_PPV_ARGS(&m_spSampleAllocator)));
    wil::com_ptr_nothrow<IMFDXGIDeviceManager> spD3DManager;
    //RETURN_IF_FAILED(MFGetService(spEVRSink.get(), MR_VIDEO_ACCELERATION_SERVICE, IID_PPV_ARGS(&spD3DManager)), "Failed to get Direct3D manager from EVR media sink.");
    //RETURN_IF_FAILED(m_spSampleAllocator->SetDirectXManager(spD3DManager.get()), "Failed to set D3DManager on video sample allocator.");
    RETURN_IF_FAILED(m_spSampleAllocator->InitializeSampleAllocator(5, pMediaType));
    m_spOutputMediaType = pMediaType;

    //Create the sink writer
    LOG_COMMENT(L"Create EVRWriter");
    RETURN_IF_FAILED(MFCreateSinkWriterFromMediaSink(spEVRSink.get(), NULL, &m_spEVRSinkWriter));
    RETURN_IF_FAILED(m_spEVRSinkWriter->SetInputMediaType(0, pMediaType, NULL));

    OutputDebugString(L"\n[CustomSink] Begin EVRWriter");
    RETURN_IF_FAILED(m_spEVRSinkWriter->BeginWriting());

    m_llBaseTimestamp = MFGetSystemTime();
    RETURN_IF_FAILED(m_spEVRSinkWriter->SendStreamTick(0, m_llBaseTimestamp));
    LOG_COMMENT(L"stream tick: %I64d hns", m_llBaseTimestamp);

    return S_OK;
}

HRESULT EVRHelper::_OffsetTimestamp(IMFSample* pSample)
{
    LONGLONG samplets = 0;
    RETURN_IF_FAILED(pSample->GetSampleTime(&samplets));

    LONGLONG ts = samplets - m_llBaseTimestamp;
    if (ts < 0)
    {
        return E_INVALIDARG;
    }
    (pSample->SetSampleTime(ts));

    return S_OK;
}

