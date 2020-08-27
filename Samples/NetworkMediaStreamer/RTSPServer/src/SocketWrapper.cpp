// Copyright (C) Microsoft Corporation. All rights reserved.
#include <pch.h>
#include"SocketWrapper.h"

CSocketWrapper::CSocketWrapper(SOCKET connectedSocket, winrt::array_view<PCCERT_CONTEXT> aCertContext /*= empty vector*/)
    : m_bIsSecure(!aCertContext.empty())
    , m_socket(connectedSocket)
    , m_hCred({ 0 })
    , m_hCtxt({ 0,0 })
    , m_aCertContext(aCertContext)
    , m_secPkgContextStrmSizes({ 0 })
    , m_bufSz(0)
    , m_readEvent(WSA_INVALID_EVENT)
    , m_callBackHandle(nullptr)
    , m_bIsAuthenticated(false)
{
    if (m_bIsSecure)
    {

        InitilizeSecurity();

        CHECK_WIN32(QueryContextAttributes(
            &m_hCtxt,
            SECPKG_ATTR_STREAM_SIZES,
            &m_secPkgContextStrmSizes));
        auto newBufsz = m_secPkgContextStrmSizes.cbHeader + m_secPkgContextStrmSizes.cbMaximumMessage + m_secPkgContextStrmSizes.cbTrailer;
        if (newBufsz > m_bufSz)
        {
            m_bufSz = newBufsz;
            m_pInBuf.reset(std::make_unique<BYTE[]>(m_bufSz).release());
            m_pOutBuf.reset(std::make_unique<BYTE[]>(m_bufSz).release());
        }

        // if TLS authentication fails, the rtsp session will ask for username/password digest auth
        m_bIsAuthenticated = AuthenticateClient();

    }
}
CSocketWrapper::~CSocketWrapper()
{
    //TODO: need to handle secure socket shutdown message to client
}

void CSocketWrapper::ReadDelegate(PVOID arg, BOOLEAN flag)
{
    auto pSock = (CSocketWrapper*)arg;
    WSAResetEvent(pSock->m_readEvent.get());

    SECURITY_STATUS ss = SEC_I_CONTINUE_NEEDED;
    
    auto numRead = recv(pSock->m_socket, (char*)pSock->m_pInBuf.get() , pSock->m_bufSz , 0);
    if (numRead > 0)
    {
        TimeStamp         tokenLifetime;
        SecBufferDesc     OutBuffDesc;
        SecBuffer         OutSecBuff;
        SecBufferDesc     InBuffDesc;
        SecBuffer         InSecBuff[2];
        ULONG             Attribs = ASC_REQ_MUTUAL_AUTH;

        OutBuffDesc.ulVersion = 0;
        OutBuffDesc.cBuffers = 1;
        OutBuffDesc.pBuffers = &OutSecBuff;

        OutSecBuff.cbBuffer = pSock->m_bufSz;
        OutSecBuff.BufferType = SECBUFFER_TOKEN;
        OutSecBuff.pvBuffer = pSock->m_pOutBuf.get();

        InBuffDesc.ulVersion = 0;
        InBuffDesc.cBuffers = 2;
        InBuffDesc.pBuffers = InSecBuff;

        InSecBuff[0].cbBuffer = numRead;
        InSecBuff[0].BufferType = SECBUFFER_TOKEN;
        InSecBuff[0].pvBuffer = pSock->m_pInBuf.get();

        InSecBuff[1].cbBuffer = 0;
        InSecBuff[1].BufferType = SECBUFFER_EMPTY;
        InSecBuff[1].pvBuffer = nullptr;
        bool bFirstHandshake = !(pSock->m_hCtxt.dwLower || pSock->m_hCtxt.dwUpper);
        ss = AcceptSecurityContext(
            &pSock->m_hCred,
            bFirstHandshake ? NULL : &pSock->m_hCtxt,
            &InBuffDesc,
            Attribs,
            SECURITY_NETWORK_DREP,
            bFirstHandshake ? &pSock->m_hCtxt : NULL,
            &OutBuffDesc,
            &Attribs,
            &tokenLifetime);

        if (InSecBuff[1].BufferType == SECBUFFER_EXTRA)
        {
            //TODO: handle this case
        }
        CHECK_WIN32(ss);
        send(pSock->m_socket, (char*)OutSecBuff.pvBuffer, OutSecBuff.cbBuffer, 0);
    }
    else
    {
        if (numRead < 0)
        {
            //TODO: Contructor needs to be called from factory method which can signal failure to caller.
            winrt::check_win32(WSAGetLastError());
        }
    }
    if (((SEC_I_CONTINUE_NEEDED == ss)
        || (SEC_I_COMPLETE_AND_CONTINUE == ss)))
    {
        UnregisterWait(pSock->m_callBackHandle.detach());
        RegisterWaitForSingleObject(pSock->m_callBackHandle.put(), pSock->m_readEvent.get(), (WAITORTIMERCALLBACK)ReadDelegate, pSock, INFINITE, WT_EXECUTEONLYONCE);
    }
    else
    {
        pSock->m_handshakeDone.notify_all();
    }
}

