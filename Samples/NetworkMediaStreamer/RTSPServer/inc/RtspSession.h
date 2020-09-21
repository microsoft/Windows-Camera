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
#define RTP_DEFAULT_PORT 54554
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

    RTSP_CMD HandleRequest(char const* aRequest, unsigned aRequestSize);
    int GetStreamID();

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

    int            m_RtspSessionID;
    winrt::delegate<RTSPSession*> m_Completed;
    std::unique_ptr<CSocketWrapper> m_pRtspClient;
    std::string    m_RtspClientAddr;
    u_short  m_RtspPort;
    u_short  m_LocalRTPPort;                           // client port for UDP based RTP transport
    u_short  m_LocalRTCPPort;                          // client port for UDP based RTCP transport  

    u_short  m_ClientRTPPort;                           // client port for UDP based RTP transport
    u_short  m_ClientRTCPPort;                          // client port for UDP based RTCP transport  
    bool     m_TcpTransport;                            // if Tcp based streaming was activated
    uint32_t m_ssrc;
    winrt::Windows::Foundation::Collections::PropertySet m_streamers;
    winrt::com_ptr<IMFStreamSink> m_spCurrentStreamer;

    // parameters of the last received RTSP request
    std::string           m_CSeq;             // RTSP command sequence number
    std::string           m_URLHostPort;      // host:port part of the URL
    std::string           m_URLProto;
    std::string           m_curAuthSessionMsg;
    winrt::com_ptr<IRTSPAuthProvider> m_spAuthProvider;
    winrt::handle m_RtspReadEvent;
    winrt::handle m_callBackHandle;
    winrt::PacketHandler m_packetHandler;
    bool m_bStreamingStarted,m_bTerminate, m_bAuthorizationReceived;
    std::unique_ptr<BYTE[]> m_pTcpTxBuff;
    std::unique_ptr<BYTE[]> m_pTcpRxBuff;
    std::string m_urlSuffix;
    std::mutex m_readDelegateMutex;
    winrt::event<winrt::LogHandler>* m_pLoggerEvents;
};
