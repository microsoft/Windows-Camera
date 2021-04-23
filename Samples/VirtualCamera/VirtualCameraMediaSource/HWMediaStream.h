//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#ifndef  HWMEDIASTREAM_H
#define  HWMEDIASTREAM_H

#pragma once
#include "HWMediaSource.h"

namespace winrt::WindowsSample::implementation
{
    struct HWMediaStream : winrt::implements<HWMediaStream, IMFMediaStream2>
    {
        friend struct HWMediaSource;

    public:
        ~HWMediaStream();

        // IMFMediaEventGenerator
        IFACEMETHOD(BeginGetEvent)(IMFAsyncCallback* pCallback, IUnknown* punkState);
        IFACEMETHOD(EndGetEvent)(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent);
        IFACEMETHOD(GetEvent)(DWORD dwFlags, IMFMediaEvent** ppEvent);
        IFACEMETHOD(QueueEvent)(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT* pvValue);

        // IMFMediaStream
        IFACEMETHOD(GetMediaSource)(IMFMediaSource** ppMediaSource);
        IFACEMETHOD(GetStreamDescriptor)(IMFStreamDescriptor** ppStreamDescriptor);
        IFACEMETHOD(RequestSample)(IUnknown* pToken);

        // IMFMediaStream2
        IFACEMETHOD(SetStreamState)(MF_STREAM_STATE state);
        IFACEMETHOD(GetStreamState)(_Out_ MF_STREAM_STATE* pState);

        // Non-interface methods.
        HRESULT Initialize(_In_ HWMediaSource* pSource, _In_ IMFStreamDescriptor* pStreamDesc, _In_ DWORD dwWorkQueue);
        HRESULT Shutdown();
        DWORD StreamIdentifier() { return m_dwStreamIdentifier; };
        HRESULT SetMediaStream(_In_ IMFMediaStream* pMediaStream);

        HRESULT OnMediaStreamEvent(_In_ IMFAsyncResult* pResult);

    protected:
        HRESULT _CheckShutdownRequiresLock();

        wil::com_ptr_nothrow<CAsyncCallback<HWMediaStream>> m_xOnMediaStreamEvent;

        winrt::slim_mutex  m_Lock;

        wil::com_ptr_nothrow<IMFMediaSource> m_parent;
        wil::com_ptr_nothrow<IMFMediaStream> m_spDevStream;
        wil::com_ptr_nothrow<IMFStreamDescriptor> m_spStreamDesc;
        wil::com_ptr_nothrow<IMFMediaEventQueue> m_spEventQueue;

        bool m_isShutdown = false;
        DWORD m_dwStreamIdentifier = 0;
        DWORD m_dwSerialWorkQueueId = 0;
    };
}

#endif
