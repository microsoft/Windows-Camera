// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for more information

#pragma comment(lib, "Ws2_32.lib")
#include <pch.h>

STDMETHODIMP RTSPServer::StopServer()
{
    HRESULT_EXCEPTION_BOUNDARY_START;
    auto apiLock = std::lock_guard(m_apiGuard);
    if (m_callbackHandle)
    {
        UnregisterWait(m_callbackHandle.detach());
    }
    WSACloseEvent(m_acceptEvent.detach());

    m_Sessions.clear();

    closesocket(m_masterSocket);
    WSACleanup();

    HRESULT_EXCEPTION_BOUNDARY_END;
}

STDMETHODIMP RTSPServer::StartServer()
{
    HRESULT_EXCEPTION_BOUNDARY_START;
    auto apiLock = std::lock_guard(m_apiGuard);
    if (m_callbackHandle)
    {
        //this means server is already started.
        return S_OK;
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

    m_masterSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, 0);
    if (m_masterSocket == INVALID_SOCKET)
    {
        winrt::check_win32(WSAGetLastError());
    }
    // bind our master socket to the RTSP port and listen for a client connection
    if (bind(m_masterSocket, (sockaddr*)&ServerAddr, sizeof(ServerAddr)) != 0)
    {
        winrt::check_win32(WSAGetLastError());
    }
    if (listen(m_masterSocket, 5) != 0)
    {
        winrt::check_win32(WSAGetLastError());
    }
    m_acceptEvent.attach(WSACreateEvent());
    if (!m_acceptEvent)
    {
        winrt::check_win32(WSAGetLastError());
    }

    if (WSAEventSelect(m_masterSocket, m_acceptEvent.get(), FD_ACCEPT) != 0)
    {
        winrt::check_win32(WSAGetLastError());
    }
    winrt::check_bool(RegisterWaitForSingleObject(m_callbackHandle.put(), m_acceptEvent.get(), [](PVOID arg, BOOLEAN flag)
        {
            auto pServer = (RTSPServer*)arg;
            SOCKET      clientSocket;                                 // RTSP socket to handle an client
            try
            {
                auto apiLock = std::lock_guard(pServer->m_apiGuard);
                if (!pServer->m_callbackHandle)
                {
                    // Server stopped 
                    return;
                }
                WSAResetEvent(pServer->m_acceptEvent.get());

                sockaddr_in ClientAddr;                                   // address parameters of a new RTSP client
                int         ClientAddrLen = sizeof(ClientAddr);
                char addr[INET_ADDRSTRLEN];

                // loop forever to accept client connections
                clientSocket = WSAAccept(pServer->m_masterSocket, (struct sockaddr*)&ClientAddr, &ClientAddrLen, nullptr, NULL);
                if (clientSocket == INVALID_SOCKET)
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
                        pClientSocketWrapper = std::make_unique<CSocketWrapper>(clientSocket, pServer->m_serverCerts);
                    }
                    else
                    {
                        pClientSocketWrapper = std::make_unique<CSocketWrapper>(clientSocket);
                    }
                }
                catch (winrt::hresult_error const& ex)
                {
                    if (clientSocket != INVALID_SOCKET)
                    {
                        closesocket(clientSocket);
                    }
                    pServer->m_LoggerEvents[(int)LoggerType::ERRORS](ex.code(), L"\nFailed to Create Socket wrapper:" + ex.message());
                    return;
                }
                
                pServer->m_Sessions.insert(
                    { 
                    clientSocket,
                    std::make_unique<RTSPSession>(pClientSocketWrapper.release(), pServer->m_streamers, pServer->m_spAuthProvider.get(), pServer->m_LoggerEvents) 
                    });
                pServer->m_SessionStatusEvents(pServer->m_Sessions[clientSocket]->GetStreamID(), SessionStatus::SessionStarted);
                pServer->m_LoggerEvents[(int)LoggerType::OTHER](S_OK, L"\nStarting session:" + winrt::to_hstring(pServer->m_Sessions[clientSocket]->GetStreamID()));

                pServer->m_Sessions[clientSocket]->BeginSession([pServer](RTSPSession* pSession)
                    {
                        pServer->m_LoggerEvents[(int)LoggerType::OTHER](S_OK, L"\nSession completed:" + winrt::to_hstring(pSession->GetStreamID()));
                        pServer->m_SessionStatusEvents(pSession->GetStreamID(), SessionStatus::SessionEnded);
                        pServer->m_Sessions.erase(pSession->GetSocket());
                    });
            }
            catch (...)
            {
                auto hr = winrt::to_hresult();
                if (clientSocket != INVALID_SOCKET)
                {
                    closesocket(clientSocket);
                }
                pServer->m_LoggerEvents[(int)LoggerType::ERRORS](hr, L"\nFailed to Create Session");
            }
        }, this, INFINITE, WT_EXECUTEINWAITTHREAD));

    HRESULT_EXCEPTION_BOUNDARY_END;
}

RTSPSERVER_API STDMETHODIMP CreateRTSPServer(ABI::RTSPSuffixSinkMap* streamers, uint16_t socketPort, bool bSecure, IRTSPAuthProvider* pAuthProvider, PCCERT_CONTEXT* serverCerts, size_t uCertCount, IRTSPServerControl** ppRTSPServerControl /*=empty*/)
{
    HRESULT_EXCEPTION_BOUNDARY_START;

    winrt::check_pointer(ppRTSPServerControl);
    if (bSecure && !uCertCount)
    {
        winrt::check_hresult(SEC_E_NO_CREDENTIALS);
    }
    winrt::com_ptr<RTSPServer> spRTSPServer;
    spRTSPServer.attach(new RTSPServer(streamers, socketPort, pAuthProvider, serverCerts,uCertCount));
    *ppRTSPServerControl = spRTSPServer.as<IRTSPServerControl>().detach();

    HRESULT_EXCEPTION_BOUNDARY_END;
}

