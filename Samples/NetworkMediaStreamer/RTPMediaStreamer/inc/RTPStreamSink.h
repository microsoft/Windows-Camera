// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

constexpr BYTE h264payloadType = 96;
constexpr size_t rtpHeaderSize = 12;
constexpr size_t stapAtypeHeaderSize = 1;
constexpr size_t stapASzHeaderSize = 2;
class TxContext final
{
    uint16_t m_localRTPPort, m_localRTCPPort, m_remotePort;
    sockaddr_in m_remoteAddr;
    SOCKET m_RtpSocket, m_RtcpSocket;
    winrt::PacketHandler m_packetHandler;

public:
    TxContext(std::string destination, winrt::PacketHandler packetHandler = nullptr);
    ~TxContext();
    void SendPacket(winrt::Windows::Storage::Streams::IBuffer buf);

    uint32_t m_ssrc;
    uint64_t m_u64StartTime;
    uint32_t m_SequenceNumber;
};

class RTPVideoStreamSink  final : public NwMediaStreamSinkBase
{
    std::mutex m_guardlock;
    std::map<std::string, std::unique_ptr<TxContext>> m_RtpStreamers;
    winrt::Windows::Storage::Streams::Buffer m_pTxBuf;
    size_t m_MTUSize;
    uint32_t m_packetizationMode;
    uint32_t m_SequenceNumber;
    RTPVideoStreamSink(IMFMediaType *pMT, IMFMediaSink* pParent, DWORD dwStreamID);
    virtual ~RTPVideoStreamSink () = default;
    BYTE* FindSC(BYTE* bufStart, BYTE* bufEnd);
    STDMETHODIMP PacketizeAndSend(IMFSample *pSample);
    void PacketizeMode0(BYTE* bufIn, size_t szIn, LONGLONG llSampleTime);

    void SendPacket(winrt::Windows::Storage::Streams::IBuffer buf, LONGLONG ts, bool bLastNalOfFrame);
    void PacketizeMode1(BYTE* bufIn, size_t szIn, LONGLONG llSampleTime);
public:
    static INetworkMediaStreamSink* CreateInstance(IMFMediaType *pMediaType, IMFMediaSink* pParent, DWORD dwStreamID);

    STDMETHODIMP AddTransportHandler(ABI::PacketHandler* packetHandler, LPCWSTR protocol = L"rtp", LPCWSTR params = L"") override;
    STDMETHODIMP AddNetworkClient(LPCWSTR destination, LPCWSTR protocol = L"rtp", LPCWSTR param = L"") override;
    STDMETHODIMP RemoveNetworkClient(LPCWSTR destination) override;
    STDMETHODIMP RemoveTransportHandler(ABI::PacketHandler* packetHandler) override;
    STDMETHODIMP GenerateSDP(uint8_t* buf, size_t maxSize, LPCWSTR dest) override;

};

