// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once
#include <windows.h>
#include <wincrypt.h>
#include <mfidl.h>
#include <mfapi.h>
#include <windows.media.h>
#include <winrt\base.h>
using ::IUnknown;
#if (defined VIDEOSTREAMER_EXPORTS) || (defined VideoStreamer_EXPORTS)
#define VIDEOSTREAMER_API __declspec(dllexport)
#else
#define VIDEOSTREAMER_API __declspec(dllimport)
#endif

//EXTERN_C const IID IID_IVideoStreamer;
MIDL_INTERFACE("022C6CB9-64D5-472F-8753-76382CC5F4DA")
INetworkMediaStreamSink : public IMFStreamSink
{
public:
    virtual void AddTransportHandler(winrt::delegate<BYTE*, size_t> packethandler, std::string protocol = "rtp", uint32_t ssrc = 0) = 0;
    virtual void AddNetworkClient(std::string destination, std::string protocol = "rtp", uint32_t ssrc = 0) = 0;
    virtual void RemoveNetworkClient(std::string destination) = 0;
    virtual void RemoveTransportHandler(winrt::delegate<BYTE*, size_t> packetHandler) = 0;
    virtual void GenerateSDP(char* buf, size_t maxSize, std::string destination) = 0;
    virtual STDMETHODIMP Start(MFTIME hnsSystemTime, LONGLONG llClockStartOffset) = 0;
    virtual STDMETHODIMP Stop(MFTIME hnsSystemTime) = 0;
    virtual STDMETHODIMP Pause(MFTIME hnsSystemTime) = 0;
    virtual STDMETHODIMP Shutdown() = 0;

};
