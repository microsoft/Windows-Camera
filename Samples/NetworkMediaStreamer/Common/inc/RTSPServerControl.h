// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for more information

#pragma once

/* Note: Application must include following files in this exact order before including this file

#include <windows.h>
#include <wincrypt.h>
#include <windows.foundation.h>
#include <windows.foundation.collections.h>
#include <winrt\base.h>
#include <winrt\Windows.Media.h>
#include <winrt\Windows.Foundation.h>
*/

#if (defined RTSPSERVER_EXPORTS)
#define RTSPSERVER_API __declspec(dllexport)
#else
#define RTSPSERVER_API __declspec(dllimport)
#endif

enum class SessionStatus : int32_t
{
    SessionStarted = 0,
    SessionSetupDone,
    SessionPlaying,
    SessionPaused,
    SessionEnded
};
template <> struct winrt::impl::category<SessionStatus> { using type = winrt::impl::enum_category; };
template <> struct winrt::impl::name<SessionStatus> { static constexpr auto& value{ L"SessionStatus" }; };

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

//EXTERN_C const IID IID_IRTSPAuthProvider;
MIDL_INTERFACE("BC710897-4727-4154-B085-52C5F5A4047C")
IRTSPAuthProvider : public ::IUnknown
{
    virtual STDMETHODIMP GetNewAuthSessionMessage(HSTRING* pAuthSessionMessage) = 0;
    virtual STDMETHODIMP Authorize(LPCWSTR pAuthResp, LPCWSTR pAuthSesMsg, LPCWSTR pMethod) = 0;
};

//EXTERN_C const IID IID_IRTSPAuthProviderCredStore;
MIDL_INTERFACE("E155C9EF-66BF-48E8-BBF7-888C914AB453")
IRTSPAuthProviderCredStore : public ::IUnknown
{
    virtual STDMETHODIMP AddUser(LPCWSTR pUserName, LPCWSTR pPassword) = 0;
    virtual STDMETHODIMP RemoveUser(LPCWSTR pUserName) = 0;
};

namespace ABI
{
    using namespace ABI::Windows::Foundation;
    template<> MIDL_INTERFACE("022C6CB9-64D5-472F-8753-76382CC5F4DE") ITypedEventHandler< uintptr_t, SessionStatus> : ITypedEventHandler_impl<uintptr_t, SessionStatus>{};
    typedef ITypedEventHandler<uintptr_t, SessionStatus> SessionStatusHandler;

    template<> MIDL_INTERFACE("022C6CB9-64D5-472F-8753-76382CC5F4DF") ITypedEventHandler<HRESULT, HSTRING> : ITypedEventHandler_impl<HRESULT, HSTRING>  { };
    typedef ITypedEventHandler<HRESULT, HSTRING> LogHandler;

    typedef Collections::IPropertySet RTSPSuffixSinkMap;
}
namespace winrt
{
    typedef winrt::Windows::Foundation::TypedEventHandler<uintptr_t, SessionStatus> SessionStatusHandler;
    template <> struct winrt::impl::guid_storage<SessionStatusHandler>
    {
        static constexpr guid value{ __uuidof(ABI::SessionStatusHandler) };
    };
    typedef winrt::Windows::Foundation::TypedEventHandler <winrt::hresult, winrt::hstring> LogHandler;
    template <> struct winrt::impl::guid_storage<LogHandler>
    {
        static constexpr guid value{ __uuidof(ABI::LogHandler) };
    };

    typedef winrt::Windows::Foundation::Collections::PropertySet RTSPSuffixSinkMap;
}

//EXTERN_C const IID IID_IRTSPServerControl;
MIDL_INTERFACE("2E8A2DA6-2FB9-43A8-A7D6-FB4085DE67B0")
IRTSPServerControl : public ::IUnknown
{
public:
    virtual STDMETHODIMP StartServer() = 0;
    virtual STDMETHODIMP StopServer() = 0;
    virtual STDMETHODIMP AddLogHandler(LoggerType type, ABI::LogHandler* pHandler, EventRegistrationToken* pToken) = 0;
    virtual STDMETHODIMP RemoveLogHandler(LoggerType type, EventRegistrationToken token) = 0;
    virtual STDMETHODIMP AddSessionStatusHandler(LoggerType type, ABI::SessionStatusHandler* pHandler, EventRegistrationToken* pToken) = 0;
    virtual STDMETHODIMP RemoveSessionStatusHandler(LoggerType type, EventRegistrationToken token) = 0;
};

RTSPSERVER_API STDMETHODIMP CreateRTSPServer(ABI::RTSPSuffixSinkMap *pStreamers, uint16_t socketPort, bool bSecure, IRTSPAuthProvider* pAuthProvider, PCCERT_CONTEXT* aServerCerts, size_t uCertCount, IRTSPServerControl** ppRTSPServerControl);
RTSPSERVER_API STDMETHODIMP GetAuthProviderInstance(AuthType authType, LPCWSTR pResourceName, IRTSPAuthProvider** ppRTSPAuthProvider);
