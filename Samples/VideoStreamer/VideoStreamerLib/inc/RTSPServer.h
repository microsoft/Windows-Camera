// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once
#define DBGLEVEL 1
#include "RtspSession.h"
#include <inspectable.h>

class RTSPServer : public IRTSPServerControl
{
public:
    RTSPServer(streamerMapType streamers, uint16_t socketPort, std::vector<PCCERT_CONTEXT> serverCerts)
        : m_acceptEvent(nullptr)
        , m_callbackHandle(nullptr)
        , m_socketPort(socketPort)
        , m_streamers(streamers)
        , m_bSecure(!serverCerts.empty())
        , m_serverCerts(serverCerts)
        , m_cRef(0)
        , MasterSocket(INVALID_SOCKET)
        , m_bIsShutdown(false)
    {
    }
    virtual  ~RTSPServer()
    {
        StopServer();
    }

    // IRTSPServerControl
    winrt::event_token AddLogHandler(LoggerType type, winrt::delegate < winrt::hresult, winrt::hstring> handler)
    {
        return m_LoggerEvents[(int)type].add(handler);
    }
    void RemoveLogHandler(LoggerType type, winrt::event_token token)
    {
        m_LoggerEvents[(int)type].remove(token);
    }

    winrt::event_token AddSessionStatusHandler(LoggerType type, winrt::delegate <uint64_t,SessionStatus> handler)
    {
        return m_SessionStatusEvents.add(handler);
    }
    void RemoveSessionStatusHandler(LoggerType type, winrt::event_token token)
    {
        m_SessionStatusEvents.remove(token);
    }
    void StartServer();
    void StopServer();

    // IUnknown Methods
    STDMETHOD_(HRESULT __stdcall, QueryInterface)(REFIID riid, void** ppv);
    STDMETHOD_(ULONG __stdcall, AddRef)();
    STDMETHOD_(ULONG __stdcall, Release)();

private:

    streamerMapType m_streamers;
    std::map <RTSPSession*, SOCKET> m_Sessions;
    SOCKET      MasterSocket;                                 // our masterSocket(socket that listens for RTSP client connections)  
    WSAEVENT m_acceptEvent;
    HANDLE m_callbackHandle;
    uint16_t m_socketPort;
    bool m_bSecure;
    std::vector<PCCERT_CONTEXT> m_serverCerts;
    winrt::event<winrt::delegate<winrt::hresult, winrt::hstring>> m_LoggerEvents[(size_t)LoggerType::LOGGER_MAX];
    winrt::event<winrt::delegate<uint64_t, SessionStatus>> m_SessionStatusEvents;
    long m_cRef;
    winrt::com_ptr<IMFPresentationClock> m_spClock;
    bool m_bIsShutdown;
public:
#if 0
    STDMETHODIMP GetCharacteristics(
        /* [out] */ __RPC__out DWORD* pdwCharacteristics)
    {
        RETURNIFSHUTDOWN;
        if (!pdwCharacteristics)
        {
            return E_POINTER;
        }
        *pdwCharacteristics = MEDIASINK_FIXED_STREAMS | MEDIASINK_RATELESS;
        return S_OK;
    }

    STDMETHODIMP AddStreamSink(
        /* [in] */ DWORD dwStreamSinkIdentifier,
        /* [in] */ __RPC__in_opt IMFMediaType* pMediaType,
        /* [out] */ __RPC__deref_out_opt IMFStreamSink** ppStreamSink)
    {
        RETURNIFSHUTDOWN;
        return MF_E_STREAMSINKS_FIXED;
    }

    STDMETHODIMP RemoveStreamSink(
        /* [in] */ DWORD dwStreamSinkIdentifier)
    {
        RETURNIFSHUTDOWN;
        return MF_E_STREAMSINKS_FIXED;
    }

    STDMETHODIMP GetStreamSinkCount(
        /* [out] */ __RPC__out DWORD* pcStreamSinkCount)
    {
        RETURNIFSHUTDOWN;
        if (!pcStreamSinkCount)
        {
            return E_POINTER;
        }
        else
        {
            *pcStreamSinkCount = 1;
            return S_OK;
        }
    }

    STDMETHODIMP GetStreamSinkByIndex(
        /* [in] */ DWORD dwIndex,
        /* [out] */ __RPC__deref_out_opt IMFStreamSink** ppStreamSink)
    {
        RETURNIFSHUTDOWN;
        if (!ppStreamSink) return E_POINTER;
        if (dwIndex > 0)
        {
            return MF_E_INVALIDINDEX;
        }
        auto iter = m_streamers.begin();
        for (DWORD i = 0; i < dwIndex; i++) iter++;
        iter->second.as<IMFStreamSink>().copy_to(ppStreamSink);
        //m_spStreamSink.as<IMFStreamSink>().copy_to(ppStreamSink);
        return S_OK;
    }

