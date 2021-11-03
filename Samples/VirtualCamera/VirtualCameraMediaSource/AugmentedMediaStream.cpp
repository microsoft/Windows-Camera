//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#include "pch.h"
#include "AugmentedMediaStream.h"
#include "mfobjects.h"
#include "d3d11.h"
#include "dxgi.h"

namespace winrt::WindowsSample::implementation
{
    AugmentedMediaStream::~AugmentedMediaStream()
    {
        Shutdown();
    }

    /// <summary>
    /// Initialize the stream. This is where we filter the MediaTypes exposed out of this stream.
    /// </summary>
    /// <param name="pSource"></param>
    /// <param name="dwStreamId"></param>
    /// <param name="pStreamDesc"></param>
    /// <param name="dwWorkQueue"></param>
    /// <returns></returns>
    HRESULT AugmentedMediaStream::Initialize(
        _In_ AugmentedMediaSource* pSource, 
        _In_ DWORD dwStreamId, 
        _In_ IMFStreamDescriptor* pStreamDesc, 
        _In_ DWORD dwWorkQueue)
    {
        DEBUG_MSG(L"AugmentedMediaStream Initialize enter");
        RETURN_HR_IF_NULL(E_INVALIDARG, pSource);
        m_parent = pSource;

        RETURN_HR_IF_NULL(E_INVALIDARG, pStreamDesc);

        m_dwStreamId = dwStreamId;
        
        // look at stream descriptor, extract list of MediaTypes and filter to preserve only supported MediaTypes
        wil::com_ptr_nothrow<IMFStreamDescriptor> spStreamDesc = pStreamDesc;
        RETURN_IF_FAILED(spStreamDesc->GetStreamIdentifier(&m_dwDevSourceStreamIdentifier));

        m_dwSerialWorkQueueId = dwWorkQueue;

        wil::com_ptr_nothrow<IMFMediaTypeHandler> spSourceStreamMediaTypeHandler;
        RETURN_IF_FAILED(spStreamDesc->GetMediaTypeHandler(&spSourceStreamMediaTypeHandler));
        
        ULONG ulMediaTypeCount = 0;
        ULONG validMediaTypeCount = 0;
        RETURN_IF_FAILED(spSourceStreamMediaTypeHandler->GetMediaTypeCount(&ulMediaTypeCount));
        wil::unique_cotaskmem_array_ptr<wil::com_ptr_nothrow<IMFMediaType>> sourceStreamMediaTypeList = 
            wilEx::make_unique_cotaskmem_array<wil::com_ptr_nothrow<IMFMediaType>>(ulMediaTypeCount);
        RETURN_IF_NULL_ALLOC(sourceStreamMediaTypeList.get());

        for (DWORD i = 0; i < ulMediaTypeCount; i++)
        {
            DEBUG_MSG(L"Looking at MediaType number=%u", i);
            wil::com_ptr_nothrow<IMFMediaType> spMediaType;
            RETURN_IF_FAILED(spSourceStreamMediaTypeHandler->GetMediaTypeByIndex(i, &spMediaType));
            
            GUID majorType;
            spMediaType->GetGUID(MF_MT_MAJOR_TYPE, &majorType);

            GUID subtype;
            spMediaType->GetGUID(MF_MT_SUBTYPE, &subtype);
            
            UINT width = 0, height = 0;
            MFGetAttributeSize(spMediaType.get(), MF_MT_FRAME_SIZE, &width, &height);

            UINT numerator = 0, denominator = 0;
            MFGetAttributeRatio(spMediaType.get(), MF_MT_FRAME_RATE, &numerator, &denominator);
            int framerate = (int)((float)numerator / denominator + 0.5f); //fps

            // Check if this MediaType conforms to our heuristic we decided to filter with:
            // 1- is it a Video MediaType
            // 2- is it a NV12 subtype
            // 3- is its width between [1, 1920] and its height between [1, 1080]
            // 4- is its framerate less than or equal to 30 fps and above or equal to 15fps
            if (IsEqualGUID(majorType, MFMediaType_Video)
                && IsEqualGUID(subtype, MFVideoFormat_NV12)
                && ((width <= 1920 && width > 0) && (height <= 1080 && height > 0))
                && (framerate <= 30 && framerate >= 15))
            {
                DEBUG_MSG(L"Found a valid and compliant Mediatype=%u", i);
                sourceStreamMediaTypeList[validMediaTypeCount] = spMediaType.detach();
                validMediaTypeCount++;
            }
        }
        if (validMediaTypeCount == 0)
        {
            DEBUG_MSG(L"Did not find any valid compliant Mediatype");
            return MF_E_INVALIDMEDIATYPE;
        }

        wil::unique_cotaskmem_array_ptr<wil::com_ptr_nothrow<IMFMediaType>> mediaTypeList = 
            wilEx::make_unique_cotaskmem_array<wil::com_ptr_nothrow<IMFMediaType>>(validMediaTypeCount);
        RETURN_IF_NULL_ALLOC(mediaTypeList.get());

        for (DWORD i = 0; i < validMediaTypeCount; i++)
        {
            mediaTypeList[i] = sourceStreamMediaTypeList[i];
        }

        RETURN_IF_FAILED(MFCreateAttributes(&m_spAttributes, 10));
        RETURN_IF_FAILED(_SetStreamAttributes(m_spAttributes.get()));
        
        wil::com_ptr_nothrow<IMFMediaTypeHandler> spTypeHandler;
        
        // create our stream descriptor
        RETURN_IF_FAILED(MFCreateStreamDescriptor(
            m_dwStreamId /*StreamId*/, 
            validMediaTypeCount /*MT count*/, 
            mediaTypeList.get(), &m_spStreamDesc));
        RETURN_IF_FAILED(m_spStreamDesc->GetMediaTypeHandler(&spTypeHandler));
        RETURN_IF_FAILED(spTypeHandler->SetCurrentMediaType(mediaTypeList[0]));

        // set same attributes on stream descriptor as on stream attribute store
        RETURN_IF_FAILED(_SetStreamAttributes(m_spStreamDesc.get())); 

        RETURN_IF_FAILED(MFCreateEventQueue(&m_spEventQueue));
        
        auto ptr = winrt::make_self<CAsyncCallback<AugmentedMediaStream>>(this, &AugmentedMediaStream::OnMediaStreamEvent, m_dwSerialWorkQueueId);
        m_xOnMediaStreamEvent.attach(ptr.detach());

        DEBUG_MSG(L"Initialize exit | stream id: %d | dev source streamId: %d | MediaType count: %u", m_dwStreamId, m_dwDevSourceStreamIdentifier, validMediaTypeCount);
        return S_OK;
    }

