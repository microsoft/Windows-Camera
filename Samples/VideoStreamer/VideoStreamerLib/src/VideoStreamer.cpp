// Copyright (C) Microsoft Corporation. All rights reserved.
#include "VideoStreamerInternal.h"

using namespace winrt;
//#define TIGHT_LATENCY_CONTROL
#define VERBOSITY  0

#ifdef TIGHT_LATENCY_CONTROL
#define MAX_SINK_LATENCY 100
uint32_t g_dropCount;
#endif

#if (VERBOSITY > 0)
#define INFOLOG1 printf 
#else 
#define INFOLOG1 
#endif
#if (VERBOSITY > 1)
#define INFOLOG2 printf 
#else
#define INFOLOG2
#endif
#if 0
void NwMediaStreamSinkBase::WritePacket(IMFSample* pSample)
{
#ifdef TIGHT_LATENCY_CONTROL
    if (g_dropCount)
    {
        g_dropCount--;
    }
    else
#endif
    {
        try
        {
            if (pSample)
            {
                winrt::check_hresult(m_spSinkWriter->WriteSample(0, pSample));
            }
        }
        catch (hresult_error const& ex)
        {
            std::wcout << L"Error:" << std::hex << ex.code() << L":" << ex.message().c_str();
        }
    }
}
#endif
STDMETHODIMP NwMediaStreamSinkBase::QueryInterface(REFIID riid, void** ppv)
{
    static const QITAB qit[] =
    {
        QITABENT(NwMediaStreamSinkBase, IMFClockStateSink),
        QITABENT(NwMediaStreamSinkBase, IMFStreamSink),
        QITABENT(NwMediaStreamSinkBase, IMFMediaEventGenerator),
        QITABENT(NwMediaStreamSinkBase, INetworkMediaStreamSink),
        { 0 }
    };
    return QISearch(this, qit, riid, ppv);
}

STDMETHODIMP_(ULONG) NwMediaStreamSinkBase::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

STDMETHODIMP_(ULONG) NwMediaStreamSinkBase::Release(void)
{
    ULONG cRef = InterlockedDecrement(&m_cRef);
    if (cRef == 0)
    {
        delete this;
    }
    return cRef;

}

// IMFClockStateSink methods.

// In these example, the IMFClockStateSink methods do not perform any actions. 
// You can use these methods to track the state of the sample grabber sink.

STDMETHODIMP NwMediaStreamSinkBase::Start(MFTIME hnsSystemTime, LONGLONG llClockStartOffset)
{
    RETURNIFSHUTDOWN;
    auto hr = QueueEvent(MEStreamSinkStarted, GUID_NULL, S_OK, nullptr);
    if (m_spMTHandler)
    {
        winrt::com_ptr<IMFMediaType> spMT;
        hr = m_spMTHandler->GetCurrentMediaType(spMT.put());
        (void)spMT->GetAllocatedBlob(MF_MT_MPEG_SEQUENCE_HEADER, &m_pVideoHeader, &m_VideoHeaderSize);
    }
    if (SUCCEEDED(hr))
    {
        hr = QueueEvent(MEStreamSinkRequestSample, GUID_NULL, S_OK, nullptr);
    }
    return hr;
}

STDMETHODIMP NwMediaStreamSinkBase::Stop(MFTIME hnsSystemTime)
{
    RETURNIFSHUTDOWN;
    return QueueEvent(MEStreamSinkStopped, GUID_NULL, S_OK, nullptr);
}

STDMETHODIMP NwMediaStreamSinkBase::Pause(MFTIME hnsSystemTime)
{
    RETURNIFSHUTDOWN;
    return QueueEvent(MEStreamSinkPaused, GUID_NULL, S_OK, nullptr);;
}

STDMETHODIMP NwMediaStreamSinkBase::Shutdown()
{
    RETURNIFSHUTDOWN;
    m_bIsShutdown = true;
    return S_OK;
}

