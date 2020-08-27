// Copyright (C) Microsoft Corporation. All rights reserved.
#include <pch.h>
#include "RTPStreamSink.h"

TxContext::TxContext(std::string destination, winrt::delegate<BYTE*, size_t> packetHandler /*= nullptr*/)
    : m_u64StartTime(0)
    , m_packetHandler(packetHandler)
    , m_ssrc(0)
    , m_RtpSocket(INVALID_SOCKET)
    , m_RtcpSocket(INVALID_SOCKET)
    , m_localRTPPort(0)
    , m_localRTCPPort(0)
    , m_remotePort(0)
    , m_SequenceNumber(0)
{
    memset(&m_remoteAddr, 0, sizeof(m_remoteAddr));
    if (!packetHandler)
    {
        std::string param = "localrtpport=";
        auto sep1 = destination.find(":");
        auto ipaddr = destination.substr(0, sep1);
        auto sep2 = destination.find(param);
        m_remotePort = std::stoi(destination.substr(sep1 + 1, sep2 - sep1 - 1));
        if (sep2 != std::string::npos)
        {
            auto sep3 = destination.find("&", sep2);
            m_localRTPPort = std::stoi(destination.substr(sep2 + param.size(), sep3 - sep2));
        }
        else
        {
            m_localRTPPort = 6970;
        }
        //else
        {
            sockaddr_in Server;
            Server.sin_family = AF_INET;
            Server.sin_addr.s_addr = INADDR_ANY;
            for (u_short P = m_localRTPPort; P < 0xFFFE; P += 2)
            {
                m_RtpSocket = socket(AF_INET, SOCK_DGRAM, 0);
                Server.sin_port = htons(P);
                if (bind(m_RtpSocket, (sockaddr*)&Server, sizeof(Server)) == 0)
                {   // Rtp socket was bound successfully. Lets try to bind the consecutive Rtsp socket
                    m_RtcpSocket = socket(AF_INET, SOCK_DGRAM, 0);
                    Server.sin_port = htons(P + 1);
                    if (bind(m_RtcpSocket, (sockaddr*)&Server, sizeof(Server)) == 0)
                    {
                        m_localRTPPort = P;
                        m_localRTCPPort = P + 1;
                        break;
                    }
                    else
                    {
                        closesocket(m_RtpSocket);
                        closesocket(m_RtcpSocket);
                    };
                }
                else closesocket(m_RtpSocket);
            }
        }
        inet_pton(AF_INET, destination.c_str(), &(m_remoteAddr.sin_addr));
        m_remoteAddr.sin_port = htons(m_remotePort);
        m_remoteAddr.sin_family = AF_INET;
    }
}

void TxContext::SendPacket(BYTE* buf, size_t sz)
{
    if (m_packetHandler)
    {
        m_packetHandler(buf, sz);
    }
    else
    {
        sendto(m_RtpSocket, (char*)buf, (int)sz, 0, (SOCKADDR*)&m_remoteAddr, sizeof(m_remoteAddr));
    }
    m_SequenceNumber++;
}


RTPVideoStreamSink::RTPVideoStreamSink(IMFMediaType* pMediaType, IMFMediaSink* pParent, DWORD dwStreamID)
    : NwMediaStreamSinkBase(pMediaType, pParent, dwStreamID)
    , m_packetizationMode(1)
    , m_SequenceNumber(0)
{
    if (m_packetizationMode == 1)
    {
        m_MTUSize = 1500;
    }
    else
    {
        //TODO: find a way to control encoder's max NAL size and edit this param to a better value
        m_MTUSize = 65535; // max size to allow any size NAL into one packet
    }
    m_pTxBuf = std::make_unique<BYTE[]>(m_MTUSize);//new BYTE[m_MTUSize];

}
RTPVideoStreamSink ::~RTPVideoStreamSink()
{
    //delete[] m_pTxBuf;
}
BYTE* RTPVideoStreamSink::FindSC(BYTE* bufStart, BYTE* bufEnd)
{
    auto it = bufStart;
    while (it < bufEnd - 3)
    {
        if ((it[0] == 0) && (it[1] == 0) && (it[2] == 1)
            || (*((uint32_t*)it) == 0x01000000)
            )
        {
            return it;
        }
        while (*(++it) != 0);
    }
    return bufEnd;
}

