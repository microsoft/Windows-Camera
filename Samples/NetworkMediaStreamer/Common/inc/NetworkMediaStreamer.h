// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

/* Note: Application must include following files in this exact order before including this file
#include <mfidl.h>
#include <winrt\base.h>
#include <windows.foundation.h>
#include <windows.Storage.streams.h>
#include <winrt\Windows.Foundation.h>
#include <winrt\Windows.storage.streams.h>
*/

#if (defined VIDEOSTREAMER_EXPORTS) || (defined VideoStreamer_EXPORTS)
#define VIDEOSTREAMER_API __declspec(dllexport)
#else
#define VIDEOSTREAMER_API __declspec(dllimport)
#endif

namespace ABI 
{
    using namespace ABI::Windows::Foundation;
    using namespace ABI::Windows::Storage::Streams;
    template<> MIDL_INTERFACE("022C6CB9-64D5-472F-8753-76382CC5F4DB") IEventHandler<IBuffer*> : IEventHandler_impl<IBuffer*>{};
    typedef IEventHandler<IBuffer*> PacketHandler;
}
namespace winrt {

    typedef winrt::Windows::Foundation::EventHandler<winrt::Windows::Storage::Streams::IBuffer> PacketHandler;
    template <> struct winrt::impl::guid_storage<PacketHandler>
    {
        static constexpr guid value{ __uuidof(ABI::PacketHandler) };
    };
}

//EXTERN_C const IID IID_IVideoStreamer;
MIDL_INTERFACE("022C6CB9-64D5-472F-8753-76382CC5F4DA")
INetworkMediaStreamSink : public IMFStreamSink
{
public:
    virtual STDMETHODIMP AddTransportHandler (ABI::PacketHandler* packethandler, LPCWSTR protocol = L"rtp", LPCWSTR param = L"") = 0;
    virtual STDMETHODIMP AddNetworkClient(LPCWSTR destination, LPCWSTR protocol = L"rtp", LPCWSTR params = L"") = 0;
    virtual STDMETHODIMP RemoveNetworkClient(LPCWSTR destination) = 0;
    virtual STDMETHODIMP RemoveTransportHandler(ABI::PacketHandler* packetHandler) = 0;
    virtual STDMETHODIMP GenerateSDP(uint8_t* buf, size_t maxSize, LPCWSTR destination) = 0;
    virtual STDMETHODIMP Start(MFTIME hnsSystemTime, LONGLONG llClockStartOffset) = 0;
    virtual STDMETHODIMP Stop(MFTIME hnsSystemTime) = 0;
    virtual STDMETHODIMP Pause(MFTIME hnsSystemTime) = 0;
    virtual STDMETHODIMP Shutdown() = 0;

};
