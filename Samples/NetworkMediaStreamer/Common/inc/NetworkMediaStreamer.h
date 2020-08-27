// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once
#include <mfidl.h>
#include <winrt\base.h>
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
    virtual STDMETHODIMP AddTransportHandler(winrt::delegate<BYTE*, size_t> packethandler, winrt::hstring protocol = L"rtp", uint32_t ssrc = 0) = 0;
    virtual STDMETHODIMP AddNetworkClient(winrt::hstring destination, winrt::hstring protocol = L"rtp", uint32_t ssrc = 0) = 0;
    virtual STDMETHODIMP RemoveNetworkClient(winrt::hstring destination) = 0;
    virtual STDMETHODIMP RemoveTransportHandler(winrt::delegate<BYTE*, size_t> packetHandler) = 0;
    virtual STDMETHODIMP GenerateSDP(uint8_t * buf, size_t maxSize, winrt::hstring destination) = 0;
    virtual STDMETHODIMP Start(MFTIME hnsSystemTime, LONGLONG llClockStartOffset) = 0;
    virtual STDMETHODIMP Stop(MFTIME hnsSystemTime) = 0;
    virtual STDMETHODIMP Pause(MFTIME hnsSystemTime) = 0;
    virtual STDMETHODIMP Shutdown() = 0;

};
