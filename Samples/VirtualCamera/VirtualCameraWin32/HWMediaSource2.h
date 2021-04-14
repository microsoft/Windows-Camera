//
// Copyright (C) Microsoft Corporation. All rights reserved.
//
#pragma once

#ifndef HWMEDIASOIURCE2_H
#define HWMEDIASOIURCE2_H

namespace winrt::WindowsSample::implementation
{
    // forward declaration
    struct HWMediaStream2;

    //
    // MediaSource that wrapper real hardware
    //
    struct HWMediaSource2 : winrt::implements<HWMediaSource2, IMFMediaSourceEx, IMFGetService, IKsControl, IMFSampleAllocatorControl>
    {
        HWMediaSource2() = default;
        ~HWMediaSource2();

    private:
        enum class SourceState
        {
            Invalid, Stopped, Started, Shutdown
        };

    public:
        // override implementation from winrt::implementation
        // TODO: Remove when IMFSampleAllocator is fix in devicesource.
        impl::hresult_type __stdcall QueryInterface(impl::guid_type const& id, void** object) noexcept
        {
            if (FAILED(implements::QueryInterface(reinterpret_cast<guid const&>(id), object)))
            {
                // project interface from the device source.
                // for internal interface like IDeviceS
                RETURN_IF_FAILED(_devSource->QueryInterface(id, object));
            }
        }

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

        HRESULT Initialize(LPCWSTR pwszSymLink);

    private:
        HRESULT _CheckShutdownRequiresLock();
        HRESULT _CreateSourceAttributes();

        HRESULT OnMediaSourceEvent(_In_ IMFAsyncResult* pResult);
        wil::com_ptr_nothrow<CAsyncCallback<HWMediaSource2>> m_xOnMediaSourceEvent;
        //METHODASYNCCALLBACKEX2(OnMediaSourceEvent, HWMediaSource, GetSerialWorkQueue);

        HRESULT OnNewStream(IMFMediaEvent* pEvent, MediaEventType met);
        HRESULT OnSourceStarted(IMFMediaEvent* pEvent);
        HRESULT OnSourceStopped(IMFMediaEvent* pEvent);

        HRESULT CreateMediaStreams();

        winrt::slim_mutex m_Lock;
        SourceState _sourceState{ SourceState::Invalid };
        wil::com_ptr_nothrow<IMFMediaEventQueue> _spEventQueue;
        wil::com_ptr_nothrow<IMFAttributes> _spAttributes;
        wil::com_ptr_nothrow<IMFPresentationDescriptor>_spPDesc;

        bool _wasStreamPreviouslySelected; // maybe makes more sense as a property of the stream
        wil::com_ptr_nothrow<IMFMediaSource> _devSource;
        bool bInitialize = false;
        DWORD m_dwSerialWorkQueueId;

        //std::vector <wil::com_ptr_nothrow<HWMediaStream2>> m_streamList;
        wil::unique_cotaskmem_array_ptr<wil::com_ptr<HWMediaStream2>> m_streamList;
    };
}


#endif
