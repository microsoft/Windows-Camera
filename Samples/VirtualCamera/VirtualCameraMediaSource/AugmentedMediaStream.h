//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#ifndef  AUGMENTEDMEDIASTREAM_H
#define  AUGMENTEDMEDIASTREAM_H

#pragma once
#include "AugmentedMediaSource.h"

namespace winrt::WindowsSample::implementation
{
    struct AugmentedMediaStream : winrt::implements<AugmentedMediaStream, IMFMediaStream2>
    {
        friend struct AugmentedMediaSource;

    public:
        ~AugmentedMediaStream();

        // IMFMediaEventGenerator
        IFACEMETHODIMP BeginGetEvent(IMFAsyncCallback* pCallback, IUnknown* punkState) override;
        IFACEMETHODIMP EndGetEvent(IMFAsyncResult* pResult, IMFMediaEvent** ppEvent) override;
        IFACEMETHODIMP GetEvent(DWORD dwFlags, IMFMediaEvent** ppEvent) override;
        IFACEMETHODIMP QueueEvent(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, const PROPVARIANT* pvValue) override;

        // IMFMediaStream
        IFACEMETHODIMP GetMediaSource(IMFMediaSource** ppMediaSource) override;
        IFACEMETHODIMP GetStreamDescriptor(IMFStreamDescriptor** ppStreamDescriptor) override;
        IFACEMETHODIMP RequestSample(IUnknown* pToken) override;

        // IMFMediaStream2
        IFACEMETHODIMP SetStreamState(MF_STREAM_STATE state) override;
        IFACEMETHODIMP GetStreamState(_Out_ MF_STREAM_STATE* pState) override;

        // Non-interface methods.
        HRESULT Initialize(_In_ AugmentedMediaSource* pSource, _In_ DWORD dwStreamId, _In_ IMFStreamDescriptor* pStreamDesc, _In_ DWORD dwWorkQueue);
        HRESULT ProcessSample(_In_ IMFSample* inputSample);
        HRESULT Shutdown();
        DWORD StreamIdentifier() { return m_dwStreamId; };
        HRESULT SetMediaStream(_In_ IMFMediaStream* pMediaStream);

        void OnMediaStreamEvent(_In_ IMFAsyncResult* pResult);

        HRESULT Start();
        HRESULT Stop();

    protected:
        _Requires_lock_held_(m_Lock) HRESULT _CheckShutdownRequiresLock();
        _Requires_lock_held_(m_Lock) HRESULT _SetStreamAttributes(_In_ IMFAttributes* pAttributeStore);

        wil::com_ptr_nothrow<CAsyncCallback<AugmentedMediaStream>> m_xOnMediaStreamEvent;
        wil::unique_cotaskmem_array_ptr<wil::com_ptr_nothrow<IMFMediaType>> m_mediaTypeList;
        std::vector<wil::com_ptr_nothrow<IMFMediaType>> m_mediaTypeCache;

        winrt::slim_mutex  m_Lock;

        wil::com_ptr_nothrow<AugmentedMediaSource> m_parent;
        wil::com_ptr_nothrow<IMFMediaStream> m_spDevStream;
        wil::com_ptr_nothrow<IMFStreamDescriptor> m_spStreamDesc;
        wil::com_ptr_nothrow<IMFMediaEventQueue> m_spEventQueue;
        wil::com_ptr_nothrow<IMFAttributes> m_spAttributes;

        bool m_isShutdown = false;
        DWORD m_dwDevSourceStreamIdentifier = 0;
        DWORD m_dwSerialWorkQueueId = 0;
        DWORD m_dwStreamId = 0;
        UINT32 m_width;
        UINT32 m_height;
        bool m_isCustomFXEnabled = false;
    };
}

#endif
