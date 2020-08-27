// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once
#include "NwMediaStreamSinkBase.h"
#include "RTPMediaStreamer.h"

constexpr BYTE h264payloadType = 96;
constexpr size_t rtpHeaderSize = 12;
constexpr size_t stapAtypeHeaderSize = 1;
constexpr size_t stapASzHeaderSize = 2;
class TxContext
{
    uint16_t m_localRTPPort, m_localRTCPPort, m_remotePort;
    sockaddr_in m_remoteAddr;
    SOCKET m_RtpSocket, m_RtcpSocket;
    winrt::delegate<BYTE*, size_t> m_packetHandler;

public:
    TxContext(std::string destination, winrt::delegate<BYTE*, size_t> packetHandler = nullptr);
    void SendPacket(BYTE* buf, size_t sz);

    uint32_t m_ssrc;
    uint64_t m_u64StartTime;
    uint32_t m_SequenceNumber;
};

class RTPVideoStreamSink  sealed : public NwMediaStreamSinkBase
{
    std::mutex m_guardlock;
    std::map<std::string, TxContext> m_RtpStreamers;
    std::unique_ptr<BYTE[]> m_pTxBuf;
    size_t m_MTUSize;
    uint32_t m_packetizationMode;
    uint32_t m_SequenceNumber;
    RTPVideoStreamSink(IMFMediaType *pMT, IMFMediaSink* pParent, DWORD dwStreamID);
    virtual ~RTPVideoStreamSink ();
    BYTE* FindSC(BYTE* bufStart, BYTE* bufEnd);
    STDMETHODIMP PacketizeAndSend(IMFSample *pSample);
    void PacketizeMode0(BYTE* bufIn, size_t szIn, LONGLONG llSampleTime);

    void SendPacket(BYTE* pOut, size_t sz, LONGLONG ts, bool bLastNalOfFrame);
    void PacketizeMode1(BYTE* bufIn, size_t szIn, LONGLONG llSampleTime);
public:
    static INetworkMediaStreamSink* CreateInstance(IMFMediaType *pMediaType, IMFMediaSink* pParent, DWORD dwStreamID);

    STDMETHODIMP AddTransportHandler(winrt::delegate<BYTE*, size_t> packetHandler, winrt::hstring protocol = L"rtp", uint32_t ssrc = 0) override;
    STDMETHODIMP AddNetworkClient(winrt::hstring destination, winrt::hstring protocol = L"rtp", uint32_t ssrc = 0) override;
    STDMETHODIMP RemoveNetworkClient(winrt::hstring destination) override;
    STDMETHODIMP RemoveTransportHandler(winrt::delegate<BYTE*, size_t> packetHandler) override;
    STDMETHODIMP GenerateSDP(uint8_t* buf, size_t maxSize, winrt::hstring destination) override;

};

