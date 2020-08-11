// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include "NetworkMediaStreamer.h"

#if (defined RTPMEDIASTREAMER_EXPORTS)
#define RTPMEDIASTREAMER_API __declspec(dllexport)
#else
#define RTPMEDIASTREAMER_API __declspec(dllimport)
#endif

RTPMEDIASTREAMER_API IMFMediaSink* CreateRTPMediaSink(std::vector<IMFMediaType*> mediaTypes);