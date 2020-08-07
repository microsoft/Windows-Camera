// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once
#include <ws2tcpip.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <Shlwapi.h>
#include <windows.media.h>
#include "NetworkMediaStreamer.h"

#define RETURNIFSHUTDOWN if(m_bIsShutdown) return MF_E_SHUTDOWN;

class NwMediaStreamSinkBase  : public INetworkMediaStreamSink
{
protected:
    long m_cRef;;
    uint8_t* m_pVideoHeader;
    uint32_t m_VideoHeaderSize;
    bool m_bIsShutdown;
    IMFMediaSink *m_pParentSink;
    winrt::com_ptr<IMFMediaEventQueue> m_spEventQueue;
    winrt::com_ptr<IMFMediaTypeHandler> m_spMTHandler;
    DWORD m_dwStreamID;
    NwMediaStreamSinkBase(IMFMediaType* pMediaType, IMFMediaSink* pParent, DWORD dwStreamID);

    virtual ~NwMediaStreamSinkBase ();

    virtual STDMETHODIMP PacketizeAndSend(IMFSample* pSample) = 0;

public:
    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID iid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

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
    STDMETHODIMP GetMediaSink(
        /* [out] */ __RPC__deref_out_opt IMFMediaSink** ppMediaSink);

    STDMETHODIMP GetIdentifier(
        /* [out] */ __RPC__out DWORD* pdwIdentifier);

    STDMETHODIMP GetMediaTypeHandler(
        /* [out] */ __RPC__deref_out_opt IMFMediaTypeHandler** ppHandler);

    STDMETHODIMP ProcessSample(
        /* [in] */ __RPC__in_opt IMFSample* pSample);

    STDMETHODIMP PlaceMarker(
        /* [in] */ MFSTREAMSINK_MARKER_TYPE eMarkerType,
        /* [in] */ __RPC__in const PROPVARIANT* pvarMarkerValue,
        /* [in] */ __RPC__in const PROPVARIANT* pvarContextValue);

    STDMETHODIMP Flush(void);

};

