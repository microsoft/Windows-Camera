#pragma once
#ifndef HWMEDIASOIURCE_H
#define HWMEDIASOIURCE_H

#include <vector>

// TODO: Rename SimpleMediaSource -> SWMediaSource private
// TODO: Rename public source to VCamSource (use either HWMediaSource / SimpleMediaSource base on the activation)
// TODO: Add IMFSampleAllocatorControl

namespace winrt::WindowsSample::implementation
{
    // forward declaration
    struct HWMediaStream;

    //
    // MediaSource that wrapper real hardware
    //
    struct HWMediaSource : winrt::implements<HWMediaSource, IMFMediaSourceEx, IMFGetService, IKsControl>
    {
        HWMediaSource() = default;

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
        IFACEMETHOD(KsProperty)(_In_reads_bytes_(ulPropertyLength) PKSPROPERTY pProperty,
            _In_ ULONG ulPropertyLength,
            _Inout_updates_to_(ulDataLength, *pBytesReturned) LPVOID pPropertyData,
            _In_ ULONG ulDataLength,
            _Out_ ULONG* pBytesReturned);
        IFACEMETHOD(KsMethod)(_In_reads_bytes_(ulMethodLength) PKSMETHOD pMethod,
            _In_ ULONG ulMethodLength,
            _Inout_updates_to_(ulDataLength, *pBytesReturned) LPVOID pMethodData,
            _In_ ULONG ulDataLength,
            _Out_ ULONG* pBytesReturned);
        IFACEMETHOD(KsEvent)(_In_reads_bytes_opt_(ulEventLength) PKSEVENT pEvent,
            _In_ ULONG ulEventLength,
            _Inout_updates_to_(ulDataLength, *pBytesReturned) LPVOID pEventData,
            _In_ ULONG ulDataLength,
            _Out_opt_ ULONG* pBytesReturned);

        HRESULT Initialize(LPCWSTR pwszSymLink);

    private:
        HRESULT _CheckShutdownRequiresLock();

        winrt::slim_mutex m_Lock;
        SourceState _sourceState{ SourceState::Invalid };
        wil::com_ptr_nothrow<IMFMediaEventQueue> _spEventQueue;
        wil::com_ptr_nothrow<IMFAttributes> _spAttributes;
        wil::com_ptr_nothrow<IMFPresentationDescriptor>_spPresentationDescriptor;

        bool _wasStreamPreviouslySelected; // maybe makes more sense as a property of the stream
        wil::com_ptr_nothrow<IMFMediaSource> _devSource;
        wil::com_ptr_nothrow<IMFSourceReader> m_spSourceReader;
        bool bInitialize;

        wil::com_ptr_nothrow<HWMediaStream> m_spStream;
        const DWORD NUM_STREAMS = 1;
    };
}


#endif