    STDMETHODIMP GetStreamSinkById(
        /* [in] */ DWORD dwStreamSinkIdentifier,
        /* [out] */ __RPC__deref_out_opt IMFStreamSink** ppStreamSink)
    {
        RETURNIFSHUTDOWN;
        if (dwStreamSinkIdentifier > m_streamers.size())
        {
            return MF_E_INVALIDSTREAMNUMBER;
        }
        else
        {
            auto iter = m_streamers.begin();
            for (DWORD i = 0; i < dwStreamSinkIdentifier; i++) iter++;
            iter->second.as<IMFStreamSink>().copy_to(ppStreamSink);
            return S_OK;
        }
    }

    STDMETHODIMP SetPresentationClock(
        /* [in] */ __RPC__in_opt IMFPresentationClock* pPresentationClock)
    {
        RETURNIFSHUTDOWN;
        if (m_spClock)
        {
            m_spClock->RemoveClockStateSink(this);
            m_spClock = nullptr;
        }
        m_spClock.copy_from(pPresentationClock);
        return m_spClock->AddClockStateSink(this);
        //return //m_spStreamSink->SetPresentationClock(pPresentationClock);
    }

    STDMETHODIMP GetPresentationClock(
        /* [out] */ __RPC__deref_out_opt IMFPresentationClock** ppPresentationClock)
    {
        RETURNIFSHUTDOWN;
        if (!ppPresentationClock) return E_POINTER;
        m_spClock.copy_to(ppPresentationClock);
        return S_OK;
    }

    STDMETHODIMP Shutdown(void)
    {
        RETURNIFSHUTDOWN;
        m_bIsShutdown = true;
        for (auto s : m_streamers)
        {
            s.second->Shutdown();
        }
        return S_OK;
    }

    //IMediaExtension
    STDMETHODIMP SetProperties(
        ABI::Windows::Foundation::Collections::IPropertySet* configuration
    )
    {
        return S_OK;
    }

    // IInspectable

    STDMETHODIMP GetIids(
        /* [out] */ __RPC__out ULONG* iidCount,
        /* [size_is][size_is][out] */ __RPC__deref_out_ecount_full_opt(*iidCount) IID** iids)
    {
        if ((!iidCount) || (!iids))
        {
            return E_POINTER;
        }
        *iidCount = 4;
        *iids = (IID*)CoTaskMemAlloc(sizeof(IID) * 4);

        if (!(*iids)) return E_OUTOFMEMORY;

        *iids[0] = __uuidof(IMFMediaSink);
        *iids[1] = __uuidof(IMFClockStateSink);
        *iids[2] = __uuidof(ABI::Windows::Media::IMediaExtension);
        *iids[3] = __uuidof(IRTSPServerControl);

        return S_OK;
    }

    STDMETHODIMP GetRuntimeClassName(
        /* [out] */ __RPC__deref_out_opt HSTRING* className)
    {
        auto name = winrt::hstring(L"VideoStreamer.RTPSink");
        return WindowsCreateString(name.c_str(), name.size(), className);
    }

    STDMETHODIMP GetTrustLevel(
        /* [out] */ __RPC__out TrustLevel* trustLevel)
    {
        if (!trustLevel)
        {
            return E_POINTER;
        }
        *trustLevel = TrustLevel::FullTrust;
        return S_OK;
    }
    /****************************************/

    // IMFClockStateSink methods
    STDMETHODIMP OnClockStart(MFTIME hnsSystemTime, LONGLONG llClockStartOffset)
    {
        for (auto s : m_streamers)
        {
            s.second->Start(hnsSystemTime, llClockStartOffset);
        }
        return S_OK;
    }
    STDMETHODIMP OnClockStop(MFTIME hnsSystemTime)
    {
        for (auto s : m_streamers)
        {
            s.second->Stop(hnsSystemTime);
        }
        return S_OK;
    }
    STDMETHODIMP OnClockPause(MFTIME hnsSystemTime)
    {
        for (auto s : m_streamers)
        {
            s.second->Pause(hnsSystemTime);
        }
        return S_OK;
    }
    STDMETHODIMP OnClockRestart(MFTIME hnsSystemTime)
    {
        for (auto s : m_streamers)
        {
            s.second->Start(hnsSystemTime, 0);
        }
        return S_OK;
    }
    STDMETHODIMP OnClockSetRate(MFTIME hnsSystemTime, float flRate)
    {
        return S_OK;
    }
#endif
};
