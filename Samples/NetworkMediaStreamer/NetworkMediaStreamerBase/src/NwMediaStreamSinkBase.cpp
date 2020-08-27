// Copyright (C) Microsoft Corporation. All rights reserved.
#include <pch.h>
#include "NwMediaStreamSinkBase.h"

using namespace winrt;

NwMediaStreamSinkBase::NwMediaStreamSinkBase(IMFMediaType* pMediaType, IMFMediaSink* pParent, DWORD dwStreamID)
    : m_cRef(1)
    , m_pVideoHeader(nullptr)
    , m_VideoHeaderSize(0)
    , m_bIsShutdown(false)
    , m_pParentSink(pParent) // to avoid circular reference never Add-Ref to pParent unless giving ownership to another class
    , m_dwStreamID(dwStreamID)
{
    winrt::check_hresult(MFCreateEventQueue(m_spEventQueue.put()));
    winrt::check_hresult(MFCreateSimpleTypeHandler(m_spMTHandler.put()));
    winrt::check_hresult(m_spMTHandler->SetCurrentMediaType(pMediaType));
}

NwMediaStreamSinkBase ::~NwMediaStreamSinkBase()
{
    if (m_pVideoHeader != nullptr)
    {
        m_VideoHeaderSize = 0;
        CoTaskMemFree(m_pVideoHeader);
    }
}

STDMETHODIMP NwMediaStreamSinkBase::QueryInterface(REFIID riid, void** ppv)
{
    static const QITAB qit[] =
    {
        QITABENT(NwMediaStreamSinkBase , IMFStreamSink),
        QITABENT(NwMediaStreamSinkBase , IMFMediaEventGenerator),
        QITABENT(NwMediaStreamSinkBase , INetworkMediaStreamSink),
        QITABENT(NwMediaStreamSinkBase , IUnknown),
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

STDMETHODIMP NwMediaStreamSinkBase::BeginGetEvent(IMFAsyncCallback* pCallback, IUnknown* pState)
{
    RETURNIFSHUTDOWN;
    return m_spEventQueue->BeginGetEvent(pCallback, pState);
}

STDMETHODIMP NwMediaStreamSinkBase::EndGetEvent(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent)
{
    RETURNIFSHUTDOWN;
    return m_spEventQueue->EndGetEvent(pResult, ppEvent);
}

STDMETHODIMP NwMediaStreamSinkBase::GetEvent(DWORD dwFlags, IMFMediaEvent** ppEvent)
{
    RETURNIFSHUTDOWN;
    return m_spEventQueue->GetEvent(dwFlags, ppEvent);
}

STDMETHODIMP NwMediaStreamSinkBase::QueueEvent(MediaEventType met, REFGUID extendedType, HRESULT hrStatus, const PROPVARIANT* pvValue)
{
    RETURNIFSHUTDOWN;
    return m_spEventQueue->QueueEventParamVar(met, extendedType, hrStatus, pvValue);
}

STDMETHODIMP NwMediaStreamSinkBase::GetMediaSink(IMFMediaSink** ppMediaSink)
{
    RETURNIFSHUTDOWN;

    if (!ppMediaSink)
    {
        return E_POINTER;
    }
    if (m_pParentSink)
    {
        m_pParentSink->AddRef();
        *ppMediaSink = m_pParentSink;
        return S_OK;
    }
    else
    {
        return MF_E_STREAMSINK_REMOVED;
    }
}

STDMETHODIMP NwMediaStreamSinkBase::GetIdentifier(DWORD* pdwIdentifier)
{
    RETURNIFSHUTDOWN;
    if (!pdwIdentifier)
    {
        return E_POINTER;
    }

    *pdwIdentifier = m_dwStreamID;
    return S_OK;
}

STDMETHODIMP NwMediaStreamSinkBase::GetMediaTypeHandler(IMFMediaTypeHandler** ppHandler)
{
    RETURNIFSHUTDOWN;
    if (!ppHandler) return E_POINTER;
    m_spMTHandler.copy_to(ppHandler);
    return S_OK;
}

STDMETHODIMP NwMediaStreamSinkBase::ProcessSample(IMFSample* pSample)
{
    RETURNIFSHUTDOWN;
    auto hr = PacketizeAndSend(pSample);
    if (SUCCEEDED(hr))
    {
        hr = QueueEvent(MEStreamSinkRequestSample, GUID_NULL, S_OK, nullptr);
    }

    return hr;
}

STDMETHODIMP NwMediaStreamSinkBase::PlaceMarker(MFSTREAMSINK_MARKER_TYPE eMarkerType, const PROPVARIANT* pvarMarkerValue, const PROPVARIANT* pvarContextValue)
{
    RETURNIFSHUTDOWN;
    return QueueEvent(MEStreamSinkMarker, GUID_NULL, S_OK, pvarContextValue);
}

STDMETHODIMP NwMediaStreamSinkBase::Flush(void)
{
    RETURNIFSHUTDOWN;
    return S_OK;
}