#if 0
void NwMediaStreamSinkBase::ConfigEncoder(uint32_t width, uint32_t height, float framerate, GUID inVideoFormat, GUID outVideoFormat, uint32_t bitrate)
{
    winrt::com_ptr<IMFMediaType> spOutType, spInType;

    /*winrt::com_ptr<IMFActivate>  spSinkActivate;*/

    m_width = width;
    m_height = height;
    m_frameRate = framerate;
    m_bitrate = bitrate;
    m_outVideoFormat = outVideoFormat;


    check_hresult(MFCreateMediaType(spInType.put()));
    //check_hresult(MFCreateMediaType(spOutType.put()));
    //check_hresult(spOutType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
    //check_hresult(spOutType->SetGUID(MF_MT_SUBTYPE, outVideoFormat));
    //check_hresult(spOutType->SetUINT32(MF_MT_AVG_BITRATE, bitrate));

   // check_hresult(spOutType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));
    check_hresult(MFSetAttributeSize(spInType.get(), MF_MT_FRAME_SIZE, width, height));
    check_hresult(MFSetAttributeRatio(spInType.get(), MF_MT_FRAME_RATE, (int)(framerate * 100), 100));

    //check_hresult(spOutType->CopyAllItems(spInType.get()));
    check_hresult(spInType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
    check_hresult(spInType->SetGUID(MF_MT_SUBTYPE, inVideoFormat));
    //check_hresult(spInType->DeleteItem(MF_MT_AVG_BITRATE));
    LONG stride = 0;
    check_hresult(MFGetStrideForBitmapInfoHeader(MFVideoFormat_NV12.Data1, width, &stride));
    check_hresult(spInType->SetUINT32(MF_MT_DEFAULT_STRIDE, UINT32(stride)));

    check_hresult(MFSetAttributeRatio(spInType.get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1));

    //m_spMTHandler->SetCurrentMediaType(spOutType.get());

#if 0
    // Create the sample grabber sink.
    check_hresult(MFCreateSampleGrabberSinkActivate(spOutType.get(), SinkGrabberCBWrapper::CreateWrapper(this), spSinkActivate.put()));

    // To run as fast as possible, set this attribute
    check_hresult(spSinkActivate->SetUINT32(MF_SAMPLEGRABBERSINK_IGNORE_CLOCK, TRUE));
    check_hresult(spSinkActivate->ActivateObject(__uuidof(IMFMediaSink), m_spSampleGrabberSink.put_void()));
    check_hresult(m_spSampleGrabberSink->GetStreamSinkByIndex(0, m_spStreamSink.put()));
#endif
#if 0
    com_ptr<IMFAttributes> spSWAttributes;
    check_hresult(MFCreateAttributes(spSWAttributes.put(), 2));
    check_hresult(spSWAttributes->SetUINT32(MF_LOW_LATENCY, TRUE));
    HRESULT hr = S_OK;
    BOOL bEnableHWTransforms = FALSE;
    do
    {
        m_spSinkWriter = nullptr;
        check_hresult(spSWAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, bEnableHWTransforms));
        m_spParentSink.attach(new RTPMediaSink(this));
        check_hresult(MFCreateSinkWriterFromMediaSink(m_spParentSink.get(), spSWAttributes.get(), m_spSinkWriter.put()));
        hr = m_spSinkWriter->SetInputMediaType(0, spInType.get(), nullptr);
        bEnableHWTransforms = !bEnableHWTransforms;
    } while (hr == MF_E_TOPO_CODEC_NOT_FOUND);
    check_hresult(hr);
    check_hresult(m_spSinkWriter->BeginWriting());

    int i = 0;
    GUID cagtegory;
    winrt::com_ptr<IMFTransform> spTranform;
    while (SUCCEEDED(m_spSinkWriter.as<IMFSinkWriterEx>()->GetTransformForStream(0, i++, &cagtegory, spTranform.put())))
    {
        INFOLOG1("\nTranform %d: %x-%x-%x-%x%x%x%x", i - 1, cagtegory.Data1, cagtegory.Data2, cagtegory.Data3, cagtegory.Data4[0], cagtegory.Data4[1], cagtegory.Data4[2], cagtegory.Data4[3]);
        if (IsEqualGUID(cagtegory, MFT_CATEGORY_VIDEO_ENCODER))
        {
            winrt::com_ptr<IMFMediaType> spOType;
            DWORD inStrmId = 0, outStrmId = 0;
            // ignore error, if not implemented the strm ids are 0
            (void)spTranform->GetStreamIDs(1, &inStrmId, 1, &outStrmId);
            check_hresult(spTranform->GetOutputCurrentType(outStrmId, spOType.put()));
            m_pVideoHeader = nullptr;
            m_VideoHeaderSize = 0;
            (void)spOType->GetAllocatedBlob(MF_MT_MPEG_SEQUENCE_HEADER, &m_pVideoHeader, &m_VideoHeaderSize);
        }
        spTranform = nullptr;
    }
    INFOLOG1("\nTotal: %d", i);
#endif
}

VIDEOSTREAMER_API IMFMediaSink* CreateRTPSink(INetworkMediaStreamSink* pVideoStreamer)
{
    return new RTPMediaSink(pVideoStreamer);
}
#endif

