// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once
#define DBGLEVEL 1
#include "RtspSession.h"

class RTSPServer : public winrt::implements<RTSPServer, IRTSPServerControl>
{
public:
    RTSPServer(streamerMapType streamers, uint16_t socketPort, IRTSPAuthProvider *pAuthProvider, std::vector<PCCERT_CONTEXT> serverCerts)
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
        m_spAuthProvider.copy_from(pAuthProvider);
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

private:

    streamerMapType m_streamers;

    std::map<RTSPSession*, SOCKET> m_Sessions;
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
    winrt::com_ptr<IRTSPAuthProvider> m_spAuthProvider;
};
