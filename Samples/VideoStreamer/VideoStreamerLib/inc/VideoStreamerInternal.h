// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#define SECURITY_WIN32

#include <WinSock2.h>
#include <windows.h>
#include<Unknwn.h>
#include <iostream>
#include <mutex>

#include <Security.h>
#include <winternl.h>
#define SCHANNEL_USE_BLACKLISTS
#include <schnlsp.h>

#include <ws2def.h>
#include <mstcpip.h>
#include <ws2tcpip.h>

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <Shlwapi.h>
#include <windows.media.h>
#include "VideoStreamer.h"

#define RETURNIFSHUTDOWN if(m_bIsShutdown) return MF_E_SHUTDOWN;

class NwMediaStreamSinkBase : public INetworkMediaStreamSink
{
protected:
    long m_cRef;
    uint32_t m_height;
    float m_frameRate;
    uint32_t m_width;
    uint32_t m_bitrate;
    GUID m_outVideoFormat;
    uint8_t* m_pVideoHeader;
    uint32_t m_VideoHeaderSize;
    bool m_bIsShutdown;
    IMFMediaSink *m_pParentSink;
    winrt::com_ptr<IMFMediaEventQueue> m_spEventQueue;
    winrt::com_ptr<IMFMediaTypeHandler> m_spMTHandler;

    DWORD m_dwStreamID;
    NwMediaStreamSinkBase(IMFMediaType* pMediaType, IMFMediaSink* pParent) :
        m_cRef(1),
        m_width(0),
        m_bitrate(0),
        m_height(0),
        m_frameRate(0),
        m_pVideoHeader(nullptr),
        m_VideoHeaderSize(0),
        m_outVideoFormat(GUID_NULL),
        m_dwStreamID(0),
        m_bIsShutdown(false),
        m_pParentSink(pParent)
    {
        winrt::check_hresult(MFCreateEventQueue(m_spEventQueue.put()));
        winrt::check_hresult(MFCreateSimpleTypeHandler(m_spMTHandler.put()));
        winrt::check_hresult(m_spMTHandler->SetCurrentMediaType(pMediaType));
    }

    virtual ~NwMediaStreamSinkBase()
    {
        if (m_pVideoHeader != nullptr)
        {
            m_VideoHeaderSize = 0;
            CoTaskMemFree(m_pVideoHeader);
        }
    }

    virtual STDMETHODIMP PacketizeAndSend(IMFSample* pSample) = 0;

public:
    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID iid, void** ppv) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IMFClockStateSink methods
    STDMETHODIMP Start(MFTIME hnsSystemTime, LONGLONG llClockStartOffset);
    STDMETHODIMP Stop(MFTIME hnsSystemTime);
    STDMETHODIMP Pause(MFTIME hnsSystemTime);
    STDMETHODIMP Shutdown();

    /************IMFSinkStream methods*********/
     // IMFMediaEventGenerator::BeginGetEvent
    STDMETHODIMP BeginGetEvent(IMFAsyncCallback* pCallback, IUnknown* pState)
    {
        RETURNIFSHUTDOWN;
        return m_spEventQueue->BeginGetEvent(pCallback, pState);
    }

    // IMFMediaEventGenerator::EndGetEvent
    STDMETHODIMP EndGetEvent(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent)
    {
        RETURNIFSHUTDOWN;
        return m_spEventQueue->EndGetEvent(pResult, ppEvent);
    }

    // IMFMediaEventGenerator::GetEvent
    STDMETHODIMP GetEvent(DWORD dwFlags, IMFMediaEvent** ppEvent)
    {
        RETURNIFSHUTDOWN;
        return m_spEventQueue->GetEvent(dwFlags, ppEvent);
    }

    // IMFMediaEventGenerator::QueueEvent
    STDMETHODIMP QueueEvent(
        MediaEventType met, REFGUID extendedType,
        HRESULT hrStatus, const PROPVARIANT* pvValue)
    {
        RETURNIFSHUTDOWN;
        return m_spEventQueue->QueueEventParamVar(met, extendedType, hrStatus, pvValue);
    }

    STDMETHODIMP GetMediaSink(
        /* [out] */ __RPC__deref_out_opt IMFMediaSink** ppMediaSink)
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

    STDMETHODIMP GetIdentifier(
        /* [out] */ __RPC__out DWORD* pdwIdentifier)
    {
        RETURNIFSHUTDOWN;
        if (!pdwIdentifier)
        {
            return E_POINTER;
        }

        *pdwIdentifier = m_dwStreamID;
        return S_OK;
    }

    STDMETHODIMP GetMediaTypeHandler(
        /* [out] */ __RPC__deref_out_opt IMFMediaTypeHandler** ppHandler)
    {
        RETURNIFSHUTDOWN;
        if (!ppHandler) return E_POINTER;
        m_spMTHandler.copy_to(ppHandler);
        return S_OK;
    }

    STDMETHODIMP ProcessSample(
        /* [in] */ __RPC__in_opt IMFSample* pSample)
    {
        RETURNIFSHUTDOWN;

        auto hr = PacketizeAndSend(pSample);
        if (SUCCEEDED(hr))
        {
            hr = QueueEvent(MEStreamSinkRequestSample, GUID_NULL, S_OK, nullptr);
        }

        return hr;
    }
    STDMETHODIMP PlaceMarker(
        /* [in] */ MFSTREAMSINK_MARKER_TYPE eMarkerType,
        /* [in] */ __RPC__in const PROPVARIANT* pvarMarkerValue,
        /* [in] */ __RPC__in const PROPVARIANT* pvarContextValue)
    {
        RETURNIFSHUTDOWN;
        return QueueEvent(MEStreamSinkMarker, GUID_NULL, S_OK, pvarContextValue);
    }

    STDMETHODIMP Flush(void)
    {
        RETURNIFSHUTDOWN;
        return S_OK;
    }
    /**********************/


};

