//
// Copyright (C) Microsoft Corporation. All rights reserved.
//
#pragma once
#include <unknwn.h>
#include <windows.h>
#include <propvarutil.h>
#include <devpropdef.h>
#include "devpkey.h"
#include "cfgmgr32.h"

#include <ole2.h>  // include unknown.h this must come before winrt header
#include <initguid.h>
#include <Ks.h>
#include <ksproxy.h>
#include <ksmedia.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfobjects.h>
#include <mferror.h>
#include <mfreadwrite.h>
#include <nserror.h>
#include <winmeta.h>
#include <d3d9types.h>

#include <mfvirtualcamera.h>

#define RESULT_DIAGNOSTICS_LEVEL 4 // include function name

#include <wil\cppwinrt.h> // must be before the first C++ WinRT header, ref:https://github.com/Microsoft/wil/wiki/Error-handling-helpers
#include <wil\result.h>
#include <wil\com.h>

#include "EventHandler.h"
#include "SimpleFrameGenerator.h"
#include "SimpleMediaSource.h"
#include "SimpleMediaStream.h"
#include "HWMediaSource.h"
#include "HWMediaStream.h"
#include "AugmentedMediaSource.h"
#include "AugmentedMediaStream.h"
#include "VirtualCameraMediaSource.h"
#include "VirtualCameraMediaSourceActivate.h"

#include "winrt\Windows.ApplicationModel.h"

#pragma comment(lib, "mfuuid")
#pragma comment(lib, "mf")
#pragma comment(lib, "windowsapp")
#pragma comment(lib, "mfplat")
#pragma comment(lib, "Mfsensorgroup")

inline void DebugPrint(LPCWSTR szFormat, ...)
{
    WCHAR szBuffer[MAX_PATH] = { 0 };

    va_list pArgs;
    va_start(pArgs, szFormat);
    StringCbVPrintf(szBuffer, sizeof(szBuffer), szFormat, pArgs);
    va_end(pArgs);
    OutputDebugStringW(szBuffer);
}

#define DEBUG_MSG(msg,...) \
{\
    DebugPrint(L"[%s@%d] ", TEXT(__FUNCTION__), __LINE__);\
    DebugPrint(msg, __VA_ARGS__);\
    DebugPrint(L"\n");\
}\

namespace wilEx
{
    //template <typename T>
    //using make_unique_cotaskmem_array = unique_any_array_ptr<typename details::element_traits<T>::type>;

    template<typename T>
    wil::unique_cotaskmem_array_ptr<T> make_unique_cotaskmem_array(size_t numOfElements)
    {
        wil::unique_cotaskmem_array_ptr<T> arr;
        size_t cb = sizeof(wil::details::element_traits<T>::type) * numOfElements;
        void* ptr = ::CoTaskMemAlloc(cb);
        if (ptr != nullptr)
        {
            ZeroMemory(ptr, cb);
            arr.reset(reinterpret_cast<typename wil::details::element_traits<T>::type*>(ptr), numOfElements);
        }
        return arr;
    }
};

namespace winrt
{
    template<> bool is_guid_of<IMFMediaSourceEx>(guid const& id) noexcept;

    template<> bool is_guid_of<IMFMediaStream2>(guid const& id) noexcept;

    template<> bool is_guid_of<IMFActivate>(guid const& id) noexcept;
};

#define CHECKHR_GOTO( _hr, _lbl ) { hr = _hr; if( FAILED( hr ) ){ DEBUG_MSG(L"hr=0x%08x", _hr); goto _lbl; } }
#define CHECKNULL_GOTO( _ptr, _hr, _lbl ) { if(_ptr == nullptr) {hr = _hr; if( FAILED( hr ) ){ DEBUG_MSG(L"hr=0x%08x", _hr); goto _lbl; } } }