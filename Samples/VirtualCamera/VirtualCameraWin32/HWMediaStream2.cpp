//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#include "pch.h"
#include "HWMediaStream2.h"

namespace winrt::WindowsSample::implementation
{
    HWMediaStream2::~HWMediaStream2()
    {
        Shutdown();
    }

    HRESULT HWMediaStream2::Initialize(_In_ HWMediaSource2* pSource, _In_ IMFStreamDescriptor* pStreamDesc, DWORD dwWorkQueue)
    {
        DEBUG_MSG(L"Initailize enter");
        RETURN_HR_IF_NULL(E_INVALIDARG, pSource);
        m_parent = pSource;

        RETURN_HR_IF_NULL(E_INVALIDARG, pStreamDesc);
        m_spStreamDesc = pStreamDesc;

        RETURN_IF_FAILED(MFCreateEventQueue(&m_spEventQueue));
        BOOL selected = FALSE;

        RETURN_IF_FAILED(m_spStreamDesc->GetStreamIdentifier(&m_dwStreamIdentifier));

        m_dwSerialWorkQueueId = dwWorkQueue;

        auto ptr = winrt::make_self<CAsyncCallback<HWMediaStream2>>(this, &HWMediaStream2::OnMediaStreamEvent, m_dwSerialWorkQueueId);
        m_xOnMediaStreamEvent.attach(ptr.detach());

        DEBUG_MSG(L"Initailize exit, streamId: %d", m_dwStreamIdentifier);
        return S_OK;
    }

    // IMFMediaEventGenerator
    IFACEMETHODIMP HWMediaStream2::BeginGetEvent(
            _In_ IMFAsyncCallback* pCallback,
            _In_ IUnknown* punkState
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        RETURN_IF_FAILED(m_spEventQueue->BeginGetEvent(pCallback, punkState));

        return S_OK;
    }

    IFACEMETHODIMP HWMediaStream2::EndGetEvent(
            _In_ IMFAsyncResult* pResult,
            _COM_Outptr_ IMFMediaEvent** ppEvent
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        RETURN_IF_FAILED(m_spEventQueue->EndGetEvent(pResult, ppEvent));

        return S_OK;
    }

    IFACEMETHODIMP HWMediaStream2::GetEvent(
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
        RETURN_IF_FAILED(m_spEventQueue->GetEvent(dwFlags, ppEvent));

        return S_OK;
    }

    IFACEMETHODIMP HWMediaStream2::QueueEvent(
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
    IFACEMETHODIMP HWMediaStream2::GetMediaSource(
            _COM_Outptr_ IMFMediaSource** ppMediaSource
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_HR_IF_NULL(E_POINTER, ppMediaSource);
        *ppMediaSource = nullptr;

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        *ppMediaSource = m_parent.get();
        (*ppMediaSource)->AddRef();

        return S_OK;
    }

    IFACEMETHODIMP HWMediaStream2::GetStreamDescriptor(
            _COM_Outptr_ IMFStreamDescriptor** ppStreamDescriptor
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_HR_IF_NULL(E_POINTER, ppStreamDescriptor);
        *ppStreamDescriptor = nullptr;

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        if (m_spStreamDesc != nullptr)
        {
            *ppStreamDescriptor = m_spStreamDesc.get();
            (*ppStreamDescriptor)->AddRef();
        }
        else
        {
            return E_UNEXPECTED;
        }

        return S_OK;
    }

    IFACEMETHODIMP HWMediaStream2::RequestSample(
            _In_ IUnknown* pToken
        )
    {
        winrt::slim_lock_guard lock(m_Lock);
        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        RETURN_IF_FAILED(m_devStream->RequestSample(pToken));

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    // IMFMediaStream2
    IFACEMETHODIMP HWMediaStream2::SetStreamState(MF_STREAM_STATE state)
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        DEBUG_MSG(L"[%d] SetStreamState %d", m_dwStreamIdentifier, state);

        wil::com_ptr_nothrow<IMFMediaStream2> spStream2;
        RETURN_IF_FAILED(m_devStream.query_to(&spStream2));
        RETURN_IF_FAILED(spStream2->SetStreamState(state));

        return S_OK;
    }

    IFACEMETHODIMP HWMediaStream2::GetStreamState(_Out_ MF_STREAM_STATE* pState)
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_HR_IF_NULL(E_POINTER, pState);
        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        wil::com_ptr_nothrow<IMFMediaStream2> spStream2;
        RETURN_IF_FAILED(m_devStream.query_to(&spStream2));
        RETURN_IF_FAILED(spStream2->GetStreamState(pState));

        return S_OK;
    }

    HRESULT HWMediaStream2::OnMediaStreamEvent(_In_ IMFAsyncResult* pResult)
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

        {
            winrt::slim_lock_guard lock(m_Lock);
            if (SUCCEEDED(_CheckShutdownRequiresLock()))
            {
                // Forward event
                RETURN_IF_FAILED(m_spEventQueue->QueueEvent(spEvent.get()));
                
                // Continue listening to source event
                RETURN_IF_FAILED(spMediaStream->BeginGetEvent(m_xOnMediaStreamEvent.get(), m_devStream.get()));
            }
        }
        
        DEBUG_MSG(L"[%d] OnMediaStreamEvent exit", m_dwStreamIdentifier);
        return S_OK;
    }

    HRESULT HWMediaStream2::Shutdown()
    {
        HRESULT hr = S_OK;
        winrt::slim_lock_guard lock(m_Lock);

        m_isShutdown = true;
        m_parent.reset();
        m_devStream.reset();

        if (m_spEventQueue != nullptr)
        {
            hr = m_spEventQueue->Shutdown();
            m_spEventQueue.reset();
        }

        m_spStreamDesc.reset();

        return hr;
    }

    HRESULT HWMediaStream2::SetMediaStream(_In_ IMFMediaStream* pMediaStream)
    {
        winrt::slim_lock_guard lock(m_Lock);
        DEBUG_MSG(L"[%d] Set MediaStream %p ", m_dwStreamIdentifier, pMediaStream);

        RETURN_HR_IF_NULL(E_INVALIDARG, pMediaStream);
        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        // Check stream state?
        m_devStream = pMediaStream;
        RETURN_IF_FAILED(m_devStream->BeginGetEvent(m_xOnMediaStreamEvent.get(), m_devStream.get()));

        return S_OK;
    }

    HRESULT HWMediaStream2::_CheckShutdownRequiresLock()
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

