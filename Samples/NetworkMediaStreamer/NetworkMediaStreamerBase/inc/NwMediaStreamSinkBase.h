// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for more information

#pragma once

#define RETURN_IF_SHUTDOWN if(m_bIsShutdown) return MF_E_SHUTDOWN;
#define RETURN_IF_NULL(p) if(!p) return E_POINTER;
#define HRESULT_EXCEPTION_BOUNDARY_FUNC catch(...) { auto hr = winrt::to_hresult(); return hr;}

class NwMediaStreamSinkBase : public winrt::implements<NwMediaStreamSinkBase, INetworkMediaStreamSink, IMFStreamSink, IMFMediaEventGenerator>
{
protected:
    uint8_t* m_pVideoHeader;
    uint32_t m_VideoHeaderSize;
    bool m_bIsShutdown;
    IMFMediaSink* m_pParentSink;
    winrt::com_ptr<IMFMediaEventQueue> m_spEventQueue;
    winrt::com_ptr<IMFMediaTypeHandler> m_spMTHandler;
    DWORD m_dwStreamID;
    NwMediaStreamSinkBase(IMFMediaType* pMediaType, IMFMediaSink* pParent, DWORD dwStreamID);

    virtual ~NwMediaStreamSinkBase();

    virtual STDMETHODIMP PacketizeAndSend(IMFSample* pSample) = 0;

public:

    // INetworkStreamSink
    STDMETHODIMP Start(MFTIME hnsSystemTime, LONGLONG llClockStartOffset);
    STDMETHODIMP Stop(MFTIME hnsSystemTime);
    STDMETHODIMP Pause(MFTIME hnsSystemTime);
    STDMETHODIMP Shutdown();

    // IMFMediaEventGenerator
    STDMETHODIMP BeginGetEvent(IMFAsyncCallback* pCallback, IUnknown* pState);

    STDMETHODIMP EndGetEvent(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent);

    STDMETHODIMP GetEvent(DWORD dwFlags, IMFMediaEvent** ppEvent);

    STDMETHODIMP QueueEvent(
        MediaEventType met, REFGUID extendedType,
        HRESULT hrStatus, const PROPVARIANT* pvValue);

    // IMFStreamSink
    STDMETHODIMP GetMediaSink(IMFMediaSink** ppMediaSink);

    STDMETHODIMP GetIdentifier(DWORD* pdwIdentifier);

    STDMETHODIMP GetMediaTypeHandler(IMFMediaTypeHandler** ppHandler);

    STDMETHODIMP ProcessSample(IMFSample* pSample);

    STDMETHODIMP PlaceMarker(
        MFSTREAMSINK_MARKER_TYPE eMarkerType,
        const PROPVARIANT* pvarMarkerValue,
        const PROPVARIANT* pvarContextValue);

    STDMETHODIMP Flush(void);

};

