//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#include "pch.h"
#include "HWMediaStream.h"

namespace winrt::WindowsSample::implementation
{
    HWMediaStream::~HWMediaStream()
    {
        Shutdown();
    }

    HRESULT HWMediaStream::Initialize(_In_ HWMediaSource* pSource, _In_ IMFStreamDescriptor* pStreamDesc, DWORD dwWorkQueue)
    {
        DEBUG_MSG(L"Initialize enter");
        RETURN_HR_IF_NULL(E_INVALIDARG, pSource);
        m_parent = pSource;

        RETURN_HR_IF_NULL(E_INVALIDARG, pStreamDesc);
        m_spStreamDesc = pStreamDesc;

        RETURN_IF_FAILED(MFCreateEventQueue(&m_spEventQueue));

        RETURN_IF_FAILED(m_spStreamDesc->GetStreamIdentifier(&m_dwStreamIdentifier));

        m_dwSerialWorkQueueId = dwWorkQueue;

        auto ptr = winrt::make_self<CAsyncCallback<HWMediaStream>>(this, &HWMediaStream::OnMediaStreamEvent, m_dwSerialWorkQueueId);
        m_xOnMediaStreamEvent.attach(ptr.detach());

        DEBUG_MSG(L"Initialize exit, streamId: %d", m_dwStreamIdentifier);
        return S_OK;
    }

    // IMFMediaEventGenerator
    IFACEMETHODIMP HWMediaStream::BeginGetEvent(
            _In_ IMFAsyncCallback* pCallback,
            _In_ IUnknown* punkState
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        RETURN_IF_FAILED(m_spEventQueue->BeginGetEvent(pCallback, punkState));

        return S_OK;
    }

    IFACEMETHODIMP HWMediaStream::EndGetEvent(
            _In_ IMFAsyncResult* pResult,
            _COM_Outptr_ IMFMediaEvent** ppEvent
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        RETURN_IF_FAILED(m_spEventQueue->EndGetEvent(pResult, ppEvent));

        return S_OK;
    }

    IFACEMETHODIMP HWMediaStream::GetEvent(
            DWORD dwFlags,
            _COM_Outptr_ IMFMediaEvent** ppEvent
        )
    {
        // NOTE:
        // GetEvent can block indefinitely, so we don't hold the lock.
        // This requires some juggling with the event queue pointer.
        wil::com_ptr_nothrow<IMFMediaEventQueue> spQueue;

        {
            winrt::slim_lock_guard lock(m_Lock);

            RETURN_IF_FAILED(_CheckShutdownRequiresLock());
            spQueue = m_spEventQueue;
        }

        // Now get the event.
        RETURN_IF_FAILED(spQueue->GetEvent(dwFlags, ppEvent));

        return S_OK;
    }

    IFACEMETHODIMP HWMediaStream::QueueEvent(
            MediaEventType eventType,
            REFGUID guidExtendedType,
            HRESULT hrStatus,
            _In_opt_ PROPVARIANT const* pvValue
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        RETURN_IF_FAILED(m_spEventQueue->QueueEventParamVar(eventType, guidExtendedType, hrStatus, pvValue));

        return S_OK;
    }

    // IMFMediaStream
    IFACEMETHODIMP HWMediaStream::GetMediaSource(
            _COM_Outptr_ IMFMediaSource** ppMediaSource
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_HR_IF_NULL(E_POINTER, ppMediaSource);
        *ppMediaSource = nullptr;

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        RETURN_IF_FAILED(m_parent.copy_to(ppMediaSource));

        return S_OK;
    }

    IFACEMETHODIMP HWMediaStream::GetStreamDescriptor(
            _COM_Outptr_ IMFStreamDescriptor** ppStreamDescriptor
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_HR_IF_NULL(E_POINTER, ppStreamDescriptor);
        *ppStreamDescriptor = nullptr;

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        if (m_spStreamDesc != nullptr)
        {
            RETURN_IF_FAILED(m_spStreamDesc.copy_to(ppStreamDescriptor));
        }
        else
        {
            return E_UNEXPECTED;
        }

        return S_OK;
    }

    IFACEMETHODIMP HWMediaStream::RequestSample(
            _In_ IUnknown* pToken
        )
    {
        winrt::slim_lock_guard lock(m_Lock);
        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        RETURN_IF_FAILED(m_spDevStream->RequestSample(pToken));

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    // IMFMediaStream2
    IFACEMETHODIMP HWMediaStream::SetStreamState(MF_STREAM_STATE state)
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        DEBUG_MSG(L"[%d] SetStreamState %d", m_dwStreamIdentifier, state);

        wil::com_ptr_nothrow<IMFMediaStream2> spStream2;
        RETURN_IF_FAILED(m_spDevStream->QueryInterface(IID_PPV_ARGS(&spStream2)));
        RETURN_IF_FAILED(spStream2->SetStreamState(state));

        return S_OK;
    }

    IFACEMETHODIMP HWMediaStream::GetStreamState(_Out_ MF_STREAM_STATE* pState)
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_HR_IF_NULL(E_POINTER, pState);
        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        wil::com_ptr_nothrow<IMFMediaStream2> spStream2;
        RETURN_IF_FAILED(m_spDevStream->QueryInterface(IID_PPV_ARGS(&spStream2)));
        RETURN_IF_FAILED(spStream2->GetStreamState(pState));

