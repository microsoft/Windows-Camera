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

    HRESULT HWMediaStream::OnMediaStreamEvent(_In_ IMFAsyncResult* pResult)
    {
        // Forward deviceStream event
        wil::com_ptr_nothrow<IUnknown> spUnknown;
        RETURN_IF_FAILED(pResult->GetState(&spUnknown));

        wil::com_ptr_nothrow<IMFMediaStream> spMediaStream;
        RETURN_IF_FAILED(spUnknown->QueryInterface(IID_PPV_ARGS(&spMediaStream)));

        wil::com_ptr_nothrow<IMFMediaEvent> spEvent;
        RETURN_IF_FAILED(spMediaStream->EndGetEvent(pResult, &spEvent));
        RETURN_HR_IF_NULL(MF_E_UNEXPECTED, spEvent);

        MediaEventType met;
        RETURN_IF_FAILED(spEvent->GetType(&met));
        DEBUG_MSG(L"[%d] OnMediaStreamEvent, streamId: %d, met:%d ", m_dwStreamIdentifier, met);
        
        // This shows how to intercept sample from physical camera
        // and do custom processin gon the sample.
        // 
        bool bForwardEvent = true;
        if (met == MEMediaSample)
        {
            wil::com_ptr_nothrow<IMFSample> spSample;
            wil::com_ptr_nothrow<IUnknown> spToken;
            wil::com_ptr_nothrow<IUnknown> spSampleUnk;

            wil::unique_prop_variant propVar = {};
            RETURN_IF_FAILED(spEvent->GetValue(&propVar));
            if (VT_UNKNOWN != propVar.vt)
            {
                RETURN_HR(MF_E_UNEXPECTED);
            }
            spSampleUnk = propVar.punkVal;
            RETURN_IF_FAILED(spSampleUnk->QueryInterface(IID_PPV_ARGS(&spSample)));

            RETURN_IF_FAILED(ProcessSample(spSample.get()));
            bForwardEvent = false;
        }

        {
            winrt::slim_lock_guard lock(m_Lock);
            if (SUCCEEDED(_CheckShutdownRequiresLock()))
            {
                // Forward event
                if (bForwardEvent)
                {
                    RETURN_IF_FAILED(m_spEventQueue->QueueEvent(spEvent.get()));
                }

                // Continue listening to source event
                RETURN_IF_FAILED(spMediaStream->BeginGetEvent(m_xOnMediaStreamEvent.get(), m_spDevStream.get()));
            }
        }
        
        DEBUG_MSG(L"[%d] OnMediaStreamEvent exit", m_dwStreamIdentifier);
        return S_OK;
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

