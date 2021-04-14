//
// Copyright (C) Microsoft Corporation. All rights reserved.
//
#pragma once
#ifndef SIMPLEMEDIASOURCE_H
#define SIMPLEMEDIASOURCE_H

namespace winrt
{
    template<> bool is_guid_of<IMFMediaSourceEx>(guid const& id) noexcept
    {
        return is_guid_of<IMFMediaSourceEx, IMFMediaSource, IMFMediaEventGenerator>(id);
    }
}

namespace winrt::WindowsSample::implementation
{
    // forward declaration
    struct SimpleMediaStream;
    
    struct SimpleMediaSource : winrt::implements<SimpleMediaSource, IMFMediaSourceEx, IMFGetService, IKsControl, IMFSampleAllocatorControl>
    {
        SimpleMediaSource() = default;

    private:
        enum class SourceState
        {
            Invalid, Stopped, Started, Shutdown
        };

    public:
        // IMFMediaEventGenerator (inherits by IMFMediaSource)
        IFACEMETHOD(BeginGetEvent)(_In_ IMFAsyncCallback* pCallback, _In_ IUnknown* punkState);
        IFACEMETHOD(EndGetEvent)(_In_ IMFAsyncResult* pResult, _Out_ IMFMediaEvent** ppEvent);
        IFACEMETHOD(GetEvent)(DWORD dwFlags, _Out_ IMFMediaEvent** ppEvent);
        IFACEMETHOD(QueueEvent)(MediaEventType met, REFGUID guidExtendedType, HRESULT hrStatus, _In_ const PROPVARIANT* pvValue);

        // IMFMediaSource (inherits by IMFMediaSourceEx)
        IFACEMETHOD(CreatePresentationDescriptor)(_Out_ IMFPresentationDescriptor** ppPresentationDescriptor);
        IFACEMETHOD(GetCharacteristics)(_Out_ DWORD* pdwCharacteristics);
        IFACEMETHOD(Pause)();
        IFACEMETHOD(Shutdown)();
        IFACEMETHOD(Start)(_In_ IMFPresentationDescriptor* pPresentationDescriptor, _In_ const GUID* pguidTimeFormat, _In_ const PROPVARIANT* pvarStartPosition);
        IFACEMETHOD(Stop)();

        // IMFMediaSourceEx
        IFACEMETHOD(GetSourceAttributes)(_COM_Outptr_ IMFAttributes** ppAttributes);
        IFACEMETHOD(GetStreamAttributes)(DWORD dwStreamIdentifier, _COM_Outptr_ IMFAttributes** ppAttributes);
        IFACEMETHOD(SetD3DManager)(_In_opt_ IUnknown* pManager);

        // IMFGetService
        IFACEMETHOD(GetService)(_In_ REFGUID guidService, _In_ REFIID riid, _Out_ LPVOID* ppvObject);

        // IKsControl
        IFACEMETHOD(KsProperty)(
            _In_reads_bytes_(ulPropertyLength) PKSPROPERTY pProperty,
            _In_ ULONG ulPropertyLength,
            _Inout_updates_to_(ulDataLength, *pBytesReturned) LPVOID pPropertyData,
            _In_ ULONG ulDataLength,
            _Out_ ULONG* pBytesReturned);
        IFACEMETHOD(KsMethod)(
            _In_reads_bytes_(ulMethodLength) PKSMETHOD pMethod,
            _In_ ULONG ulMethodLength,
            _Inout_updates_to_(ulDataLength, *pBytesReturned) LPVOID pMethodData,
            _In_ ULONG ulDataLength,
            _Out_ ULONG* pBytesReturned);
        IFACEMETHOD(KsEvent)(
            _In_reads_bytes_opt_(ulEventLength) PKSEVENT pEvent,
            _In_ ULONG ulEventLength,
            _Inout_updates_to_(ulDataLength, *pBytesReturned) LPVOID pEventData,
            _In_ ULONG ulDataLength,
            _Out_opt_ ULONG* pBytesReturned);

        // IMFSampleAlloactorControl
        IFACEMETHOD(SetDefaultAllocator)(
            _In_  DWORD dwOutputStreamID,
            _In_  IUnknown* pAllocator);

        IFACEMETHOD(GetAllocatorUsage)(
            _In_  DWORD dwOutputStreamID,
            _Out_  DWORD* pdwInputStreamID,
            _Out_  MFSampleAllocatorUsage* peUsage);

        // Non-Interface functions
        HRESULT Initialize();

    private:
        HRESULT _CheckShutdownRequiresLock();
        HRESULT _ValidatePresentationDescriptor(_In_ IMFPresentationDescriptor* pPresentationDescriptor);
        HRESULT _CreateSourceAttributes();
        HRESULT _GetStreamDescriptorByStreamId(_In_ DWORD dwStreamId, _Out_ DWORD* pdwStreamIdx, _Out_ bool* pSelected, _COM_Outptr_ IMFStreamDescriptor** ppStreamDescriptor);
        HRESULT _GetMediaStreamById(_In_ DWORD dwStreamId, _COM_Outptr_ SimpleMediaStream** ppMediaStream);

        winrt::slim_mutex m_Lock;
        SourceState _sourceState{ SourceState::Invalid };

        wil::com_ptr_nothrow<IMFMediaEventQueue> _spEventQueue;
        wil::com_ptr_nothrow<IMFPresentationDescriptor> _spPresentationDescriptor;
        wil::com_ptr_nothrow<IMFAttributes> _spAttributes;
        wil::unique_cotaskmem_array_ptr<wil::com_ptr_nothrow<SimpleMediaStream>> m_streamList;

        const DWORD NUM_STREAMS = 1;
        bool bInitialize;
    };
}

#endif