void CSocketWrapper::InitilizeSecurity()
{
    TimeStamp         Lifetime;
    PSecPkgInfo pPkgInfo = nullptr;
    CHECK_WIN32(QuerySecurityPackageInfo(
        (LPWSTR)g_lpPackageName,
        &pPkgInfo));

    auto maxTokenSz = pPkgInfo->cbMaxToken;
    m_bufSz = pPkgInfo->cbMaxToken;
    m_pInBuf = std::make_unique<BYTE[]>(pPkgInfo->cbMaxToken);
    m_pOutBuf = std::make_unique<BYTE[]>(pPkgInfo->cbMaxToken);
    FreeContextBuffer(pPkgInfo);

    // Perform Handshake
    SCH_CREDENTIALS credData;
    ZeroMemory(&credData, sizeof(credData));
    credData.dwVersion = SCH_CREDENTIALS_VERSION;
    credData.dwCredFormat = SCH_CRED_FORMAT_CERT_HASH;
    credData.cCreds = (DWORD)m_aCertContext.size();
    credData.paCred = m_aCertContext.data();

    CHECK_WIN32(AcquireCredentialsHandle(
        NULL,
        g_lpPackageName,
        SECPKG_CRED_INBOUND,
        NULL,
        &credData,
        NULL,
        NULL,
        &m_hCred,
        &Lifetime));

    m_readEvent.attach(WSACreateEvent());      // create READ wait event for our RTSP client socket
    if (m_readEvent) // == WSA_INVALID_EVENT)
    {
        winrt::check_win32(WSAGetLastError());
    }

    std::mutex mutexDone;
    std::unique_lock<std::mutex> lockDone(mutexDone);

    WSAEventSelect(m_socket, m_readEvent.get(), FD_READ | FD_CLOSE);   // select socket read event
    RegisterWaitForSingleObject(m_callBackHandle.put(), m_readEvent.get(), (WAITORTIMERCALLBACK)ReadDelegate, this, INFINITE, WT_EXECUTEONLYONCE);

    m_handshakeDone.wait(lockDone);
    UnregisterWait(m_callBackHandle.detach());

    WSACloseEvent(m_readEvent.detach());
}

