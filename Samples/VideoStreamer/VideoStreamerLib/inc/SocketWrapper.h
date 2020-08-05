// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once
#include "VideoStreamerInternal.h"

constexpr LPWSTR g_lpPackageName = (LPWSTR)UNISP_NAME;
#define CHECK_WIN32(result) {auto r = (result); if(r <0)  winrt::check_win32(r);}
#define SEC_SUCCESS(Status) ((Status) >= 0)

class CSocketWrapper
{
public:

    CSocketWrapper(SOCKET connectedSocket, std::vector<PCCERT_CONTEXT> aCertContext = std::vector<PCCERT_CONTEXT>(0));
    virtual ~CSocketWrapper();
    int Recv(BYTE* buf, int sz);
    int Send(BYTE* buf, int sz);
    SOCKET GetSocket() { return m_socket; }
    bool IsClientAuthenticated() { return m_bIsAuthenticated; }
    std::wstring GetClientUserName() { return m_clientUserName; }
private:

    static void ReadDelegate(PVOID arg, BOOLEAN flag);
    void InitilizeSecurity();
    bool AuthenticateClient();
    bool m_bIsSecure;
    SOCKET m_socket;
    bool m_bIsAuthenticated;

    CredHandle m_hCred;
    struct _SecHandle  m_hCtxt;

    std::unique_ptr<BYTE[]> m_pInBuf;
    std::unique_ptr<BYTE[]> m_pOutBuf;
    DWORD m_bufSz;
    std::vector<PCCERT_CONTEXT> m_aCertContext;
    SecPkgContext_StreamSizes m_secPkgContextStrmSizes;
    WSAEVENT m_readEvent;
    std::condition_variable m_handshakeDone;
    HANDLE m_callBackHandle;
    std::mutex m;
    std::wstring m_clientUserName;
};