// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once
#include "guiddef.h"

namespace winrt::VirtualCameraManager_WinRT::implementation
{
    // Define virtual camera CLSID (refer to virtual camera dll, should match the CLSID of the )
    static const GUID CLSID_SIMPLEMEDIASOURCE_WIN32 = { 0x7b89b92e, 0xfe71, 0x42d0, {0x8a, 0x41, 0xe1, 0x37, 0xd0, 0x6e, 0xa1, 0x84 } };
    static const wchar_t* SIMPLEMEDIASOURCE_WIN32 = L"{7B89B92E-FE71-42D0-8A41-E137D06EA184}";
    static const wchar_t* SIMPLEMEDIASOURCE_WIN32_CLISD = L"7B89B92E-FE71-42D0-8A41-E137D06EA184";
    static const wchar_t* SIMPLEMEDIASOURCE_WIN32_FRIENDLYNAME = L"VirtualCameraMediaSource";
}
