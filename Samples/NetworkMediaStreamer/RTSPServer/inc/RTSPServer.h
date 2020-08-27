// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once
#define DBGLEVEL 1
#include "RTSPServerControl.h"
#include "RtspSession.h"

class RTSPServer : public winrt::implements<RTSPServer, IRTSPServerControl>
{
public:
    RTSPServer(RTSPSuffixSinkMapView streamers, uint16_t socketPort, IRTSPAuthProvider *pAuthProvider, winrt::array_view<PCCERT_CONTEXT> serverCerts)
        : m_acceptEvent(nullptr)
        , m_callbackHandle(nullptr)
        , m_socketPort(socketPort)
        , m_streamers(streamers)
        , m_bSecure(!serverCerts.empty())
        , m_masterSocket(INVALID_SOCKET)
        , m_bIsShutdown(false)
    {
        m_serverCerts = winrt::com_array<PCCERT_CONTEXT>(serverCerts.size());
        for (uint32_t i=0; i < serverCerts.size(); i++)
        {
            m_serverCerts[i] = CertDuplicateCertificateContext(serverCerts[i]);
        }
        m_spAuthProvider.copy_from(pAuthProvider);
    }
    virtual  ~RTSPServer()
    {
        StopServer();
    }

    // IRTSPServerControl
    STDMETHODIMP AddLogHandler(LoggerType type, winrt::delegate < winrt::hresult, winrt::hstring> handler, winrt::event_token & token)
    {
        HRESULT_EXCEPTION_BOUNDARY_START;
        token = m_LoggerEvents[(int)type].add(handler);
        HRESULT_EXCEPTION_BOUNDARY_END;
    }
    STDMETHODIMP RemoveLogHandler(LoggerType type, winrt::event_token token)
    {
        HRESULT_EXCEPTION_BOUNDARY_START;
        m_LoggerEvents[(int)type].remove(token);
        HRESULT_EXCEPTION_BOUNDARY_END;
    }

    STDMETHODIMP AddSessionStatusHandler(LoggerType type, winrt::delegate <uint64_t,SessionStatus> handler, winrt::event_token & token)
    {
        HRESULT_EXCEPTION_BOUNDARY_START;
        token = m_SessionStatusEvents.add(handler);
        HRESULT_EXCEPTION_BOUNDARY_END;
    }
    STDMETHODIMP RemoveSessionStatusHandler(LoggerType type, winrt::event_token token)
    {
        HRESULT_EXCEPTION_BOUNDARY_START;
        m_SessionStatusEvents.remove(token);
        HRESULT_EXCEPTION_BOUNDARY_END;
    }
    STDMETHODIMP StartServer();
    STDMETHODIMP StopServer();

private:

    RTSPSuffixSinkMapView m_streamers;

    std::map<RTSPSession*, SOCKET> m_Sessions;
    SOCKET      m_masterSocket;                                 // our masterSocket(socket that listens for RTSP client connections)  
    winrt::handle m_acceptEvent;
    winrt::handle m_callbackHandle;
    uint16_t m_socketPort;
    bool m_bSecure;
    winrt::com_array<PCCERT_CONTEXT> m_serverCerts;
    winrt::event<winrt::delegate<winrt::hresult, winrt::hstring>> m_LoggerEvents[(size_t)LoggerType::LOGGER_MAX];
    winrt::event<winrt::delegate<uint64_t, SessionStatus>> m_SessionStatusEvents;
    winrt::com_ptr<IMFPresentationClock> m_spClock;
    bool m_bIsShutdown;
    std::mutex m_apiGuard;
    winrt::com_ptr<IRTSPAuthProvider> m_spAuthProvider;
};
