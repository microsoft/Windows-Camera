// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#define SECURITY_WIN32
#include <WinSock2.h>
#include <windows.h>
#include <iostream>
#include <mutex>

#include <Security.h>
#include <winternl.h>
#define SCHANNEL_USE_BLACKLISTS
#include <schnlsp.h>

#include <ws2def.h>
#include <mstcpip.h>
#include <ws2tcpip.h>
#include <mfidl.h>

#include <winrt\base.h>
#include <Shlwapi.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/windows.system.threading.h>
#include <winrt\Windows.Security.Cryptography.h>
#include <winrt\Windows.Security.Cryptography.Core.h>
#include <winrt\Windows.Security.Credentials.h>

#define HRESULT_EXCEPTION_BOUNDARY_START HRESULT hr = S_OK; try {
#define HRESULT_EXCEPTION_BOUNDARY_END }catch(...) { hr = winrt::to_hresult();} return hr;
