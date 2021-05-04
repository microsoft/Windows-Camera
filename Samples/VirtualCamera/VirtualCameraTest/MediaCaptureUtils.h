//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#pragma once
#ifndef MEDIACAPTUREUTILS_H
#define MEDIACAPTUREUTILS_H

#ifndef  E_TIMEOUT
#define E_TIMEOUT HRESULT_FROM_WIN32( ERROR_TIMEOUT )
#endif

//
// wil macro extension
//
#define TRY_RETURN_CAUGHT_EXCEPTION_MSG(exp, fmt, ...) try { exp; } CATCH_RETURN_MSG(fmt, __VA_ARGS__) 
#define TRY_RETURN_CAUGHT_EXCEPTION(exp)               try { exp; } CATCH_RETURN() 

#define __RETURN_T_FAIL(ret, hr, str)  __WI_SUPPRESS_4127_S do { const HRESULT __hr = (hr); __R_FN(Return_Hr)(__R_INFO(str) __hr); return ret; } __WI_SUPPRESS_4127_E while ((void)0, 0)
#define RETURN_T_IF_FAILED(ret, hr)    __WI_SUPPRESS_4127_S do { const auto __hrRet = wil::verify_hresult(hr); if (FAILED(__hrRet)) { __RETURN_T_FAIL(ret, __hrRet, #hr); }} __WI_SUPPRESS_4127_E while ((void)0, 0)

#define RETURN_IF_UNEXPECTED_HR(hresult, hrExpected) \
{ \
    const HRESULT __hresult = (hresult); \
    HRESULT __retHr = IsSkip(__hresult) ? E_SKIP : E_TEST_FAILED; \
    RETURN_HR_IF_MSG(__retHr, (__hresult != hrExpected), "hr =0x%.8X (Expected: 0x%.8X)", __hresult, hrExpected); \
    LOG_COMMENT(L" return expected result. hr=0x%.8X", __hresult ); \
}\

#define CATCH_LOG_RETURN_T(ret)                   catch (...) { __pragma(warning(suppress : 4297)); LOG_CAUGHT_EXCEPTION(); return ret; } 
#define CATCH_LOG_RETURN_MSG_T(ret, fmt, ...)     catch (...) { __pragma(warning(suppress : 4297)); LOG_CAUGHT_EXCEPTION_MSG(fmt, ##__VA_ARGS__); return ret; } 

#define TRY_RETURN_CAUGHT_EXCEPTION_T(ret, exp) \
    try { exp; } CATCH_LOG_RETURN_T(ret) \

#define TRY_RETURN_CAUGHT_EXCEPTION_MSG_T(ret, exp, fmt, ...) \
    try { exp; } CATCH_LOG_RETURN_MSG_T(ret, fmt, ##__VA_ARGS__ ) \

class MediaCaptureTaskHelper
{
public:
    static winrt::Windows::Foundation::IAsyncAction RunTaskWithTimeout(winrt::Windows::Foundation::IAsyncAction op, unsigned int timeoutMs = 60000 /* 60s*/)
    {
        return RunTaskWithTimeout_base(op, timeoutMs);
    };

    template<typename T>
    static winrt::Windows::Foundation::IAsyncOperation<T> RunTaskWithTimeout(winrt::Windows::Foundation::IAsyncOperation<T>  op, unsigned int timeoutMs = 60000 /* 60s*/)
    {
        return RunTaskWithTimeout_base(op, timeoutMs);
    };

    template <typename T, typename U>
    static winrt::Windows::Foundation::IAsyncOperationWithProgress<T, U> RunTaskWithTimeout(winrt::Windows::Foundation::IAsyncOperationWithProgress<T, U>  op, unsigned int timeoutMs = 60000 /* 60s*/)
    {
        return RunTaskWithTimeout_base(op, timeoutMs);
    }

private:
    template <typename I, std::enable_if_t<!std::is_base_of_v<winrt::Windows::Foundation::IAsyncInfo, I>, int> = 0>
    static I RunTaskWithTimeout_base(I op, unsigned int timeoutMs = 60000 /* 60s*/)
    {
        
        unsigned int _timeoutMs = IsDebuggerPresent() ? UINT_MAX - 1 : timeoutMs;

        switch (op.wait_for(std::chrono::milliseconds(_timeoutMs)))
        {
        case winrt::Windows::Foundation::AsyncStatus::Completed:
        case winrt::Windows::Foundation::AsyncStatus::Error:
            return op;
            break;
        case winrt::Windows::Foundation::AsyncStatus::Canceled:
        case winrt::Windows::Foundation::AsyncStatus::Started:
            op.Cancel();

            LOG_WARNING(L"Cancel task timeout: %d ms", timeoutMs);

            winrt::throw_hresult(E_TIMEOUT);
            break;
        }
        return op;
    }
};

namespace winrt
{
    extern inline winrt::hstring StringFormat(const wchar_t* format, ...)
    {
        wchar_t str[MAX_PATH];
        va_list argList;
        va_start(argList, format);
        if (SUCCEEDED(vswprintf(str, MAX_PATH, format, argList)))
        {
            return winrt::hstring(str);
        }
        else
        {
            return L"";
        }
    };
}

class CMediaCaptureUtils
{
public:
    CMediaCaptureUtils();
    ~CMediaCaptureUtils();

    static HRESULT GetDeviceList(winrt::hstring const& strDeviceSelector, DeviceInformationCollection& devList);
    static HRESULT GetDeviceInfo(winrt::hstring const& strDevSymLink, DeviceInformation& devInfo);

    template<typename T>
    static HRESULT GetDeviceProperty(winrt::hstring const& strDevSymLink, winrt::hstring const& devPropKey, DeviceInformationKind const& kind, winrt::Windows::Foundation::IReference<T>& value)
    {
        winrt::Windows::Foundation::Collections::IVector<winrt::hstring> requestedProps{ winrt::single_threaded_vector<winrt::hstring>() };;
        requestedProps.Append(devPropKey);

        DeviceInformation devObj{ nullptr };
        TRY_RETURN_CAUGHT_EXCEPTION_MSG(devObj = MediaCaptureTaskHelper::RunTaskWithTimeout(DeviceInformation::CreateFromIdAsync(strDevSymLink, requestedProps, kind)).get(),
            "Fail to get device property");

        auto obj = devObj.Properties().TryLookup(devPropKey);
        if (obj != nullptr)
        {
            value = obj.try_as<winrt::Windows::Foundation::IReference<T>>();
           
            if(!value)
            {
                return HRESULT_FROM_WIN32(ERROR_DATATYPE_MISMATCH);
            }
        }
        else
        {
            return HRESULT_FROM_WIN32(ERROR_NOT_FOUND);
        }
        return S_OK;
    };
};

#endif