void RTPVideoStreamSink::PacketizeMode0(BYTE* bufIn, size_t szIn, LONGLONG llSampleTime)
{
    BYTE* sc = FindSC(bufIn, bufIn + szIn);
    //BYTE* pOut = m_pTxBuf;
    size_t szOut = m_MTUSize;
    while (sc < (bufIn + szIn))
    {
        while ((sc < bufIn + szIn) && !(*sc++));
        BYTE* sc1 = FindSC(sc, bufIn + szIn);
        size_t nalsz = sc1 - sc;
        auto nalStart = sc;
        while (nalsz)
        {
            auto szToSend = __min(nalsz, szOut);
            memcpy(&m_pTxBuf[rtpHeaderSize], nalStart, szToSend);
            SendPacket(m_pTxBuf.get(), szToSend + rtpHeaderSize, llSampleTime, sc1 == (bufIn + szIn));
            nalStart += szToSend;
            nalsz -= szToSend;
        }
        sc = sc1;
    }
}

void RTPVideoStreamSink::SendPacket(BYTE* pOut, size_t sz, LONGLONG ts, bool bLastNalOfFrame)
{
    // Prepare the 12 byte RTP header
    pOut[0] = (byte)0x80;                               // RTP version

    if (bLastNalOfFrame)
    {
        pOut[1] = (byte)(h264payloadType | 1 << 7);  //last packet of frame marker bit
    }
    else
    {
        pOut[1] = (byte)(h264payloadType);
    }
    pOut[2] = m_SequenceNumber >> 8;
    pOut[3] = m_SequenceNumber & 0x0FF;           // each packet is counted with a sequence counter
    m_SequenceNumber++;
    for (auto& ct : m_RtpStreamers)
    {
        auto ssrc = ct.second.m_ssrc;
        auto ts1 = ts;
        pOut[4] = (BYTE)((ts1 & 0xFF000000) >> 24);   // each image gets a timestamp
        pOut[5] = (BYTE)((ts1 & 0x00FF0000) >> 16);
        pOut[6] = (BYTE)((ts1 & 0x0000FF00) >> 8);
        pOut[7] = (BYTE)((ts1 & 0x000000FF));

        pOut[8] = (BYTE)((ssrc & 0xFF000000) >> 24);// 4 BYTE SSRC (sychronization source identifier)
        pOut[9] = (BYTE)((ssrc & 0x00FF0000) >> 16);
        pOut[10] = (BYTE)((ssrc & 0x0000FF00) >> 8);
        pOut[11] = (BYTE)((ssrc & 0x000000FF));

        ct.second.SendPacket(pOut, sz);
    }
}

