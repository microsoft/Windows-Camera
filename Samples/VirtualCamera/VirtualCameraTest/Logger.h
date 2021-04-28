//
// Copyright (C) Microsoft Corporation. All rights reserved.
//
#pragma once
#ifndef LOGGER_H
#define LOGGER_H

#include "windows.h"
#include "strsafe.h"

enum class LogLevel
{
    Info = 15,
    Warn = 33,
    Error = 31,
    Success = 32
};

inline int EnableVTMode()
{
    // Set output mode to handle virtual terminal sequences
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE)
    {
        return GetLastError();
    }

    DWORD dwMode = 0;
    if (!GetConsoleMode(hOut, &dwMode))
    {
        return GetLastError();
    }

    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(hOut, dwMode))
    {
        return GetLastError();
    }
    return 0;
}

#ifdef GTEST
#define GTEST_LOG_INFO    L"[   INFO   ]"
#define GTEST_LOG_WARNING L"[  WARNING ]"
#define GTEST_LOG_ERROR   L"[   ERROR  ]"
#define GTEST_LOG_SUCCESS L"[  SUCCESS ]"
#endif

inline void LOG(LogLevel flag, const wchar_t* fmt, ...)
{
#ifdef GTEST
    switch (flag)
    {
    case LogLevel::Info:
        wprintf(L"\x1b[1;%dm%s ", flag, GTEST_LOG_INFO);
        break;
    case LogLevel::Warn:
        wprintf(L"\x1b[1;%dm%s ", flag, GTEST_LOG_WARNING);
        break;
    case LogLevel::Error:
        wprintf(L"\x1b[1;%dm%s ", flag, GTEST_LOG_ERROR);
        break;
    case LogLevel::Success:
        wprintf(L"\x1b[1;%dm%s ", flag, GTEST_LOG_SUCCESS);
        break;
    }
#else
    wprintf(L"\x1b[%dm ", flag);
#endif
    va_list argList;
    va_start(argList, fmt);
    wchar_t message[2048];
    StringCchVPrintfW(message, ARRAYSIZE(message), fmt, argList);
    wprintf(message);
    wprintf(L"\n");
    va_end(argList);

    wprintf(L"\x1b[0m");
};

static constexpr HRESULT E_TEST_FAILED = 0xA0000002;

#define LOG_COMMENT(fmt, ...)  { LOG(LogLevel::Info, fmt, __VA_ARGS__);  }
#define LOG_WARNING(fmt, ...)     { LOG(LogLevel::Warn, fmt, __VA_ARGS__);  }
#ifdef GTEST
#define LOG_ERROR(fmt, ...)    { ADD_FAILURE(); LOG(LogLevel::Error, fmt, __VA_ARGS__); }
#define LOG_ERROR_RETURN(hr, fmt, ...)    {  const HRESULT __hresult = (hr); ADD_FAILURE(); LOG(LogLevel::Error, fmt, __VA_ARGS__); return __hresult; }
#else
#define LOG_ERROR(fmt, ...)    { LOG(LogLevel::Error, fmt, __VA_ARGS__); }
#define LOG_ERROR_RETURN(hr, fmt, ...)  {  const HRESULT __hresult = (hr); LOG(LogLevel::Error, fmt, __VA_ARGS__); return __hresult; }
#endif
#define LOG_SUCCESS(fmt, ...)  { LOG(LogLevel::Success, fmt, __VA_ARGS__); }

#endif
