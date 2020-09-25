// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for more information

#pragma once

constexpr LPWSTR g_lpPackageName = (LPWSTR)UNISP_NAME;

#define SEC_SUCCESS(Status) ((Status) >= 0)

class CSocketWrapper
{
public:

    CSocketWrapper(SOCKET connectedSocket, winrt::array_view<PCCERT_CONTEXT> aCertContext = winrt::com_array<PCCERT_CONTEXT>());
    virtual ~CSocketWrapper();
    int Recv(BYTE* buf, int sz);
    int Send(BYTE* buf, int sz);

    SOCKET GetSocket()
    {
        return m_socket;
    }

    bool IsClientCertAuthenticated()
    {
        return m_bIsAuthenticated;
    }

    bool IsSecure()
    {
        return m_bIsSecure;
    }

    std::wstring GetClientCertUserName()
    {
        return m_clientUserName;
    }

private:

    static void ReadDelegate(PVOID arg, BOOLEAN flag);
    void InitializeSecurity();
    bool AuthenticateClient();
    bool m_bIsSecure;
    SOCKET m_socket;
    bool m_bIsAuthenticated;

    CredHandle m_hCred;
    struct _SecHandle  m_hCtxt;
    SECURITY_STATUS m_securityStatus;
    std::unique_ptr<BYTE[]> m_pInBuf;
    std::unique_ptr<BYTE[]> m_pOutBuf;
    DWORD m_bufSz;
    winrt::array_view<PCCERT_CONTEXT> m_aCertContext;
    SecPkgContext_StreamSizes m_secPkgContextStrmSizes;
    winrt::handle m_readEvent;
    std::condition_variable m_handshakeDone;
    winrt::handle m_callBackHandle;
    std::wstring m_clientUserName;
};