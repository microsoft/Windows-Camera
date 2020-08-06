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


