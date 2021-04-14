#pragma once
#include "HWMediaSource.h"

// this is declare in SimpleMediaSource
//namespace winrt
//{
//    template<> bool is_guid_of<IMFMediaStream2>(guid const& id) noexcept
//    {
//        return is_guid_of<IMFMediaStream2, IMFMediaStream, IMFMediaEventGenerator>(id);
//    }
//}

namespace winrt::WindowsSample::implementation
{
    struct HWMediaStream : winrt::implements<HWMediaStream, IMFMediaStream2>
    {
        friend struct HWMediaSource;

    public:
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
        HRESULT Initialize(_In_ HWMediaSource* pSource, _In_ IMFPresentationDescriptor* pPDesc, _In_ IMFSourceReader* pSrcReader, _In_ DWORD streamIndex);
        HRESULT Shutdown();

    protected:
        HRESULT _CheckShutdownRequiresLock();

        // TODO: change to std
        winrt::slim_mutex  m_Lock;

        wil::com_ptr_nothrow<IMFMediaSource> _parent;
        wil::com_ptr_nothrow<IMFStreamDescriptor> _spStreamDesc;
        wil::com_ptr_nothrow<IMFSourceReader> m_spSrcReader;

        wil::com_ptr_nothrow<IMFMediaEventQueue> _spEventQueue;
        //wil::com_ptr_nothrow<IMFAttributes> _spAttributes;
        //wil::com_ptr_nothrow<IMFMediaType> _spMediaType;
        
        bool _isShutdown = false;
        bool _isSelected = false;
        DWORD m_streamIndex;
    };
}


