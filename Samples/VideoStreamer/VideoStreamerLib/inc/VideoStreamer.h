// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once
#include <windows.h>
#include <wincrypt.h>
#include <mfidl.h>
#include <mfapi.h>
#include <windows.media.h>
#include <winrt\base.h>
using ::IUnknown;
#if (defined VIDEOSTREAMER_EXPORTS) || (defined VideoStreamer_EXPORTS)
#define VIDEOSTREAMER_API __declspec(dllexport)
#else
#define VIDEOSTREAMER_API __declspec(dllimport)
#endif

//EXTERN_C const IID IID_IVideoStreamer;
MIDL_INTERFACE("022C6CB9-64D5-472F-8753-76382CC5F4DA")
INetworkMediaStreamSink : public IMFStreamSink
{
public:
    virtual void AddTransportHandler(winrt::delegate<BYTE*, size_t> packethandler, std::string protocol = "rtp", uint32_t ssrc = 0) = 0;
    virtual void AddNetworkClient(std::string destination, std::string protocol = "rtp", uint32_t ssrc = 0) = 0;
    virtual void RemoveNetworkClient(std::string destination) = 0;
    virtual void RemoveTransportHandler(winrt::delegate<BYTE*, size_t> packetHandler) = 0;
    virtual void GenerateSDP(char* buf, size_t maxSize, std::string destination) = 0;
    virtual STDMETHODIMP Start(MFTIME hnsSystemTime, LONGLONG llClockStartOffset) = 0;
    virtual STDMETHODIMP Stop(MFTIME hnsSystemTime) = 0;
    virtual STDMETHODIMP Pause(MFTIME hnsSystemTime) = 0;
    virtual STDMETHODIMP Shutdown() = 0;

};
enum class SessionStatus
{
    SessionStarted,
    SessionSetupDone,
    SessionPlaying,
    SessionPaused,
    SessionEnded
};

enum class LoggerType
{
    ERRORS = 0,
    WARNINGS,
    RTSPMSGS,
    OTHER,
    LOGGER_MAX
};

//EXTERN_C const IID IID_IRTSPServerControl;
MIDL_INTERFACE("2E8A2DA6-2FB9-43A8-A7D6-FB4085DE67B0")
IRTSPServerControl : public ::IUnknown
{
public:
    virtual void StartServer() = 0;
    virtual void StopServer() = 0;
    virtual winrt::event_token AddLogHandler(LoggerType type, winrt::delegate <winrt::hresult, winrt::hstring> handler) = 0;
    virtual void RemoveLogHandler(LoggerType type, winrt::event_token token) = 0;
    virtual winrt::event_token AddSessionStatusHandler(LoggerType type, winrt::delegate <uint64_t, SessionStatus> handler) = 0;
    virtual void RemoveSessionStatusHandler(LoggerType type, winrt::event_token token) = 0;
};

// factory methods
VIDEOSTREAMER_API IMFMediaSink* CreateRTPMediaSink(std::vector<IMFMediaType*> mediaTypes);
VIDEOSTREAMER_API IRTSPServerControl* CreateRTSPServer(std::map<std::string, winrt::com_ptr<IMFMediaSink>> streamers, uint16_t socketPort, bool bSecure, std::vector<PCCERT_CONTEXT> serverCerts = {});