void RTPVideoStreamSink::PacketizeMode1(BYTE* bufIn, size_t szIn, LONGLONG llSampleTime)
{
    BYTE* sc = FindSC(bufIn, bufIn + szIn);
    BYTE* pOut = m_pTxBuf.get();
    size_t szOut = m_MTUSize;
    BYTE* pOutEnd = pOut + szOut;
    BYTE* sc1 = sc;
    auto pOutCurr = pOut + rtpHeaderSize + stapAtypeHeaderSize;
    uint32_t numNalsToSend = 0;
    while (sc < (bufIn + szIn))
    {
        auto sc0 = sc;
        while ((sc < bufIn + szIn) && !(*sc++));
        sc1 = FindSC(sc, bufIn + szIn);
        size_t nalsz = sc1 - sc;
        if (pOutCurr + nalsz + stapASzHeaderSize > pOutEnd)
        {
            if (numNalsToSend == 0)
            {
                // STAP-A FU
                pOut[rtpHeaderSize] = 28; // STAP-A FU type
                pOut[rtpHeaderSize] |= (sc[0] & 0x60);
                pOut[rtpHeaderSize + 1] = sc[0] & 0x1F;
                pOut[rtpHeaderSize + 1] |= (1 << 7);
                sc++; nalsz--;
                auto maxSz = szOut - (rtpHeaderSize + stapAtypeHeaderSize + 1);
                while (nalsz > maxSz)
                {
                    memcpy(&pOut[rtpHeaderSize + stapAtypeHeaderSize + 1], sc, maxSz);
                    SendPacket(pOut, szOut, llSampleTime, false);
                    nalsz -= maxSz;
                    sc += maxSz;
                    pOut[rtpHeaderSize + 1] &= (~(1 << 7));
                }

                memcpy(&pOut[rtpHeaderSize + stapAtypeHeaderSize + 1], sc, nalsz);
                pOut[rtpHeaderSize + 1] |= (1 << 6);
                SendPacket(pOut, nalsz + rtpHeaderSize + stapAtypeHeaderSize + 1, llSampleTime, (sc1 == (bufIn + szIn)));
                sc += nalsz;
                sc = sc1;
            }
            //else if (numNalsToSend == 1)
            //{
            //    //if only this one nal fit into this packet, send as single non-stap nal
            //    //PacketizeMode0(sc0, sc1 - sc0, llSampleTime);
            //    
            //}
            else
            {
                pOut[rtpHeaderSize] = 24; //STAP-A type
                SendPacket(pOut, pOutCurr - pOut, llSampleTime, (sc1 == (bufIn + szIn)));
                sc = sc0;
            }
            numNalsToSend = 0;
            pOutCurr = pOut + rtpHeaderSize + stapAtypeHeaderSize;
        }
        else
        {
            pOutCurr[0] = (BYTE)((nalsz & 0xFF00) >> 8);
            pOutCurr[1] = (BYTE)(nalsz & 0x00FF);
            memcpy(&pOutCurr[stapASzHeaderSize], sc, nalsz);
            pOutCurr += (nalsz + stapASzHeaderSize);
            numNalsToSend++;
            sc = sc1;
        }

    }

    if (numNalsToSend)
    {
        pOut[rtpHeaderSize] = 24;
        SendPacket(pOut, pOutCurr - pOut, llSampleTime, true);
    }

}

STDMETHODIMP RTPVideoStreamSink::AddTransportHandler(winrt::delegate<BYTE*, size_t> packetHandler, winrt::hstring protocol /*= L"rtp"*/, uint32_t ssrc /*= 0*/)
{
    HRESULT_EXCEPTION_BOUNDARY_START;
    auto lock = std::lock_guard(m_guardlock);
    std::string destination = std::to_string((intptr_t)packetHandler.as<IUnknown>().get());
    m_RtpStreamers.insert({ destination, TxContext(destination,packetHandler) });
    m_RtpStreamers.at(destination).m_ssrc = ssrc;
    HRESULT_EXCEPTION_BOUNDARY_END;
}

STDMETHODIMP RTPVideoStreamSink::AddNetworkClient(winrt::hstring destination, winrt::hstring protocol /*= L"rtp"*/, uint32_t ssrc /*= 0*/)
{
    HRESULT_EXCEPTION_BOUNDARY_START;
    auto lock = std::lock_guard(m_guardlock);
    auto dest = winrt::to_string(destination);
    m_RtpStreamers.insert({ dest, TxContext(dest) });
    m_RtpStreamers.at(dest).m_ssrc = ssrc;
    HRESULT_EXCEPTION_BOUNDARY_END;
}

STDMETHODIMP RTPVideoStreamSink::RemoveNetworkClient(winrt::hstring destination)
{
    HRESULT_EXCEPTION_BOUNDARY_START;
    auto lock = std::lock_guard(m_guardlock);
    auto dest = winrt::to_string(destination);
    m_RtpStreamers.erase(dest);
    HRESULT_EXCEPTION_BOUNDARY_END;
}

STDMETHODIMP RTPVideoStreamSink::RemoveTransportHandler(winrt::delegate<BYTE*, size_t> packetHandler)
{
    HRESULT_EXCEPTION_BOUNDARY_START;
    auto lock = std::lock_guard(m_guardlock);
    std::string destination = std::to_string((intptr_t)packetHandler.as<IUnknown>().get());
    m_RtpStreamers.erase(destination);
    HRESULT_EXCEPTION_BOUNDARY_END;
}

