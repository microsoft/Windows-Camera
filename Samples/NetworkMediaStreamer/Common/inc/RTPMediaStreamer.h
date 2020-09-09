// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

/* Must include following before this file
#include "NetworkMediaStreamer.h"
*/

#if (defined RTPMEDIASTREAMER_EXPORTS)
#define RTPMEDIASTREAMER_API __declspec(dllexport)
#else
#define RTPMEDIASTREAMER_API __declspec(dllimport)
#endif

RTPMEDIASTREAMER_API STDMETHODIMP CreateRTPMediaSink(IMFMediaType** apMediaTypes, DWORD dwMediaTypeCount, IMFMediaSink** ppMediaSink);