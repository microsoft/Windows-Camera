// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for more information

#include <pch.h>

using namespace winrt;

NwMediaStreamSinkBase::NwMediaStreamSinkBase(IMFMediaType* pMediaType, IMFMediaSink* pParent, DWORD dwStreamID)
    : m_pVideoHeader(nullptr)
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


// IMFClockStateSink methods.

STDMETHODIMP NwMediaStreamSinkBase::Start(MFTIME hnsSystemTime, LONGLONG llClockStartOffset)
{
    RETURN_IF_SHUTDOWN;
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
    RETURN_IF_SHUTDOWN;
    return QueueEvent(MEStreamSinkStopped, GUID_NULL, S_OK, nullptr);
}

STDMETHODIMP NwMediaStreamSinkBase::Pause(MFTIME hnsSystemTime)
{
    RETURN_IF_SHUTDOWN;
    return QueueEvent(MEStreamSinkPaused, GUID_NULL, S_OK, nullptr);;
}

STDMETHODIMP NwMediaStreamSinkBase::Shutdown()
{
    RETURN_IF_SHUTDOWN;
    m_bIsShutdown = true;
    return S_OK;
}

STDMETHODIMP NwMediaStreamSinkBase::BeginGetEvent(IMFAsyncCallback* pCallback, IUnknown* pState)
{
    RETURN_IF_SHUTDOWN;
    return m_spEventQueue->BeginGetEvent(pCallback, pState);
}

STDMETHODIMP NwMediaStreamSinkBase::EndGetEvent(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent)
{
    RETURN_IF_SHUTDOWN;
    return m_spEventQueue->EndGetEvent(pResult, ppEvent);
}

STDMETHODIMP NwMediaStreamSinkBase::GetEvent(DWORD dwFlags, IMFMediaEvent** ppEvent)
{
    RETURN_IF_SHUTDOWN;
    return m_spEventQueue->GetEvent(dwFlags, ppEvent);
}

STDMETHODIMP NwMediaStreamSinkBase::QueueEvent(MediaEventType met, REFGUID extendedType, HRESULT hrStatus, const PROPVARIANT* pvValue)
{
    RETURN_IF_SHUTDOWN;
    return m_spEventQueue->QueueEventParamVar(met, extendedType, hrStatus, pvValue);
}

STDMETHODIMP NwMediaStreamSinkBase::GetMediaSink(IMFMediaSink** ppMediaSink)
{
    RETURN_IF_SHUTDOWN;
    RETURN_IF_NULL(ppMediaSink);

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
    RETURN_IF_SHUTDOWN;
    RETURN_IF_NULL(pdwIdentifier);

    *pdwIdentifier = m_dwStreamID;
    return S_OK;
}

STDMETHODIMP NwMediaStreamSinkBase::GetMediaTypeHandler(IMFMediaTypeHandler** ppHandler)
{
    RETURN_IF_SHUTDOWN;
    RETURN_IF_NULL(ppHandler);

    m_spMTHandler.copy_to(ppHandler);
    return S_OK;
}

STDMETHODIMP NwMediaStreamSinkBase::ProcessSample(IMFSample* pSample)
{
    RETURN_IF_SHUTDOWN;
    auto hr = PacketizeAndSend(pSample);
    if (SUCCEEDED(hr))
    {
        hr = QueueEvent(MEStreamSinkRequestSample, GUID_NULL, S_OK, nullptr);
    }

    return hr;
}

STDMETHODIMP NwMediaStreamSinkBase::PlaceMarker(MFSTREAMSINK_MARKER_TYPE eMarkerType, const PROPVARIANT* pvarMarkerValue, const PROPVARIANT* pvarContextValue)
{
    RETURN_IF_SHUTDOWN;
    return QueueEvent(MEStreamSinkMarker, GUID_NULL, S_OK, pvarContextValue);
}

STDMETHODIMP NwMediaStreamSinkBase::Flush(void)
{
    RETURN_IF_SHUTDOWN;
    return S_OK;
}
