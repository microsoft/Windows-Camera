// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for more information

#pragma once
#define DBGLEVEL 1

class RTSPServer : public winrt::implements<RTSPServer, IRTSPServerControl>
{
public:
    RTSPServer(ABI::RTSPSuffixSinkMap* streamers, uint16_t socketPort, IRTSPAuthProvider* pAuthProvider, PCCERT_CONTEXT* serverCerts, size_t uCertCount)
        : m_acceptEvent(nullptr)
        , m_callbackHandle(nullptr)
        , m_socketPort(socketPort)
        , m_bSecure(uCertCount)
        , m_masterSocket(INVALID_SOCKET)
        , m_bIsShutdown(false)
    {
        winrt::copy_from_abi(m_streamers, streamers);
        uCertCount&& winrt::check_pointer(serverCerts);
        m_serverCerts = winrt::com_array<PCCERT_CONTEXT>((uint32_t)uCertCount);
        for (uint32_t i = 0; i < uCertCount; i++)
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
    STDMETHODIMP AddLogHandler(LoggerType type, ABI::LogHandler* handler, EventRegistrationToken* pToken) override try
    {
        winrt::check_pointer(pToken);
        winrt::check_pointer(handler);
        winrt::LogHandler h;
        winrt::copy_from_abi(h, handler);
        auto token = m_loggerEvents[(int)type].add(h);
        winrt::copy_to_abi(token, *pToken);
        return S_OK;
    }HRESULT_EXCEPTION_BOUNDARY_FUNC

    STDMETHODIMP RemoveLogHandler(LoggerType type, EventRegistrationToken token) override try
    {
        winrt::event_token tk;
        winrt::copy_from_abi(tk, token);
        m_loggerEvents[(int)type].remove(tk);
        return S_OK;
    }HRESULT_EXCEPTION_BOUNDARY_FUNC

    STDMETHODIMP AddSessionStatusHandler(LoggerType type, ABI::SessionStatusHandler* handler, EventRegistrationToken* pToken) override try
    {
        winrt::SessionStatusHandler h;
        winrt::copy_from_abi(h, handler);
        auto token = m_sessionStatusEvents.add(h);
        winrt::copy_to_abi(token, *pToken);
        return S_OK;
    }HRESULT_EXCEPTION_BOUNDARY_FUNC

    STDMETHODIMP RemoveSessionStatusHandler(LoggerType type, EventRegistrationToken token) override try
    {
        winrt::event_token tk;
        winrt::copy_from_abi(tk, token);
        m_sessionStatusEvents.remove(tk);
        return S_OK;
    }HRESULT_EXCEPTION_BOUNDARY_FUNC

    STDMETHODIMP StartServer() override;
    STDMETHODIMP StopServer() override;

private:

    winrt::RTSPSuffixSinkMap m_streamers;

    std::map<SOCKET, std::unique_ptr<RTSPSession>> m_rtspSessions;
    SOCKET      m_masterSocket;                                 // our masterSocket(socket that listens for RTSP client connections)  
    winrt::handle m_acceptEvent;
    winrt::handle m_callbackHandle;
    uint16_t m_socketPort;
    bool m_bSecure;
    winrt::com_array<PCCERT_CONTEXT> m_serverCerts;
    winrt::event<winrt::LogHandler> m_loggerEvents[(size_t)LoggerType::LOGGER_MAX];
    winrt::event<winrt::SessionStatusHandler> m_sessionStatusEvents;
    winrt::com_ptr<IMFPresentationClock> m_spClock;
    bool m_bIsShutdown;
    std::mutex m_apiGuard;
    winrt::com_ptr<IRTSPAuthProvider> m_spAuthProvider;
};
