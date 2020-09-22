// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for more information

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
    virtual STDMETHODIMP AddTransportHandler(ABI::PacketHandler * pPackethandler, LPCWSTR pProtocol = L"rtp", LPCWSTR pParam = L"") = 0;
    virtual STDMETHODIMP RemoveTransportHandler(ABI::PacketHandler* pPacketHandler) = 0;
    virtual STDMETHODIMP AddNetworkClient(LPCWSTR pDestination, LPCWSTR pProtocol = L"rtp", LPCWSTR pParams = L"") = 0;
    virtual STDMETHODIMP RemoveNetworkClient(LPCWSTR pDestination) = 0;
    virtual STDMETHODIMP GenerateSDP(uint8_t* pBuf, size_t maxSize, LPCWSTR pDestination) = 0;
    virtual STDMETHODIMP Start(MFTIME hnsSystemTime, LONGLONG llClockStartOffset) = 0;
    virtual STDMETHODIMP Stop(MFTIME hnsSystemTime) = 0;
    virtual STDMETHODIMP Pause(MFTIME hnsSystemTime) = 0;
    virtual STDMETHODIMP Shutdown() = 0;

};