        return S_OK;
    }

    void HWMediaStream::OnMediaStreamEvent(_In_ IMFAsyncResult* pResult)
    {
        HRESULT hr = S_OK;

        // Forward deviceStream event
        wil::com_ptr_nothrow<IUnknown> spUnknown;
        wil::com_ptr_nothrow<IMFMediaStream> spMediaStream;
        wil::com_ptr_nothrow<IMFMediaEvent> spEvent;
        MediaEventType met = MEUnknown;
        bool bForwardEvent = true;

        CHECKHR_GOTO(pResult->GetState(&spUnknown), done);

        CHECKHR_GOTO(spUnknown->QueryInterface(IID_PPV_ARGS(&spMediaStream)), done);

        CHECKHR_GOTO(spMediaStream->EndGetEvent(pResult, &spEvent), done);
        CHECKNULL_GOTO(spEvent, MF_E_UNEXPECTED, done);

        CHECKHR_GOTO(spEvent->GetType(&met), done);
        DEBUG_MSG(L"[%d] OnMediaStreamEvent, streamId: %d, met:%d ", m_dwStreamIdentifier, met);
        
        // This shows how to intercept sample from physical camera
        // and do custom processin gon the sample.
        // 
        if (met == MEMediaSample)
        {
            wil::com_ptr_nothrow<IMFSample> spSample;
            wil::com_ptr_nothrow<IUnknown> spToken;
            wil::com_ptr_nothrow<IUnknown> spSampleUnk;

            wil::unique_prop_variant propVar = {};
            CHECKHR_GOTO(spEvent->GetValue(&propVar), done);
            if (VT_UNKNOWN != propVar.vt)
            {
                CHECKHR_GOTO(MF_E_UNEXPECTED, done);
            }
            spSampleUnk = propVar.punkVal;
            CHECKHR_GOTO(spSampleUnk->QueryInterface(IID_PPV_ARGS(&spSample)), done);

            CHECKHR_GOTO(ProcessSample(spSample.get()), done);
            bForwardEvent = false;
        }
        else if (met == MEError)
        {
            goto done;
        }

    done:
        wil::com_ptr_nothrow<IMFMediaSource> spParent;
        {
            winrt::slim_lock_guard lock(m_Lock);
            if (SUCCEEDED(_CheckShutdownRequiresLock()))
            {
                if (FAILED(hr) || met == MEError)
                {
                    spParent = m_parent;
                    DEBUG_MSG(L"[stream:%d] error when processing stream event: met=%d, hr=0x%08x", m_dwStreamIdentifier, met, hr);
                }
                else
                {
                    // Forward event
                    if (bForwardEvent)
                    {
                        (void)(m_spEventQueue->QueueEvent(spEvent.get()));
                    }

                    // Continue listening to source event
                    (void)(spMediaStream->BeginGetEvent(m_xOnMediaStreamEvent.get(), m_spDevStream.get()));
                }
            }
            if (FAILED(hr) && spParent != nullptr)
            {
                (void)spParent->QueueEvent(MEError, GUID_NULL, hr, nullptr);
            }
        }

        DEBUG_MSG(L"[%d] OnMediaStreamEvent exit", m_dwStreamIdentifier);
    }

    HRESULT HWMediaStream::ProcessSample(_In_ IMFSample* inputSample)
    {
        winrt::slim_lock_guard lock(m_Lock);

        // Do custom processing on the sample from the physical camera

        if (SUCCEEDED(_CheckShutdownRequiresLock()))
        {
            // queue event
            RETURN_IF_FAILED(m_spEventQueue->QueueEventParamUnk(
                MEMediaSample,
                GUID_NULL,
                S_OK,
                inputSample));
        }
        return S_OK;
    }

    HRESULT HWMediaStream::Shutdown()
    {
        HRESULT hr = S_OK;
        winrt::slim_lock_guard lock(m_Lock);

        m_isShutdown = true;
        m_parent.reset();
        m_spDevStream.reset();

        if (m_spEventQueue != nullptr)
        {
            hr = m_spEventQueue->Shutdown();
            m_spEventQueue.reset();
        }

        m_spStreamDesc.reset();

        return hr;
    }

    HRESULT HWMediaStream::SetMediaStream(_In_ IMFMediaStream* pMediaStream)
    {
        winrt::slim_lock_guard lock(m_Lock);
        DEBUG_MSG(L"[%d] Set MediaStream %p ", m_dwStreamIdentifier, pMediaStream);

        RETURN_HR_IF_NULL(E_INVALIDARG, pMediaStream);
        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        m_spDevStream = pMediaStream;
        RETURN_IF_FAILED(m_spDevStream->BeginGetEvent(m_xOnMediaStreamEvent.get(), m_spDevStream.get()));

        return S_OK;
    }

    _Requires_lock_held_(m_Lock)
    HRESULT HWMediaStream::_CheckShutdownRequiresLock()
    {
        if (m_isShutdown)
        {
            return MF_E_SHUTDOWN;
        }

        if (m_spEventQueue == nullptr)
        {
            return E_UNEXPECTED;

        }
        return S_OK;
    }
}