STDMETHODIMP RTPVideoStreamSink::GenerateSDP(uint8_t* buf, size_t maxSize, winrt::hstring dest)
{
    HRESULT_EXCEPTION_BOUNDARY_START;
    std::string paramSets;
    std::string profileIdc;
    auto destination = winrt::to_string(dest);
    if (m_pVideoHeader)
    {
        auto vend = m_pVideoHeader + m_VideoHeaderSize;
        auto sc = FindSC(m_pVideoHeader, vend);
        while (sc < vend)
        {
            while ((sc < vend) && !(*sc++));
            BYTE* sc1 = FindSC(sc, vend);
            auto nalsz = sc1 - sc;

            winrt::Windows::Storage::Streams::Buffer p((uint32_t)nalsz);
            if (paramSets.empty())
            {
                //first set is sps
                auto pt = p.data();
                pt[0] = sc[1];
                pt[1] = sc[2];
                pt[2] = sc[3];
                p.Length(3);
                profileIdc = winrt::to_string(winrt::Windows::Security::Cryptography::CryptographicBuffer::EncodeToHexString(p));
            }
            memcpy_s(p.data(), p.Capacity(), sc, nalsz);
            p.Length((uint32_t)nalsz);
            auto paramSet = winrt::Windows::Security::Cryptography::CryptographicBuffer::EncodeToBase64String(p);
            paramSets += winrt::to_string(paramSet) + ",";
            sc = sc1;
        }
    }
    if (!paramSets.empty())
    {
        //remove last comma
        paramSets.pop_back();
    }
    auto sep = destination.find(":");
    auto destIP = destination.substr(0, sep);
    auto destPort = destination.substr(sep + 1, destination.find("?") - sep);
    std::string sdp =
        "v=0\n"
        "o=- " + std::to_string(rand()) + " 0 IN IP4 127.0.0.1\n" //destination
        "s=MSFT VideoStreamer\n"
        "c=IN IP4 " + destIP + "\n" //source
        "t=0 0\n"
        "m=video " + destPort + " RTP/AVP " + std::to_string(h264payloadType) + "\n"
        "a=ts-refclk:ntp=time.windows.com\n"
        "a=rtpmap:" + std::to_string(h264payloadType) + " H264/90000\n"
        "a=fmtp:" + std::to_string(h264payloadType) + " packetization-mode=" + std::to_string(m_packetizationMode);
    if (!paramSets.empty())
    {
        sdp = sdp
            + "; sprop-parameter-sets="
            + paramSets + "; profile-level-id=" + profileIdc + "\n";
    }
    memcpy_s(buf, maxSize, sdp.c_str(), sdp.size());
    buf[sdp.size()] = 0;
    HRESULT_EXCEPTION_BOUNDARY_END;
}

//IMFSampleGrabberCallback
STDMETHODIMP RTPVideoStreamSink::PacketizeAndSend(IMFSample* pSample)
{
    auto lock = std::lock_guard(m_guardlock);
    winrt::com_ptr<IMFMediaBuffer> spMediaBuf;
    LONGLONG llSampleTime, llSampleDur;
    DWORD dwSampleSize, maxLen;
    BYTE* pSampleBuffer = nullptr;
    HRESULT hr = S_OK;
    try {

        winrt::check_hresult(pSample->GetBufferByIndex(0, spMediaBuf.put()));
        winrt::check_hresult(spMediaBuf->Lock(&pSampleBuffer, &maxLen, &dwSampleSize));
        winrt::check_hresult(pSample->GetSampleTime(&llSampleTime));
        winrt::check_hresult(pSample->GetSampleDuration(&llSampleDur));
        MFTIME latency = MFGetSystemTime() - llSampleTime;
        //std::cout << "\nlatency = " << latency / 10000;
        auto ts = ((llSampleTime) * 90) / 10000;
        if (m_packetizationMode == 1)
        {
            PacketizeMode1(pSampleBuffer, dwSampleSize, ts);
        }
        else
        {
            PacketizeMode0(pSampleBuffer, dwSampleSize, ts);
        }
    }
    catch (winrt::hresult_error const& ex)
    {
        hr = ex.code();
    }

    if (spMediaBuf && pSampleBuffer)
    {
        spMediaBuf->Unlock();
    }
    return hr;
}

INetworkMediaStreamSink* RTPVideoStreamSink::CreateInstance(IMFMediaType* pMediaType, IMFMediaSink* pParent, DWORD dwStreamID)
{
    winrt::com_ptr<RTPVideoStreamSink> pVS;
    pVS.attach(new RTPVideoStreamSink(pMediaType, pParent, dwStreamID));
    return pVS.as<INetworkMediaStreamSink>().detach();
}