    /// <summary>
    /// This is where some of the potential magic happens, where the pixel buffer is processed according to 
    /// the state this virtual camera stream is put into. This can be manipulated for example using 
    /// a custom DDI..
    /// </summary>
    /// <param name="inputSample"></param>
    /// <returns></returns>
    HRESULT AugmentedMediaStream::ProcessSample(_In_ IMFSample* inputSample)
    {
        winrt::slim_lock_guard lock(m_Lock);

        // ----> Do custom processing on the sample from the physical camera
        /*

        wil::com_ptr_nothrow<IMFMediaBuffer> spBuffer;
        RETURN_IF_FAILED(inputSample->GetBufferByIndex(0, &spBuffer));

        LONG pitch = 0;
        BYTE* bufferStart = nullptr;
        DWORD bufferLength = 0;
        BYTE* pbuf = nullptr;
        wil::com_ptr_nothrow<IMF2DBuffer2> buffer2D;

        RETURN_IF_FAILED(spBuffer->QueryInterface(IID_PPV_ARGS(&buffer2D)));
        RETURN_IF_FAILED(buffer2D->Lock2DSize(MF2DBuffer_LockFlags_ReadWrite,
            &pbuf,
            &pitch,
            &bufferStart,
            &bufferLength));

        // process pixel buffer..

        RETURN_IF_FAILED(buffer2D->Unlock2D());

        */
        // <--------------------------------------------- end of processing

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

    //////////////////////////////////////////////////////////////////////////////////////////
    // IMFMediaEventGenerator

    IFACEMETHODIMP AugmentedMediaStream::BeginGetEvent(
            _In_ IMFAsyncCallback* pCallback,
            _In_ IUnknown* punkState
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        RETURN_IF_FAILED(m_spEventQueue->BeginGetEvent(pCallback, punkState));

        return S_OK;
    }

    IFACEMETHODIMP AugmentedMediaStream::EndGetEvent(
            _In_ IMFAsyncResult* pResult,
            _COM_Outptr_ IMFMediaEvent** ppEvent
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        RETURN_IF_FAILED(m_spEventQueue->EndGetEvent(pResult, ppEvent));

        return S_OK;
    }

    IFACEMETHODIMP AugmentedMediaStream::GetEvent(
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

    IFACEMETHODIMP AugmentedMediaStream::QueueEvent(
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

    //////////////////////////////////////////////////////////////////////////////////////////
    // IMFMediaStream

    IFACEMETHODIMP AugmentedMediaStream::GetMediaSource(
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

    IFACEMETHODIMP AugmentedMediaStream::GetStreamDescriptor(
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

    IFACEMETHODIMP AugmentedMediaStream::RequestSample(
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
    IFACEMETHODIMP AugmentedMediaStream::SetStreamState(MF_STREAM_STATE state)
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        DEBUG_MSG(L"[%d] SetStreamState %d", m_dwStreamId, state);

        wil::com_ptr_nothrow<IMFMediaStream2> spStream2;
        RETURN_IF_FAILED(m_spDevStream->QueryInterface(IID_PPV_ARGS(&spStream2)));
        RETURN_IF_FAILED(spStream2->SetStreamState(state));

        return S_OK;
    }

    IFACEMETHODIMP AugmentedMediaStream::GetStreamState(_Out_ MF_STREAM_STATE* pState)
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_HR_IF_NULL(E_POINTER, pState);
        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        wil::com_ptr_nothrow<IMFMediaStream2> spStream2;
        RETURN_IF_FAILED(m_spDevStream->QueryInterface(IID_PPV_ARGS(&spStream2)));
        RETURN_IF_FAILED(spStream2->GetStreamState(pState));

        return S_OK;
    }

    HRESULT AugmentedMediaStream::OnMediaStreamEvent(_In_ IMFAsyncResult* pResult)
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
        DEBUG_MSG(L"[%d] OnMediaStreamEvent, streamId: %d, met:%d ", m_dwStreamId, met);
        
        // This shows how to intercept sample from physical camera
        // and do custom processing on the sample.
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
        else if (met == MEStreamStarted)
        {
            RETURN_IF_FAILED(SetStreamState(MF_STREAM_STATE_RUNNING));
            RETURN_IF_FAILED(Start());
        }
        else if (met == MEStreamStopped)
        {
            RETURN_IF_FAILED(Stop());
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
        
        DEBUG_MSG(L"[%d] OnMediaStreamEvent exit", m_dwStreamId);
        return S_OK;
    }

    HRESULT AugmentedMediaStream::Shutdown()
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
        
        m_spAttributes.reset();

        return hr;
    }

    HRESULT AugmentedMediaStream::SetMediaStream(_In_ IMFMediaStream* pMediaStream)
    {
        winrt::slim_lock_guard lock(m_Lock);
        DEBUG_MSG(L"[%d] Set MediaStream %p ", m_dwStreamId, pMediaStream);

        RETURN_HR_IF_NULL(E_INVALIDARG, pMediaStream);
        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        m_spDevStream = pMediaStream;
        RETURN_IF_FAILED(m_spDevStream->BeginGetEvent(m_xOnMediaStreamEvent.get(), m_spDevStream.get()));

        return S_OK;
    }

    HRESULT AugmentedMediaStream::Start()
    {
        winrt::slim_lock_guard lock(m_Lock);

        wil::com_ptr_nothrow<IMFMediaTypeHandler> spMTHandler;
        RETURN_IF_FAILED(m_spStreamDesc->GetMediaTypeHandler(&spMTHandler));

        wil::com_ptr_nothrow<IMFMediaType> spMediaType;

        GUID subType;
        RETURN_IF_FAILED(spMTHandler->GetCurrentMediaType(&spMediaType));
        RETURN_IF_FAILED(spMediaType->GetGUID(MF_MT_SUBTYPE, &subType));
        MFGetAttributeSize(spMediaType.get(), MF_MT_FRAME_SIZE, &m_width, &m_height);

        DEBUG_MSG(L"AugmentedMediaStream Start().. with mediatype: %s, %dx%d ", winrt::to_hstring(subType).data(), m_width, m_height);

        // Post MEStreamStarted event to signal stream has started 
        RETURN_IF_FAILED(m_spEventQueue->QueueEventParamVar(MEStreamStarted, GUID_NULL, S_OK, nullptr));

        return S_OK;
    }

    HRESULT AugmentedMediaStream::Stop()
    {
        winrt::slim_lock_guard lock(m_Lock);

        // Post MEStreamStopped event to signal stream has stopped
        RETURN_IF_FAILED(m_spEventQueue->QueueEventParamVar(MEStreamStopped, GUID_NULL, S_OK, nullptr));
        return S_OK;
    }

    HRESULT AugmentedMediaStream::_CheckShutdownRequiresLock()
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

    HRESULT AugmentedMediaStream::_SetStreamAttributes(
        _In_ IMFAttributes* pAttributeStore
    )
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, pAttributeStore);

        RETURN_IF_FAILED(pAttributeStore->SetGUID(MF_DEVICESTREAM_STREAM_CATEGORY, PINNAME_VIDEO_CAPTURE));
        RETURN_IF_FAILED(pAttributeStore->SetUINT32(MF_DEVICESTREAM_STREAM_ID, m_dwStreamId));
        RETURN_IF_FAILED(pAttributeStore->SetUINT32(MF_DEVICESTREAM_FRAMESERVER_SHARED, 1));
        RETURN_IF_FAILED(pAttributeStore->SetUINT32(MF_DEVICESTREAM_ATTRIBUTE_FRAMESOURCE_TYPES, MFFrameSourceTypes::MFFrameSourceTypes_Color));

        return S_OK;
    }

    
}

