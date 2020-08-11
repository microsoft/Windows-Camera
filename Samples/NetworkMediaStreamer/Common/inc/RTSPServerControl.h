// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once
#include "NetworkMediaStreamer.h"

#if (defined RTSPSERVER_EXPORTS)
#define RTSPSERVER_API __declspec(dllexport)
#else
#define RTSPSERVER_API __declspec(dllimport)
#endif

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

RTSPSERVER_API IRTSPServerControl* CreateRTSPServer(std::map<std::string, winrt::com_ptr<IMFMediaSink>> streamers, uint16_t socketPort, bool bSecure, std::vector<PCCERT_CONTEXT> serverCerts = {});
