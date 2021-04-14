//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#include "pch.h"
#include "SimpleFrameGenerator.h"

#define NUM_IMAGE_ROWS 480
#define NUM_IMAGE_COLS 640
#define BYTES_PER_PIXEL 4
#define IMAGE_BUFFER_SIZE_BYTES (NUM_IMAGE_ROWS * NUM_IMAGE_COLS * BYTES_PER_PIXEL)
#define IMAGE_ROW_SIZE_BYTES (NUM_IMAGE_COLS * BYTES_PER_PIXEL)

namespace winrt::WindowsSample::implementation
{
    HRESULT SimpleMediaStream::Initialize(
            _In_ SimpleMediaSource* pSource,
            _In_ DWORD dwStreamId,
            _In_ MFSampleAllocatorUsage allocatorUsage
        )
    {
        wil::com_ptr_nothrow<IMFMediaTypeHandler> spTypeHandler;
        wil::com_ptr_nothrow<IMFAttributes> attrs;

        RETURN_HR_IF_NULL(E_INVALIDARG, pSource);
        _parent = pSource;

        m_dwStreamId = dwStreamId;
        m_allocatorUsage = allocatorUsage;

        const uint32_t NUM_MEDIATYPES = 2;
        wil::unique_cotaskmem_array_ptr<wil::com_ptr_nothrow<IMFMediaType>> m_mediaTypeList = wilEx::make_unique_cotaskmem_array<wil::com_ptr_nothrow<IMFMediaType>>(NUM_MEDIATYPES);

        // Initialize media type and set the video output media type.
        wil::com_ptr_nothrow<IMFMediaType> spMediaType0;
        RETURN_IF_FAILED(MFCreateMediaType(&spMediaType0));
        spMediaType0->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        spMediaType0->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12);
        spMediaType0->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        spMediaType0->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
        MFSetAttributeSize(spMediaType0.get(), MF_MT_FRAME_SIZE, NUM_IMAGE_COLS, NUM_IMAGE_ROWS);
        MFSetAttributeRatio(spMediaType0.get(), MF_MT_FRAME_RATE, 30, 1);
        // frame size * pixle bit size * framerate
        uint32_t bitrate = NUM_IMAGE_COLS * 1.5 * NUM_IMAGE_ROWS * 8* 30;
        spMediaType0->SetUINT32(MF_MT_AVG_BITRATE, bitrate);
        MFSetAttributeRatio(spMediaType0.get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
        m_mediaTypeList[0] = spMediaType0.detach();

        wil::com_ptr_nothrow<IMFMediaType> spMediaType1;
        RETURN_IF_FAILED(MFCreateMediaType(&spMediaType1));
        spMediaType1->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
        spMediaType1->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
        spMediaType1->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
        spMediaType1->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
        MFSetAttributeSize(spMediaType1.get(), MF_MT_FRAME_SIZE, NUM_IMAGE_COLS, NUM_IMAGE_ROWS);
        MFSetAttributeRatio(spMediaType1.get(), MF_MT_FRAME_RATE, 30, 1);
        // frame size * pixle bit size * framerate
         bitrate = NUM_IMAGE_COLS * NUM_IMAGE_ROWS * 24* 30;
         spMediaType1->SetUINT32(MF_MT_AVG_BITRATE, bitrate);
        MFSetAttributeRatio(spMediaType1.get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
        m_mediaTypeList[1] = spMediaType1.detach();

        RETURN_IF_FAILED(MFCreateAttributes(&_spAttributes, 10));
        RETURN_IF_FAILED(_SetStreamAttributes(_spAttributes.get()));

        RETURN_IF_FAILED(MFCreateEventQueue(&_spEventQueue));

        // Initialize stream descriptors
        RETURN_IF_FAILED(MFCreateStreamDescriptor(m_dwStreamId /*StreamId*/, NUM_MEDIATYPES /*MT count*/, m_mediaTypeList.get(), &_spStreamDesc));

        RETURN_IF_FAILED(_spStreamDesc->GetMediaTypeHandler(&spTypeHandler));
        RETURN_IF_FAILED(spTypeHandler->SetCurrentMediaType(m_mediaTypeList[0]));
        RETURN_IF_FAILED(_SetStreamDescriptorAttributes(_spStreamDesc.get()));

        return S_OK;
    }

    // IMFMediaEventGenerator
    IFACEMETHODIMP SimpleMediaStream::BeginGetEvent(
            _In_ IMFAsyncCallback* pCallback,
            _In_ IUnknown* punkState
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        RETURN_IF_FAILED(_spEventQueue->BeginGetEvent(pCallback, punkState));

        return S_OK;
    }

    IFACEMETHODIMP SimpleMediaStream::EndGetEvent(
            _In_ IMFAsyncResult* pResult,
            _COM_Outptr_ IMFMediaEvent** ppEvent
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        RETURN_IF_FAILED(_spEventQueue->EndGetEvent(pResult, ppEvent));

        return S_OK;
    }

    IFACEMETHODIMP SimpleMediaStream::GetEvent(
            _In_ DWORD dwFlags,
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

    IFACEMETHODIMP SimpleMediaStream::QueueEvent(
            _In_ MediaEventType eventType,
            _In_ REFGUID guidExtendedType,
            _In_ HRESULT hrStatus,
            _In_opt_ PROPVARIANT const* pvValue
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        RETURN_IF_FAILED(_spEventQueue->QueueEventParamVar(eventType, guidExtendedType, hrStatus, pvValue));

        return S_OK;
    }

    // IMFMediaStream
    IFACEMETHODIMP SimpleMediaStream::GetMediaSource(
            _COM_Outptr_ IMFMediaSource** ppMediaSource
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_HR_IF_NULL(E_POINTER, ppMediaSource);
        *ppMediaSource = nullptr;

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        RETURN_IF_FAILED(_parent.copy_to(ppMediaSource));

        return S_OK;
    }

    IFACEMETHODIMP SimpleMediaStream::GetStreamDescriptor(
            _COM_Outptr_ IMFStreamDescriptor** ppStreamDescriptor
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_HR_IF_NULL(E_POINTER, ppStreamDescriptor);
        *ppStreamDescriptor = nullptr;

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        if (_spStreamDesc != nullptr)
        {
            RETURN_IF_FAILED(_spStreamDesc.copy_to(ppStreamDescriptor));
        }
        else
        {
            return E_UNEXPECTED;
        }

        return S_OK;
    }

    //
    //HRESULT SimpleMediaStream::WriteSampleData(
    //        _Inout_updates_bytes_(len) BYTE* pBuf,
    //        _In_ DWORD len, 
    //        _In_ LONG pitch,
    //        _In_ DWORD width, 
    //        _In_ DWORD height
    //    )
    //{
    //    RETURN_HR_IF_NULL(E_INVALIDARG, pBuf);

    //    if (IsEqualGUID(m_gCurrentSubType, MFVideoFormat_RGB32))
    //    {
    //        DEBUG_MSG(L"[SimpleMediaSource] RGB32 frames %s\n", winrt::to_hstring(MFVideoFormat_RGB32).data());

    //        RETURN_IF_FAILED(RGB32Frame(pBuf, len, pitch, width, height, m_rgbMask));
    //        //NUM_ROWS = len / abs(pitch);
    //    }
    //    else
    //    {
    //        DEBUG_MSG(L"[SimpleMediaSource] NV12 frames %s \n", winrt::to_hstring(MFVideoFormat_NV12).data());

    //        DWORD frameBuffLen = width * height * 4;
    //        wil::unique_cotaskmem_ptr<BYTE[]> spBuff = wil::make_unique_cotaskmem_nothrow<BYTE[]>(frameBuffLen);
    //        RETURN_IF_NULL_ALLOC(spBuff.get());

    //        RETURN_IF_FAILED(RGB32Frame(spBuff.get(), frameBuffLen, width*4, width, height, m_rgbMask));
    //        RETURN_IF_FAILED(RGB32ToNV12(spBuff.get(), frameBuffLen, width*4, width, height, pBuf, len, pitch));
    //    }

    //    return S_OK;
    //}

    IFACEMETHODIMP SimpleMediaStream::RequestSample(
            _In_ IUnknown* pToken
        )
    {
        winrt::slim_lock_guard lock(m_Lock);
        wil::com_ptr_nothrow<IMFSample> sample;
        wil::com_ptr_nothrow<IMFMediaBuffer> outputBuffer;
        LONG pitch = 0;
        BYTE* bufferStart = nullptr; // not used
        DWORD bufferLength = 0;
        BYTE* pbuf = nullptr;
        wil::com_ptr_nothrow<IMF2DBuffer2> buffer2D;

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        if (m_streamState != MF_STREAM_STATE_RUNNING)
        {
            RETURN_HR_MSG(MF_E_INVALIDREQUEST, "Stream is not in running state, state:%d", m_streamState);
        }

        RETURN_IF_FAILED(m_spSampleAllocator->AllocateSample(&sample));
        RETURN_IF_FAILED(sample->GetBufferByIndex(0, &outputBuffer));
        RETURN_IF_FAILED(outputBuffer.query_to(&buffer2D));
        RETURN_IF_FAILED(buffer2D->Lock2DSize(MF2DBuffer_LockFlags_Write,
            &pbuf,
            &pitch,
            &bufferStart,
            &bufferLength));
        

        RETURN_IF_FAILED(m_spFrameGenerator->CreateFrame(pbuf, bufferLength, pitch, m_rgbMask));
        //RETURN_IF_FAILED(WriteSampleData(pbuf, bufferLength, pitch, NUM_IMAGE_COLS, NUM_IMAGE_ROWS));
        RETURN_IF_FAILED(buffer2D->Unlock2D());

        RETURN_IF_FAILED(sample->SetSampleTime(MFGetSystemTime()));
        RETURN_IF_FAILED(sample->SetSampleDuration(333333));
        if (pToken != nullptr)
        {
            RETURN_IF_FAILED(sample->SetUnknown(MFSampleExtension_Token, pToken));
        }
        RETURN_IF_FAILED(_spEventQueue->QueueEventParamUnk(MEMediaSample,
            GUID_NULL,
            S_OK,
            sample.get()));

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    // IMFMediaStream2
    IFACEMETHODIMP SimpleMediaStream::SetStreamState(MF_STREAM_STATE state)
    {
        winrt::slim_lock_guard lock(m_Lock);
        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        switch (state)
        {
        case MF_STREAM_STATE_PAUSED:
            // because not supported
            break;

        case MF_STREAM_STATE_RUNNING:
            m_streamState = state;
            break;

        case MF_STREAM_STATE_STOPPED:
            m_streamState = state;

            break;

        default:
            return MF_E_INVALID_STATE_TRANSITION;
            break;
        }

        return S_OK;
    }

    IFACEMETHODIMP SimpleMediaStream::GetStreamState(
            _Out_ MF_STREAM_STATE* pState
        )
    {
        winrt::slim_lock_guard lock(m_Lock);
        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        *pState = m_streamState;

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    // Public methods
    HRESULT SimpleMediaStream::Start()
    {
        winrt::slim_lock_guard lock(m_Lock);

        // Create the allocator if one doesn't exist
        if (m_allocatorUsage == MFSampleAllocatorUsage_UsesProvidedAllocator)
        {
            RETURN_HR_IF_NULL_MSG(E_POINTER, m_spSampleAllocator, "Sample allocator is not set");
        }
        else
        {
            RETURN_IF_FAILED(MFCreateVideoSampleAllocatorEx(IID_PPV_ARGS(&m_spSampleAllocator)));
        }

        wil::com_ptr_nothrow<IMFMediaTypeHandler> spMTHandler;
        RETURN_IF_FAILED(_spStreamDesc->GetMediaTypeHandler(&spMTHandler));

        wil::com_ptr_nothrow<IMFMediaType> spMediaType;
        UINT32 width, height;
        GUID subType;
        RETURN_IF_FAILED(spMTHandler->GetCurrentMediaType(&spMediaType));
        RETURN_IF_FAILED(spMediaType->GetGUID(MF_MT_SUBTYPE, &subType));
        MFGetAttributeSize(spMediaType.get(), MF_MT_FRAME_SIZE, &width, &height);

        DEBUG_MSG(L"Initialize sample allocator for mediatype: %s, %dx%d ", winrt::to_hstring(subType).data(), width, height);
        RETURN_IF_FAILED(m_spSampleAllocator->InitializeSampleAllocator(10, spMediaType.get()));
        if (m_spFrameGenerator == nullptr)
        {
            m_spFrameGenerator = wil::make_unique_nothrow<SimpleFrameGenerator>();
            RETURN_IF_NULL_ALLOC_MSG(m_spFrameGenerator, "Fail to create SimpleFrameGenerator");
        }
        RETURN_IF_FAILED(m_spFrameGenerator->Initialize(spMediaType.get()));

        // Post MEStreamStarted event to signal stream has started 
        RETURN_IF_FAILED(_spEventQueue->QueueEventParamVar(MEStreamStarted, GUID_NULL, S_OK, nullptr));

        return S_OK;
    }

    HRESULT SimpleMediaStream::Stop()
    {
        winrt::slim_lock_guard lock(m_Lock);

        // Post MEStreamStopped event to signal stream has stopped
        RETURN_IF_FAILED(_spEventQueue->QueueEventParamVar(MEStreamStopped, GUID_NULL, S_OK, nullptr));
        return S_OK;
    }

    HRESULT SimpleMediaStream::Shutdown()
    {
        winrt::slim_lock_guard lock(m_Lock);

        _isShutdown = true;
        _parent.reset();

        if (_spEventQueue != nullptr)
        {
            _spEventQueue->Shutdown();
            _spEventQueue.reset();
        }

        _spAttributes.reset();
        _spStreamDesc.reset();

        m_streamState = MF_STREAM_STATE_STOPPED;

        return S_OK;
    }

    HRESULT SimpleMediaStream::SetSampleAllocator(IMFVideoSampleAllocator* pAllocator)
    {
        winrt::slim_lock_guard lock(m_Lock);

        if (m_streamState == MF_STREAM_STATE_RUNNING)
        {
            RETURN_HR_MSG(MF_E_INVALIDREQUEST, "Cannot update allocator when the stream is streaming");
        }
        m_spSampleAllocator.reset();
        m_spSampleAllocator = pAllocator;

        return S_OK;
    }

    
    //////////////////////////////////////////////////////////////////////////////////////////
    // Private methods

    HRESULT SimpleMediaStream::_CheckShutdownRequiresLock()
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

    HRESULT SimpleMediaStream::_SetStreamAttributes(
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

    HRESULT SimpleMediaStream::_SetStreamDescriptorAttributes(
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