int  CSocketWrapper::Recv(BYTE* buf, int sz)
{
    int ret = 0;

    if (m_bIsSecure)
    {
        ULONG ulQop = 0;
        auto cbRead = recv(m_socket, (char*)m_pInBuf.get(), (int)m_bufSz, 0);

        if (cbRead <= 0) return cbRead;

        SecBufferDesc     BuffDesc;
        SecBuffer         SecBuff[4];
        BuffDesc.ulVersion = SECBUFFER_VERSION;
        BuffDesc.cBuffers = 4;
        BuffDesc.pBuffers = SecBuff;

        SecBuff[0].cbBuffer = cbRead;
        SecBuff[0].BufferType = SECBUFFER_DATA;
        SecBuff[0].pvBuffer = m_pInBuf.get();

        SecBuff[1].cbBuffer = 0;
        SecBuff[1].BufferType = SECBUFFER_EMPTY;
        SecBuff[1].pvBuffer = nullptr;

        SecBuff[2].cbBuffer = 0;
        SecBuff[2].BufferType = SECBUFFER_EMPTY;
        SecBuff[2].pvBuffer = nullptr;

        SecBuff[3].cbBuffer = 0;
        SecBuff[3].BufferType = SECBUFFER_EMPTY;
        SecBuff[3].pvBuffer = nullptr;

        CHECK_WIN32(DecryptMessage(&m_hCtxt, &BuffDesc, 0, &ulQop));

        for (int i = 0; i < 4; i++)
        {
            if (SecBuff[i].BufferType == SECBUFFER_DATA)
            {
                //TODO: implement the case for buf is smaller than one decrypted
                // keep left over data and pass it over in next read
                // at present all callers to this function will pass large enough buffers
                winrt::check_win32(memcpy_s(buf, sz, SecBuff[i].pvBuffer, SecBuff[i].cbBuffer));
                ret = SecBuff[i].cbBuffer;
                break;
            }
        }
    }
    else
    {
        ret = recv(m_socket, (char*)buf, sz, 0);
    }
    return ret;
}

int CSocketWrapper::Send(BYTE* buf, int sz)
{
    if (m_bIsSecure)
    {
        SecBufferDesc     BuffDesc;
        SecBuffer         SecBuff[4];
        ULONG             ulQop = 0;

        BuffDesc.ulVersion = SECBUFFER_VERSION;
        BuffDesc.cBuffers = 4;
        BuffDesc.pBuffers = SecBuff;

        SecBuff[0].cbBuffer = m_secPkgContextStrmSizes.cbHeader;
        SecBuff[0].BufferType = SECBUFFER_STREAM_HEADER;
        SecBuff[0].pvBuffer = m_pOutBuf.get();

        SecBuff[1].cbBuffer = sz;
        SecBuff[1].BufferType = SECBUFFER_DATA;
        SecBuff[1].pvBuffer = (BYTE*)SecBuff[0].pvBuffer + SecBuff[0].cbBuffer;

        SecBuff[2].cbBuffer = m_secPkgContextStrmSizes.cbTrailer;
        SecBuff[2].BufferType = SECBUFFER_STREAM_TRAILER;
        SecBuff[2].pvBuffer = (BYTE*)SecBuff[1].pvBuffer + SecBuff[1].cbBuffer;

        SecBuff[3].cbBuffer = 0;
        SecBuff[3].BufferType = SECBUFFER_EMPTY;
        SecBuff[3].pvBuffer = nullptr;

        memcpy_s(SecBuff[1].pvBuffer, sz, buf, sz);

        winrt::check_win32(EncryptMessage(
            &m_hCtxt,
            ulQop,
            &BuffDesc,
            0));
        auto sent = send(m_socket, (char*)m_pOutBuf.get(), SecBuff[0].cbBuffer + SecBuff[1].cbBuffer + SecBuff[2].cbBuffer, 0);
        return (sent - SecBuff[0].cbBuffer - SecBuff[2].cbBuffer);
    }
    else
    {
        return send(m_socket, (char*)buf, sz, 0);
    }

}

bool CSocketWrapper::AuthenticateClient()
{
    DWORD cbUserName = 0;
    auto ss = ImpersonateSecurityContext(&m_hCtxt);
    if (!SEC_SUCCESS(ss))
    {
        return false;
    }

    GetUserName(NULL, &cbUserName);
    auto pUserName = (LPWSTR)malloc(cbUserName);

    if (!pUserName)
    {
        return false;
    }

    if (!GetUserName(
        pUserName,
        &cbUserName))
    {
        return false;
    }
    else
    {
        m_clientUserName = pUserName;
    }

    //-----------------------------------------------------------------   
    //  Revert to self.
    winrt::check_win32(RevertSecurityContext(&m_hCtxt));
    
    return true;
}
