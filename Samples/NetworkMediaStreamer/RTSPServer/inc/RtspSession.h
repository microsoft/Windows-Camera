// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for more information

#pragma once

// supported command types
enum class RTSP_CMD
{
    UNKNOWN = 0,
    OPTIONS,
    DESCRIBE,
    SETUP,
    PLAY,
    TEARDOWN
};
#define RTP_DEFAULT_PORT       54554
#define RTSP_BUFFER_SIZE       10000    // for incoming requests, and outgoing responses
#define RTSP_PARAM_STRING_MAX  200
class RTSPSession
{
public:
    RTSPSession(
        CSocketWrapper* rtspClientSocket,
        winrt::Windows::Foundation::Collections::PropertySet streamers,
        IRTSPAuthProvider* pAuthProvider,
        winrt::event<winrt::LogHandler>* m_pLoggers);
    virtual ~RTSPSession();

    virtual RTSP_CMD HandleRequest(char const* aRequest, unsigned aRequestSize);
    int GetStreamID();
    SOCKET GetSocket() 
    { 
        return m_pRtspClient->GetSocket(); 
    }
    void BeginSession(winrt::delegate<RTSPSession*> completed);
private:
    void Init();
    void InitUDPTransport();
    void InitTCPTransport();
    RTSP_CMD ParseRequest(char const* aRequest, unsigned aRequestSize);
    char const* DateHeader();
    std::string m_dest;
    // RTSP request command handlers
    void HandleCmdOPTIONS();
    void HandleCmdDESCRIBE();
    void HandleCmdSETUP();
    void HandleCmdPLAY();
    void HandleCmdTEARDOWN();
    void Handle_RtspPAUSE();
    void StopIfStreaming();
    void SendToClient(std::string Response);

    int            m_rtspSessionID;
    winrt::delegate<RTSPSession*> m_sessionCompleted;
    std::unique_ptr<CSocketWrapper> m_pRtspClient;
    std::string    m_rtspClientAddr;
    u_short  m_rtspPort;
    u_short  m_localRTPPort;                           // client port for UDP based RTP transport
    u_short  m_localRTCPPort;                          // client port for UDP based RTCP transport  

    u_short  m_clientRTPPort;                           // client port for UDP based RTP transport
    u_short  m_clientRTCPPort;                          // client port for UDP based RTCP transport  
    bool     m_bTcpTransport;                            // if Tcp based streaming was activated
    uint32_t m_ssrc;
    winrt::Windows::Foundation::Collections::PropertySet m_streamers;
    winrt::com_ptr<IMFStreamSink> m_spCurrentStreamer;

    // parameters of the last received RTSP request
    std::string           m_strCSeq;             // RTSP command sequence number
    std::string           m_urlHostPort;      // host:port part of the URL
    std::string           m_urlProto;
    std::string           m_curAuthSessionMsg;
    winrt::com_ptr<IRTSPAuthProvider> m_spAuthProvider;
    winrt::handle m_rtspReadEvent;
    winrt::handle m_callBackHandle;
    winrt::PacketHandler m_packetHandler;
    bool m_bStreamingStarted,m_bTerminate, m_bAuthorizationReceived;
    std::unique_ptr<BYTE[]> m_pTcpTxBuff;
    std::unique_ptr<BYTE[]> m_pTcpRxBuff;
    std::string m_urlSuffix;
    std::mutex m_readDelegateMutex;
    winrt::event<winrt::LogHandler>* m_pLoggerEvents;
};
