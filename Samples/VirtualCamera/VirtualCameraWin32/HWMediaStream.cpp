#include "pch.h"
#include "HWMediaStream.h"

namespace winrt::WindowsSample::implementation
{
    HRESULT
        HWMediaStream::Initialize(
            _In_ HWMediaSource* pSource,
            _In_ IMFPresentationDescriptor* pPDesc,
            _In_ IMFSourceReader* pSrcReader,
            _In_ DWORD streamIndex
        )
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, pSource);
        _parent = pSource;

        RETURN_HR_IF_NULL(E_INVALIDARG, pPDesc);

        RETURN_HR_IF_NULL(E_INVALIDARG, pSrcReader);
        m_spSrcReader = pSrcReader;

        m_streamIndex = streamIndex;

        RETURN_IF_FAILED(MFCreateEventQueue(&_spEventQueue));
        BOOL selected = FALSE;
        RETURN_IF_FAILED(pPDesc->GetStreamDescriptorByIndex(m_streamIndex, &selected, &_spStreamDesc));

        return S_OK;
    }

    // IMFMediaEventGenerator
    IFACEMETHODIMP
        HWMediaStream::BeginGetEvent(
            _In_ IMFAsyncCallback* pCallback,
            _In_ IUnknown* punkState
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        RETURN_IF_FAILED(_spEventQueue->BeginGetEvent(pCallback, punkState));

        return S_OK;
    }

    IFACEMETHODIMP
        HWMediaStream::EndGetEvent(
            _In_ IMFAsyncResult* pResult,
            _COM_Outptr_ IMFMediaEvent** ppEvent
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        RETURN_IF_FAILED(_spEventQueue->EndGetEvent(pResult, ppEvent));

        return S_OK;
    }

    IFACEMETHODIMP
        HWMediaStream::GetEvent(
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
            spQueue = _spEventQueue;
        }

        // Now get the event.
        RETURN_IF_FAILED(_spEventQueue->GetEvent(dwFlags, ppEvent));

        return S_OK;
    }

    IFACEMETHODIMP
        HWMediaStream::QueueEvent(
            MediaEventType eventType,
            REFGUID guidExtendedType,
            HRESULT hrStatus,
            _In_opt_ PROPVARIANT const* pvValue
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        RETURN_IF_FAILED(_spEventQueue->QueueEventParamVar(eventType, guidExtendedType, hrStatus, pvValue));

        return S_OK;
    }

    // IMFMediaStream
    IFACEMETHODIMP
        HWMediaStream::GetMediaSource(
            _COM_Outptr_ IMFMediaSource** ppMediaSource
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_HR_IF_NULL(E_POINTER, ppMediaSource);
        *ppMediaSource = nullptr;

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        *ppMediaSource = _parent.get();
        (*ppMediaSource)->AddRef();

        return S_OK;
    }

    IFACEMETHODIMP
        HWMediaStream::GetStreamDescriptor(
            _COM_Outptr_ IMFStreamDescriptor** ppStreamDescriptor
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_HR_IF_NULL(E_POINTER, ppStreamDescriptor);
        *ppStreamDescriptor = nullptr;

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        if (_spStreamDesc != nullptr)
        {
            *ppStreamDescriptor = _spStreamDesc.get();
            (*ppStreamDescriptor)->AddRef();
        }
        else
        {
            return E_UNEXPECTED;
        }

        return S_OK;
    }

    IFACEMETHODIMP
        HWMediaStream::RequestSample(
            _In_ IUnknown* pToken
        )
    {
        winrt::slim_lock_guard lock(m_Lock);
        wil::com_ptr_nothrow<IMFSample> spSample;
        int retry = 3;

        // TODO: How to handle null sample from source
        while (retry > 0)
        {
            DWORD dwActualStreamIndex = 0;
            DWORD dwControlFlags = 0;
            DWORD dwStreamFlags = 0;
            LONGLONG llTimestamp;
            RETURN_IF_FAILED(m_spSrcReader->ReadSample(
                m_streamIndex,
                dwControlFlags,
                &dwActualStreamIndex,
                &dwStreamFlags,
                &llTimestamp,
                &spSample));

            if (spSample == nullptr)
            {
                retry--;
                continue;
            }

            if (pToken != nullptr)
            {
                RETURN_IF_FAILED(spSample->SetUnknown(MFSampleExtension_Token, pToken));
            }

            RETURN_IF_FAILED(_spEventQueue->QueueEventParamUnk(MEMediaSample,
                GUID_NULL,
                S_OK,
                spSample.get()));
            DEBUG_MSG(L"[HWMediaStream] Sample send");
            break;
        }
        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    // IMFMediaStream2
    IFACEMETHODIMP
        HWMediaStream::SetStreamState(
            MF_STREAM_STATE state
        )
    {
        winrt::slim_lock_guard lock(m_Lock);
        bool runningState = false;

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        switch (state)
        {
        case MF_STREAM_STATE_PAUSED:
            // because not supported
            return S_OK;

        case MF_STREAM_STATE_RUNNING:
            DEBUG_MSG(L"[HWMediaStream]  Set Stream state to running \n");
            RETURN_IF_FAILED(m_spSrcReader->SetStreamSelection(m_streamIndex, TRUE));
            break;

        case MF_STREAM_STATE_STOPPED:
            DEBUG_MSG(L"[HWMediaStream]  Set Stream state to stop \n");
            RETURN_IF_FAILED(m_spSrcReader->SetStreamSelection(m_streamIndex, FALSE));
            break;

        default:
            return MF_E_INVALID_STATE_TRANSITION;
        }

        return S_OK;
    }

    IFACEMETHODIMP
        HWMediaStream::GetStreamState(
            _Out_ MF_STREAM_STATE* pState
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_HR_IF_NULL(E_POINTER, pState);
        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        BOOL selected = false;
        RETURN_IF_FAILED(m_spSrcReader->GetStreamSelection(m_streamIndex, &selected));
        *pState = (selected ? MF_STREAM_STATE_RUNNING : MF_STREAM_STATE_STOPPED);

        return S_OK;
    }

    HRESULT
        HWMediaStream::Shutdown(
        )
    {
        HRESULT hr = S_OK;
        winrt::slim_lock_guard lock(m_Lock);

        _isShutdown = true;
        _parent.reset();

        // TODO:flush?
        m_spSrcReader.reset();

        if (_spEventQueue != nullptr)
        {
            hr = _spEventQueue->Shutdown();
            _spEventQueue.reset();
        }

        _spStreamDesc.reset();

        return hr;
    }

    HRESULT
        HWMediaStream::_CheckShutdownRequiresLock(
        )
    {
        if (_isShutdown)
        {
            return MF_E_SHUTDOWN;
        }

        if (_spEventQueue == nullptr)
        {
            return E_UNEXPECTED;

        }
        return S_OK;
    }
}
