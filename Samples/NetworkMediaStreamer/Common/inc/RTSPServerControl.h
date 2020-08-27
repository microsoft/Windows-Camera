// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once
#include <windows.h>
#include <wincrypt.h>
#include <winrt\base.h>
#include <winrt\Windows.Media.h>


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

enum class AuthType
{
    Basic,
    Digest,
    Both
};
typedef winrt::Windows::Foundation::Collections::IMap<winrt::hstring, winrt::Windows::Media::IMediaExtension> RTSPSuffixSinkMap;
typedef winrt::Windows::Foundation::Collections::IMapView<winrt::hstring, winrt::Windows::Media::IMediaExtension> RTSPSuffixSinkMapView;

//EXTERN_C const IID IID_IRTSPAuthProvider;
MIDL_INTERFACE("BC710897-4727-4154-B085-52C5F5A4047C")
IRTSPAuthProvider : public ::IUnknown
{
    virtual STDMETHODIMP GetNewAuthSessionMessage(winrt::hstring& authSessionMessage) = 0;
    virtual STDMETHODIMP Authorize(winrt::hstring authResponse, winrt::hstring authSessionMessage, winrt::hstring method) = 0;
};

//EXTERN_C const IID IID_IRTSPAuthProviderCredStore;
MIDL_INTERFACE("E155C9EF-66BF-48E8-BBF7-888C914AB453")
IRTSPAuthProviderCredStore : public ::IUnknown
{
    virtual STDMETHODIMP AddUser(winrt::hstring userName, winrt::hstring password) = 0;
    virtual STDMETHODIMP RemoveUser(winrt::hstring userName) = 0;
};

//EXTERN_C const IID IID_IRTSPServerControl;
MIDL_INTERFACE("2E8A2DA6-2FB9-43A8-A7D6-FB4085DE67B0")
IRTSPServerControl : public ::IUnknown
{
public:
    virtual STDMETHODIMP StartServer() = 0;
    virtual STDMETHODIMP StopServer() = 0;
    virtual STDMETHODIMP AddLogHandler(LoggerType type, winrt::delegate <winrt::hresult, winrt::hstring> handler, winrt::event_token & token) = 0;
    virtual STDMETHODIMP RemoveLogHandler(LoggerType type, winrt::event_token token) = 0;
    virtual STDMETHODIMP AddSessionStatusHandler(LoggerType type, winrt::delegate<uint64_t, SessionStatus> handler, winrt::event_token & token) = 0;
    virtual STDMETHODIMP RemoveSessionStatusHandler(LoggerType type, winrt::event_token token) = 0;
};

RTSPSERVER_API STDMETHODIMP CreateRTSPServer(RTSPSuffixSinkMapView streamers, uint16_t socketPort, bool bSecure, IRTSPAuthProvider* pAuthProvider, winrt::array_view<PCCERT_CONTEXT> serverCerts, IRTSPServerControl** ppRTSPServerControl);
RTSPSERVER_API STDMETHODIMP GetAuthProviderInstance(AuthType authType, winrt::hstring resourceName, IRTSPAuthProvider** ppRTSPAuthProvider);
