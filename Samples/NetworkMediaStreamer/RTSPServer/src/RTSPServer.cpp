// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma comment(lib, "Ws2_32.lib")

#include "RTSPServer.h"

using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::System::Threading;


void RTSPServer::StopServer()
{
    auto apiLock = std::lock_guard(m_apiGuard);
    if (m_callbackHandle)
    {
        UnregisterWait(m_callbackHandle);
        m_callbackHandle = NULL;
    }
    WSACloseEvent(m_acceptEvent);

    for (auto& session : m_Sessions)
    {
        delete session.first;
    }
    m_Sessions.clear();

    closesocket(MasterSocket);
    WSACleanup();
}

void RTSPServer::StartServer()
{
    auto apiLock = std::lock_guard(m_apiGuard);
    if (m_callbackHandle)
    {
        //this means server is already started.
        return;
    }

    sockaddr_in ServerAddr;                                   // server address parameters
    WSADATA     WsaData;

    int ret = WSAStartup(0x101, &WsaData);
    if (ret != 0)
    {
        winrt::check_win32(WSAGetLastError());
    }

    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_addr.s_addr = INADDR_ANY;
    ServerAddr.sin_port = htons(m_socketPort);                 // listen on RTSP port

    MasterSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, 0);
    if (MasterSocket == INVALID_SOCKET)
    {
        winrt::check_win32(WSAGetLastError());
    }
    // bind our master socket to the RTSP port and listen for a client connection
    if (bind(MasterSocket, (sockaddr*)&ServerAddr, sizeof(ServerAddr)) != 0)
    {
        winrt::check_win32(WSAGetLastError());
    }
    if (listen(MasterSocket, 5) != 0)
    {
        winrt::check_win32(WSAGetLastError());
    }
    m_acceptEvent = WSACreateEvent();
    if (WSA_INVALID_EVENT == m_acceptEvent)
    {
        winrt::check_win32(WSAGetLastError());
    }

    if (WSAEventSelect(MasterSocket, m_acceptEvent, FD_ACCEPT) != 0)
    {
        winrt::check_win32(WSAGetLastError());
    }
    winrt::check_bool(RegisterWaitForSingleObject(&m_callbackHandle, m_acceptEvent, [](PVOID arg, BOOLEAN flag)
        {
            auto pServer = (RTSPServer*)arg;
            auto apiLock = std::lock_guard(pServer->m_apiGuard);
            if (pServer->m_callbackHandle == NULL)
            {
                // Server stopped 
                return;
            }
            WSAResetEvent(pServer->m_acceptEvent);
            SOCKET      ClientSocket;                                 // RTSP socket to handle an client
            sockaddr_in ClientAddr;                                   // address parameters of a new RTSP client
            int         ClientAddrLen = sizeof(ClientAddr);
            char addr[INET_ADDRSTRLEN];

            // loop forever to accept client connections
            ClientSocket = WSAAccept(pServer->MasterSocket, (struct sockaddr*) & ClientAddr, &ClientAddrLen, nullptr, NULL);
            if (ClientSocket == INVALID_SOCKET)
            {
                pServer->m_LoggerEvents[(int)LoggerType::ERRORS](E_HANDLE, L"\nInvalid client socket returned by WSAAccept");
                return;
            }
            inet_ntop(AF_INET, &(ClientAddr.sin_addr), addr, INET_ADDRSTRLEN);

            pServer->m_LoggerEvents[(int)LoggerType::OTHER](S_OK, winrt::hstring(L"\nConnected to client with address: ") + winrt::to_hstring(addr));
            std::unique_ptr<CSocketWrapper> pClientSocketWrapper;
            try
            { // TODO: use a factory to return errors instead of try-throw-catch here
                if (pServer->m_bSecure)
                {
                    pClientSocketWrapper = std::make_unique<CSocketWrapper>(ClientSocket, pServer->m_serverCerts);
                }
                else
                {
                    pClientSocketWrapper = std::make_unique<CSocketWrapper>(ClientSocket);
                }
            }
            catch (winrt::hresult_error const& ex)
            {
                pServer->m_LoggerEvents[(int)LoggerType::ERRORS](ex.code(), L"\nFailed to Create Socket wrapper:" + ex.message());
                return;
            }
            RTSPSession* pRtspSession = new RTSPSession(pClientSocketWrapper.release(), pServer->m_streamers, pServer->m_spAuthProvider.get(), pServer->m_LoggerEvents);
            pServer->m_Sessions.insert({ pRtspSession, ClientSocket });
            pServer->m_SessionStatusEvents((uintptr_t)pRtspSession, SessionStatus::SessionStarted);

            pServer->m_LoggerEvents[(int)LoggerType::OTHER](S_OK, L"\nStarting session:" + winrt::to_hstring((uintptr_t)pRtspSession));

            pRtspSession->BeginSession([pServer](RTSPSession* pSession)
                {
                    pServer->m_LoggerEvents[(int)LoggerType::OTHER](S_OK, L"\nSession completed:" + winrt::to_hstring((uintptr_t)pSession));

                    pServer->m_Sessions.erase(pSession);
                    pServer->m_SessionStatusEvents((uintptr_t)pSession, SessionStatus::SessionEnded);
                    delete pSession;

                });
        }, this, INFINITE, WT_EXECUTEINWAITTHREAD));
}

RTSPSERVER_API IRTSPServerControl* CreateRTSPServer(streamerMapType streamers, uint16_t socketPort, bool bSecure, IRTSPAuthProvider* pAuthProvider, std::vector<PCCERT_CONTEXT> serverCerts /*=empty*/)
{
    if (bSecure && serverCerts.empty())
    {
        winrt::check_hresult(SEC_E_NO_CREDENTIALS);
    }
    IRTSPServerControl* pRtspServerControl = nullptr;
    auto pRTSPServer = new RTSPServer(streamers, socketPort, pAuthProvider, serverCerts);
    winrt::check_hresult(pRTSPServer->QueryInterface(__uuidof(IRTSPServerControl),(void**)&pRtspServerControl));
    return pRtspServerControl;
}
