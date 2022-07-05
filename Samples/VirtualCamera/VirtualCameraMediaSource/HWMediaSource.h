//
// Copyright (C) Microsoft Corporation. All rights reserved.
//
#pragma once

#ifndef HWMEDIASOURCE_H
#define HWMEDIASOURCE_H

namespace winrt::WindowsSample::implementation
{
    // forward declaration
    struct HWMediaStream;

    //
    // MediaSource that wraps physical hardware
    //
    struct HWMediaSource : winrt::implements<HWMediaSource, IMFMediaSourceEx, IMFGetService, IKsControl, IMFSampleAllocatorControl>
    {
        HWMediaSource() = default;
        ~HWMediaSource();

    private:
        enum class SourceState
        {
            Invalid, Stopped, Started, Shutdown
        };

    public:
        // IMFMediaEventGenerator (inherited by IMFMediaSource)
        IFACEMETHODIMP BeginGetEvent(_In_ IMFAsyncCallback* pCallback, _In_ IUnknown* punkState) override; 
        IFACEMETHODIMP EndGetEvent(_In_ IMFAsyncResult* pResult, _Out_ IMFMediaEvent** ppEvent) override;
        IFACEMETHODIMP GetEvent(DWORD dwFlags, _Out_ IMFMediaEvent** ppEvent) override;
        IFACEMETHODIMP QueueEvent(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, _In_ const PROPVARIANT* pvValue) override;

        // IMFMediaSource (inherited by IMFMediaSourceEx)
        IFACEMETHODIMP CreatePresentationDescriptor(_Out_ IMFPresentationDescriptor** ppPresentationDescriptor) override;
        IFACEMETHODIMP GetCharacteristics(_Out_ DWORD* pdwCharacteristics) override;
        IFACEMETHODIMP Pause() override;
        IFACEMETHODIMP Shutdown() override;
        IFACEMETHODIMP Start(_In_ IMFPresentationDescriptor* pPresentationDescriptor, _In_ const GUID* pguidTimeFormat, _In_ const PROPVARIANT* pvarStartPosition) override;
        IFACEMETHODIMP Stop() override;

        // IMFMediaSourceEx
        IFACEMETHODIMP GetSourceAttributes(_COM_Outptr_ IMFAttributes** ppAttributes) override;
        IFACEMETHODIMP GetStreamAttributes(DWORD dwStreamIdentifier, _COM_Outptr_ IMFAttributes** ppAttributes) override;
        IFACEMETHODIMP SetD3DManager(_In_opt_ IUnknown* pManager) override;

        // IMFGetService
        IFACEMETHODIMP GetService(_In_ REFGUID guidService, _In_ REFIID riid, _Out_ LPVOID* ppvObject) override;

        // IKsControl
        IFACEMETHODIMP KsProperty(
            _In_reads_bytes_(ulPropertyLength) PKSPROPERTY pProperty,
            _In_ ULONG ulPropertyLength,
            _Inout_updates_to_(ulDataLength, *pBytesReturned) LPVOID pPropertyData,
            _In_ ULONG ulDataLength,
            _Out_ ULONG* pBytesReturned) override;
        IFACEMETHODIMP KsMethod(
            _In_reads_bytes_(ulMethodLength) PKSMETHOD pMethod,
            _In_ ULONG ulMethodLength,
            _Inout_updates_to_(ulDataLength, *pBytesReturned) LPVOID pMethodData,
            _In_ ULONG ulDataLength,
            _Out_ ULONG* pBytesReturned) override;
        IFACEMETHODIMP KsEvent(
            _In_reads_bytes_opt_(ulEventLength) PKSEVENT pEvent,
            _In_ ULONG ulEventLength,
            _Inout_updates_to_(ulDataLength, *pBytesReturned) LPVOID pEventData,
            _In_ ULONG ulDataLength,
            _Out_opt_ ULONG* pBytesReturned) override;
        
        // IMFSampleAlloactorControl
        IFACEMETHODIMP SetDefaultAllocator(
            _In_  DWORD dwOutputStreamID,
            _In_  IUnknown* pAllocator) override;

        IFACEMETHODIMP GetAllocatorUsage(
            _In_  DWORD dwOutputStreamID,
            _Out_  DWORD* pdwInputStreamID,
            _Out_  MFSampleAllocatorUsage* peUsage) override;

        // Non-Interface functions
        HRESULT Initialize(_In_ IMFAttributes* pAttributes, _In_ IMFMediaSource* pMediaSource);

    private:
        _Requires_lock_held_(m_Lock) HRESULT _CheckShutdownRequiresLock();
        _Requires_lock_held_(m_Lock) HRESULT _CreateSourceAttributes(_In_ IMFAttributes* pActivateAttributes);
        _Requires_lock_held_(m_Lock) HRESULT _CreateMediaStreams();

        void OnMediaSourceEvent(_In_ IMFAsyncResult* pResult);
        wil::com_ptr_nothrow<CAsyncCallback<HWMediaSource>> m_xOnMediaSourceEvent;

        _Requires_lock_held_(m_Lock) HRESULT OnNewStream(IMFMediaEvent* pEvent, MediaEventType met);
        _Requires_lock_held_(m_Lock) HRESULT OnSourceStarted(IMFMediaEvent* pEvent);
        _Requires_lock_held_(m_Lock) HRESULT OnSourceStopped(IMFMediaEvent* pEvent);

        winrt::slim_mutex m_Lock;
        SourceState m_sourceState{ SourceState::Invalid };
        wil::com_ptr_nothrow<IMFMediaEventQueue> m_spEventQueue;
        wil::com_ptr_nothrow<IMFAttributes> m_spAttributes;
        wil::com_ptr_nothrow<IMFPresentationDescriptor> m_spPDesc;

        wil::com_ptr_nothrow<IMFMediaSource> m_spDevSource;
        DWORD m_dwSerialWorkQueueId;
        bool m_initalized = false;

        wil::unique_cotaskmem_array_ptr<wil::com_ptr_nothrow<HWMediaStream>> m_streamList;
    };
}


#endif
