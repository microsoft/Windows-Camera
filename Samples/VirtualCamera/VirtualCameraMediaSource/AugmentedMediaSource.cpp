//
// Copyright (C) Microsoft Corporation. All rights reserved.
//
#include "pch.h"
#include "wtsapi32.h"
#include "userenv.h"

namespace winrt::WindowsSample::implementation
{
    AugmentedMediaSource::~AugmentedMediaSource()
    {
        Shutdown();
        if (m_dwSerialWorkQueueId != 0)
        {
            (void)MFUnlockWorkQueue(m_dwSerialWorkQueueId);
            m_dwSerialWorkQueueId = 0;
        }
    }
    
    /// <summary>
    /// /// Initialize the source. This is where we wrap an existing camera and filter its exposed set of streams.
    /// </summary>
    /// <param name="pAttributes"></param>
    /// <param name="pMediaSource"></param>
    /// <returns></returns>
    HRESULT AugmentedMediaSource::Initialize(_In_ IMFAttributes* pAttributes, _In_ IMFMediaSource* pMediaSource)
    {
        winrt::slim_lock_guard lock(m_Lock);
        if (m_initalized)
        {
            return MF_E_ALREADY_INITIALIZED;
        }

        DEBUG_MSG(L"AugmentedMediaSource Initialize enter");

        RETURN_HR_IF_NULL(E_POINTER, pMediaSource);
        m_spDevSource = pMediaSource;

        // 1. Populate source attributes like profile and control app
        RETURN_IF_FAILED(_CreateSourceAttributes(pAttributes));

        m_streamList = wilEx::make_unique_cotaskmem_array<wil::com_ptr_nothrow<AugmentedMediaStream>>(NUM_STREAMS);
        RETURN_IF_NULL_ALLOC(m_streamList.get());

        wil::unique_cotaskmem_array_ptr<wil::com_ptr_nothrow<IMFStreamDescriptor>> streamDescriptorList = 
            wilEx::make_unique_cotaskmem_array<wil::com_ptr_nothrow<IMFStreamDescriptor>>(NUM_STREAMS);
        RETURN_IF_NULL_ALLOC(streamDescriptorList.get());

        // 2. create event queue and register callbacks..
        DEBUG_MSG(L"Create and register event queue");
        RETURN_IF_FAILED(MFCreateEventQueue(&m_spEventQueue));
        RETURN_IF_FAILED(MFAllocateSerialWorkQueue(MFASYNC_CALLBACK_QUEUE_MULTITHREADED, &m_dwSerialWorkQueueId));

        auto ptr = winrt::make_self<CAsyncCallback<AugmentedMediaSource>>(this, &AugmentedMediaSource::OnMediaSourceEvent, m_dwSerialWorkQueueId);
        m_xOnMediaSourceEvent.attach(ptr.detach());
        RETURN_IF_FAILED(m_spDevSource->BeginGetEvent(m_xOnMediaSourceEvent.get(), m_spDevSource.get()));

        // 3. Create presentation descriptors
        RETURN_IF_FAILED(m_spDevSource->CreatePresentationDescriptor(&m_spDevSourcePDesc));

        DWORD dwStreamCount = 0;
        RETURN_IF_FAILED(m_spDevSourcePDesc->GetStreamDescriptorCount(&dwStreamCount));

        // walk through each stream to find the preview stream we want to wrap and filter out the rest of the streams
        ULONG ulVideoPreviewStreamId = 0;
        BOOL hasPreviewPin = false;
        BOOL hasVideoStreamPin = false;
        for (DWORD i = 0; i < dwStreamCount || !hasVideoStreamPin; i++)
        {
            DEBUG_MSG(L"Looking into stream number %u to see if it is a preview stream", i);
            wil::com_ptr_nothrow<IMFStreamDescriptor> spStreamDesc;
            BOOL selected = FALSE;
            RETURN_IF_FAILED(m_spDevSourcePDesc->GetStreamDescriptorByIndex(i, &selected, &spStreamDesc));

            ULONG  ulStreamId = 0;
            UINT32  ulFrameSourceType = 0;
            GUID streamCategory = GUID_NULL;
            RETURN_IF_FAILED(spStreamDesc->GetStreamIdentifier(&ulStreamId));
            RETURN_IF_FAILED(spStreamDesc->GetGUID(MF_DEVICESTREAM_STREAM_CATEGORY, &streamCategory));
            RETURN_IF_FAILED(spStreamDesc->GetUINT32(MF_DEVICESTREAM_ATTRIBUTE_FRAMESOURCE_TYPES, &ulFrameSourceType));
            DEBUG_MSG(L"stream number %u has the stream id=%u | is of stream category={%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX} | is of color type=%u, ", 
                i, 
                ulStreamId, 
                streamCategory.Data1, streamCategory.Data2, streamCategory.Data3, streamCategory.Data4[0], streamCategory.Data4[1], streamCategory.Data4[2], streamCategory.Data4[3],
                                                                                  streamCategory.Data4[4], streamCategory.Data4[5], streamCategory.Data4[6], streamCategory.Data4[7],
                ulFrameSourceType);

            if (ulFrameSourceType == MFFrameSourceTypes::MFFrameSourceTypes_Color)
            {
                DEBUG_MSG(L"stream number %u is of color type", i);
                if (IsEqualGUID(streamCategory, PINNAME_VIDEO_CAPTURE))
                {
                    hasVideoStreamPin = true;
                    m_desiredStreamIdx = i;
                    break;
                }
                else if (IsEqualGUID(streamCategory, PINNAME_VIDEO_PREVIEW))
                {
                    ulVideoPreviewStreamId = i;
                    hasPreviewPin = true;
                }
            }
        }
        if (!hasVideoStreamPin && hasPreviewPin)
        {
            m_desiredStreamIdx = ulVideoPreviewStreamId;
            hasVideoStreamPin = true;
        }
        // if we have not found a preview stream, bail!
        if (!hasVideoStreamPin)
        {
            return MF_E_CAPTURE_SOURCE_NO_VIDEO_STREAM_PRESENT;
        }

        // initialize the preview stream descriptor..
        DEBUG_MSG(L"Initialize preview stream");
        wil::com_ptr_nothrow<IMFStreamDescriptor> spStreamDesc;
        BOOL selected = FALSE;
        RETURN_IF_FAILED(m_spDevSourcePDesc->GetStreamDescriptorByIndex(m_desiredStreamIdx, &selected, &spStreamDesc));
        auto ptr2 = winrt::make_self<AugmentedMediaStream>();
        m_streamList[0] = ptr2.detach();
        RETURN_IF_FAILED(m_streamList[0]->Initialize(this, 0, spStreamDesc.get(), m_dwSerialWorkQueueId));
        RETURN_IF_FAILED(m_streamList[0]->GetStreamDescriptor(&streamDescriptorList[0]));
        m_streamList[0]->m_isCustomFXEnabled = m_isCustomFXEnabled;

        RETURN_IF_FAILED(MFCreatePresentationDescriptor((DWORD)m_streamList.size(), streamDescriptorList.get(), &m_spPresentationDescriptor));

        m_sourceState = SourceState::Stopped;
        DEBUG_MSG(L"Initialize exit");
        m_initalized = true;

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    // IMFMediaSource methods

    /// <summary>
    /// Start the source. 
    /// This is where we intercept if the source is started and open an RPC communication channel with control app.
    /// </summary>
    /// <param name="pPresentationDescriptor"></param>
    /// <param name="pguidTimeFormat"></param>
    /// <param name="pvarStartPos"></param>
    /// <returns></returns>
    IFACEMETHODIMP AugmentedMediaSource::Start(
        _In_ IMFPresentationDescriptor* pPresentationDescriptor,
        _In_opt_ const GUID* pguidTimeFormat,
        _In_ const PROPVARIANT* pvarStartPos
    )
    {
        winrt::slim_lock_guard lock(m_Lock);

        // create a named pipe to communicate with the virtual camera control app
        auto h_pipe = CreateFile(
            L"\\\\.\\pipe\\pipe\\ContosoVCamPipeStart",
            GENERIC_WRITE,
            0,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL);

        DWORD count = 0;
        wil::unique_prop_variant startTime;

        if (pPresentationDescriptor == nullptr || pvarStartPos == nullptr)
        {
            return E_INVALIDARG;
        }

        if (pguidTimeFormat != nullptr && *pguidTimeFormat != GUID_NULL)
        {
            return MF_E_UNSUPPORTED_TIME_FORMAT;
        }

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        if (m_sourceState == SourceState::Invalid)
        {
            RETURN_IF_FAILED(MF_E_INVALID_STATE_TRANSITION);
        }
        m_sourceState = SourceState::Started;

        // This validates that the PresentationDescriptor passed as argument matches the member of streams we
        // have defined internally
        RETURN_IF_FAILED(_ValidatePresentationDescriptor(pPresentationDescriptor));
        RETURN_IF_FAILED(pPresentationDescriptor->GetStreamDescriptorCount(&count));
        RETURN_IF_FAILED(InitPropVariantFromInt64(MFGetSystemTime(), &startTime));

        // Update devicesource presentation descriptor for selected and deselected stream(s)
        BOOL selected = false;
        bool wasSelected = false;
        DWORD streamIdx = 0;
        DWORD streamId = 0;
        for (unsigned int i = 0; i < count; i++)
        {
            wil::com_ptr_nothrow<IMFStreamDescriptor> streamDesc;
            RETURN_IF_FAILED(pPresentationDescriptor->GetStreamDescriptorByIndex(
                i,
                &selected,
                &streamDesc));

            RETURN_IF_FAILED(streamDesc->GetStreamIdentifier(&streamId));
            
            wil::com_ptr_nothrow<IMFStreamDescriptor> spLocalStreamDescriptor;
            RETURN_IF_FAILED(_GetStreamDescriptorByStreamId(streamId, &streamIdx, &wasSelected, &spLocalStreamDescriptor));

            if (wasSelected && !selected)
            {
                // stream was previously selected but not selected this time.
                RETURN_IF_FAILED(m_spPresentationDescriptor->DeselectStream(streamIdx));
                RETURN_IF_FAILED(m_spDevSourcePDesc->DeselectStream(m_desiredStreamIdx));
            }
        }
        for (unsigned int i = 0; i < count; i++)
        {
            wil::com_ptr_nothrow<IMFStreamDescriptor> streamDesc;
            RETURN_IF_FAILED(pPresentationDescriptor->GetStreamDescriptorByIndex(
                i,
                &selected,
                &streamDesc));

            RETURN_IF_FAILED(streamDesc->GetStreamIdentifier(&streamId));

            wil::com_ptr_nothrow<IMFStreamDescriptor> spLocalStreamDescriptor;
            RETURN_IF_FAILED(_GetStreamDescriptorByStreamId(streamId, &streamIdx, &wasSelected, &spLocalStreamDescriptor));
            if (selected)
            {
                if (!wasSelected)
                {
                    DEBUG_MSG(L"Selected stream Id: %d", streamIdx);
                    // Update our internal PresentationDescriptor
                    RETURN_IF_FAILED(m_spPresentationDescriptor->SelectStream(streamIdx));

                    // Update the source camera descriptor we proper stream
                    RETURN_IF_FAILED(m_spDevSourcePDesc->SelectStream(m_desiredStreamIdx));
                }

                BOOL selected2 = FALSE;
                wil::com_ptr_nothrow<IMFStreamDescriptor> spDevStreamDescriptor;
                RETURN_IF_FAILED(m_spDevSourcePDesc->GetStreamDescriptorByIndex(m_desiredStreamIdx, &selected2, &spDevStreamDescriptor));
                wil::com_ptr_nothrow<IMFMediaTypeHandler> spDevSourceStreamMediaTypeHandler;
                RETURN_IF_FAILED(spDevStreamDescriptor->GetMediaTypeHandler(&spDevSourceStreamMediaTypeHandler));

                wil::com_ptr_nothrow<IMFMediaTypeHandler> spStreamMediaTypeHandler;
                RETURN_IF_FAILED(streamDesc->GetMediaTypeHandler(&spStreamMediaTypeHandler));

                // debug what is set on the virtual camera
                wil::com_ptr_nothrow<IMFMediaType> spCurrentMediaType;
                RETURN_IF_FAILED(spStreamMediaTypeHandler->GetCurrentMediaType(&spCurrentMediaType));
                {
                    GUID majorType;
                    RETURN_IF_FAILED(spCurrentMediaType->GetGUID(MF_MT_MAJOR_TYPE, &majorType));

                    GUID subtype;
                    RETURN_IF_FAILED(spCurrentMediaType->GetGUID(MF_MT_SUBTYPE, &subtype));

                    UINT width = 0, height = 0;
                    RETURN_IF_FAILED(MFGetAttributeSize(spCurrentMediaType.get(), MF_MT_FRAME_SIZE, &width, &height));

                    UINT numerator = 0, denominator = 0;
                    RETURN_IF_FAILED(MFGetAttributeRatio(spCurrentMediaType.get(), MF_MT_FRAME_RATE, &numerator, &denominator));
                }

                RETURN_IF_FAILED(spDevSourceStreamMediaTypeHandler->SetCurrentMediaType(spCurrentMediaType.get()));
            }
        }
        RETURN_IF_FAILED(m_spDevSource->Start(m_spDevSourcePDesc.get(), pguidTimeFormat, pvarStartPos));

        // Send event that the source started. Include error code in case it failed.
        RETURN_IF_FAILED(m_spEventQueue->QueueEventParamVar(
            MESourceStarted,
            GUID_NULL,
            S_OK,
            &startTime));

        return S_OK;
    }

    IFACEMETHODIMP AugmentedMediaSource::CreatePresentationDescriptor(
        _COM_Outptr_ IMFPresentationDescriptor** ppPresentationDescriptor
    )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_HR_IF_NULL(E_POINTER, ppPresentationDescriptor);
        *ppPresentationDescriptor = nullptr;

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        RETURN_IF_FAILED(m_spPresentationDescriptor->Clone(ppPresentationDescriptor));

        return S_OK;
    }

    IFACEMETHODIMP AugmentedMediaSource::GetCharacteristics(
        _Out_ DWORD* pdwCharacteristics
    )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_HR_IF_NULL(E_POINTER, pdwCharacteristics);
        *pdwCharacteristics = 0;

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        *pdwCharacteristics = MFMEDIASOURCE_IS_LIVE;

        return S_OK;
    }

    IFACEMETHODIMP AugmentedMediaSource::Pause()
    {
        // Pause() not required/needed
        return MF_E_INVALID_STATE_TRANSITION;
    }


    IFACEMETHODIMP AugmentedMediaSource::Shutdown()
    {
        winrt::slim_lock_guard lock(m_Lock);

        m_sourceState = SourceState::Shutdown;

        m_spDevSourcePDesc.reset();
        if (m_spDevSource != nullptr)
        {
            m_spDevSource->Shutdown();
            m_spDevSource = nullptr;
        }

        m_spAttributes.reset();
        
        for (unsigned int i = 0; i < m_streamList.size(); i++)
        {
            if (m_streamList[i] != nullptr)
            {
                m_streamList[i]->Shutdown();
            }
        }
        m_streamList.reset();
        (void)m_spPresentationDescriptor.detach(); // calling reset() fails due to the stream also calling reset on its stream descriptor..
        
        if (m_spEventQueue != nullptr)
        {
            m_spEventQueue->Shutdown();
            m_spEventQueue.reset();
        }

        return S_OK;
    }

    _Requires_lock_held_(m_Lock)
    HRESULT AugmentedMediaSource::_GetStreamDescriptorByStreamId(
        _In_ DWORD dwStreamId, 
        _Out_ DWORD* pdwStreamIdx, 
        _Out_ bool* pSelected, 
        _COM_Outptr_ IMFStreamDescriptor** ppStreamDescriptor)
    {
        RETURN_HR_IF_NULL(E_POINTER, ppStreamDescriptor);
        *ppStreamDescriptor = nullptr;

        RETURN_HR_IF_NULL(E_POINTER, pdwStreamIdx);
        *pdwStreamIdx = 0;

        RETURN_HR_IF_NULL(E_POINTER, pSelected);
        *pSelected = false;

        DWORD streamCount = 0;
        RETURN_IF_FAILED(m_spPresentationDescriptor->GetStreamDescriptorCount(&streamCount));
        for (unsigned int i = 0; i < streamCount; i++)
        {
            wil::com_ptr_nothrow<IMFStreamDescriptor> spStreamDescriptor;
            BOOL selected = FALSE;

            RETURN_IF_FAILED(m_spPresentationDescriptor->GetStreamDescriptorByIndex(i, &selected, &spStreamDescriptor));

            DWORD id = 0;
            RETURN_IF_FAILED(spStreamDescriptor->GetStreamIdentifier(&id));

            if (dwStreamId == id)
            {
                // Found the streamDescriptor with matching streamId
                *pdwStreamIdx = i;
                *pSelected = !!selected;
                RETURN_IF_FAILED(spStreamDescriptor.copy_to(ppStreamDescriptor));
                return S_OK;
            }
        }

        return MF_E_NOT_FOUND;
    }

    IFACEMETHODIMP AugmentedMediaSource::Stop()
    {
        winrt::slim_lock_guard lock(m_Lock);

        if (m_sourceState != SourceState::Started)
        {
            return MF_E_INVALID_STATE_TRANSITION;
        }
        m_sourceState = SourceState::Stopped;

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        RETURN_IF_FAILED(m_spDevSource->Stop());

        wil::unique_prop_variant stopTime;
        RETURN_IF_FAILED(InitPropVariantFromInt64(MFGetSystemTime(), &stopTime));

        // Deselect the streams and send the stream stopped events.
        for (unsigned int i = 0; i < m_streamList.size(); i++)
        {
            RETURN_IF_FAILED(m_streamList[i]->SetStreamState(MF_STREAM_STATE_STOPPED));
            RETURN_IF_FAILED(m_spPresentationDescriptor->DeselectStream(i));
        }

        RETURN_IF_FAILED(m_spEventQueue->QueueEventParamVar(MESourceStopped, GUID_NULL, S_OK, &stopTime));

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    // IMFMediaSourceEx methods

    IFACEMETHODIMP AugmentedMediaSource::GetSourceAttributes(_COM_Outptr_ IMFAttributes** sourceAttributes)
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_HR_IF_NULL(E_POINTER, sourceAttributes);
        *sourceAttributes = nullptr;

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        RETURN_IF_FAILED(m_spAttributes.copy_to(sourceAttributes));

        return S_OK;
    }

    IFACEMETHODIMP AugmentedMediaSource::GetStreamAttributes(
            _In_ DWORD dwStreamIdentifier,
            _COM_Outptr_ IMFAttributes** ppAttributes
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_HR_IF_NULL(E_POINTER, ppAttributes);
        *ppAttributes = nullptr;

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        // get the stream identifier on the source camera and copy its attributes
        RETURN_IF_FAILED(m_streamList[0]->m_spAttributes.copy_to(ppAttributes));

        return S_OK;
    }

    IFACEMETHODIMP AugmentedMediaSource::SetD3DManager(
            _In_opt_ IUnknown* pManager
        )
    {
        // Return code is ignored by the frame work, this is a
        // best effort attempt to inform the media source of the
        // DXGI manager to use if DX surface support is available.
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        wil::com_ptr_nothrow<IMFMediaSourceEx> spMediaSourceEx;
        RETURN_IF_FAILED(m_spDevSource->QueryInterface(IID_PPV_ARGS(&spMediaSourceEx)));
        RETURN_IF_FAILED(spMediaSourceEx->SetD3DManager(pManager));

        return S_OK;
    }

    // IMFMediaEventGenerator methods.
    IFACEMETHODIMP AugmentedMediaSource::BeginGetEvent(
        _In_ IMFAsyncCallback* pCallback,
        _In_ IUnknown* punkState)
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        RETURN_IF_FAILED(m_spEventQueue->BeginGetEvent(pCallback, punkState));

        return S_OK;
    }

    IFACEMETHODIMP AugmentedMediaSource::EndGetEvent(
        _In_ IMFAsyncResult* pResult,
        _COM_Outptr_ IMFMediaEvent** ppEvent
    )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        RETURN_IF_FAILED(m_spEventQueue->EndGetEvent(pResult, ppEvent));

        return S_OK;
    }

    IFACEMETHODIMP AugmentedMediaSource::GetEvent(
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

    IFACEMETHODIMP AugmentedMediaSource::QueueEvent(
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
    // IMFGetService methods

    IFACEMETHODIMP AugmentedMediaSource::GetService(
            _In_ REFGUID guidService,
            _In_ REFIID riid,
            _Out_ LPVOID* ppvObject
        )
    {
        winrt::slim_lock_guard lock(m_Lock);
        wil::com_ptr_nothrow<IMFGetService> spGetService;

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        if (SUCCEEDED(m_spDevSource->QueryInterface(IID_PPV_ARGS(&spGetService))))
        {
            RETURN_IF_FAILED(spGetService->GetService(guidService, riid, ppvObject));
        }
        else
        {
            return MF_E_UNSUPPORTED_SERVICE;
        }

        return S_OK;
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    // IKsControl methods

    /// <summary>
    /// This is where GET and SET instruction for DDIs such as standard 
    /// extended controls but also custom controls are received and handled.
    /// You may decide to intercept and implement these calls here, or defer the 
    /// DDI call to the source camera driver, or simply return that this DDI is 
    /// not supported.
    /// </summary>
    /// <param name="pProperty"></param>
    /// <param name="ulPropertyLength"></param>
    /// <param name="pPropertyData"></param>
    /// <param name="ulDataLength"></param>
    /// <param name="pBytesReturned"></param>
    /// <returns></returns>
    IFACEMETHODIMP AugmentedMediaSource::KsProperty(
        _In_reads_bytes_(ulPropertyLength) PKSPROPERTY pProperty,
        _In_ ULONG ulPropertyLength,
        _Inout_updates_to_(ulDataLength, *pBytesReturned) LPVOID pPropertyData,
        _In_ ULONG ulDataLength,
        _Out_ ULONG* pBytesReturned
        )
    {
        winrt::slim_lock_guard lock(m_Lock);
        
        HRESULT hr = S_OK;
        RETURN_HR_IF_NULL(E_POINTER, pProperty);

        if (ulPropertyLength < sizeof(KSPROPERTY))
        {
            return E_INVALIDARG;
        }
        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        DEBUG_MSG(L"[enter: Set=%s | id=%u | flags=0x%08x]", 
            winrt::to_hstring(pProperty->Set).data(), 
            pProperty->Id, 
            pProperty->Flags);
        bool isHandled = false;

        // you would do the following to catch if an existing standard extended control is passed instead of the below custom control:
        // if (IsEqualCLSID(pProperty->Set, KSPROPERTYSETID_ExtendedCameraControl)) { switch (pProperty->Id) ... }

        // check if this is our custom control
        if (!IsEqualCLSID(pProperty->Set, PROPSETID_AUGMENTEDMEDIASOURCE_CUSTOMCONTROL) || (pProperty->Id >= KSPROPERTY_AUGMENTEDMEDIASOURCE_CUSTOMCONTROL_END))
        {
            return HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND);
        }
        
        isHandled = true;
        if (pPropertyData == NULL && ulDataLength == 0)
        {
            RETURN_HR_IF_NULL(E_POINTER, pBytesReturned);

            switch (pProperty->Id)
            {
            case KSPROPERTY_AUGMENTEDMEDIASOURCE_CUSTOMCONTROL_CUSTOMFX:
                *pBytesReturned = sizeof(KSPROPERTY_AUGMENTEDMEDIASOURCE_CUSTOMCONTROL_FX);
                break;

            default:
                return HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND);
                break;
            }
            return HRESULT_FROM_WIN32(ERROR_MORE_DATA);
        }
        else
        {
            RETURN_HR_IF_NULL(E_POINTER, pPropertyData);
            RETURN_HR_IF(HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER), ulDataLength == 0);
            RETURN_HR_IF_NULL(E_POINTER, pBytesReturned);

            // validate properyData length
            switch (pProperty->Id)
            {
            case KSPROPERTY_AUGMENTEDMEDIASOURCE_CUSTOMCONTROL_CUSTOMFX:
            {
                if (ulDataLength < sizeof(KSPROPERTY_AUGMENTEDMEDIASOURCE_CUSTOMCONTROL_FX))
                {
                    return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
                }

                // Set operation 
                if (0 != (pProperty->Flags & (KSPROPERTY_TYPE_SET)))
                {
                    DEBUG_MSG(L"Set filter level KSProperty");
                    *pBytesReturned = 0;
                    PKSPROPERTY_AUGMENTEDMEDIASOURCE_CUSTOMCONTROL_FX controlPayload = ((PKSPROPERTY_AUGMENTEDMEDIASOURCE_CUSTOMCONTROL_FX)pPropertyData);

                    m_isCustomFXEnabled = controlPayload->Flags & KSPROPERTY_AUGMENTEDMEDIASOURCE_CUSTOMCONTROL_CUSTOMFX_AUTO;
                    if (m_streamList.get() != nullptr && !m_streamList.empty() && m_streamList[0] != nullptr)
                    {
                        m_streamList[0]->m_isCustomFXEnabled = m_isCustomFXEnabled;
                    }
                }
                // Get operation
                else if (0 != (pProperty->Flags & (KSPROPERTY_TYPE_GET)))
                {
                    DEBUG_MSG(L"Get filter level KSProperty");
                    PKSPROPERTY_AUGMENTEDMEDIASOURCE_CUSTOMCONTROL_FX controlPayload = ((PKSPROPERTY_AUGMENTEDMEDIASOURCE_CUSTOMCONTROL_FX)pPropertyData);
                    controlPayload->Flags = m_isCustomFXEnabled ?
                        KSPROPERTY_AUGMENTEDMEDIASOURCE_CUSTOMCONTROL_CUSTOMFX_AUTO : KSPROPERTY_AUGMENTEDMEDIASOURCE_CUSTOMCONTROL_CUSTOMFX_OFF;
                    controlPayload->Capability = KSPROPERTY_AUGMENTEDMEDIASOURCE_CUSTOMCONTROL_CUSTOMFX_AUTO;
                    controlPayload->Size = sizeof(KSPROPERTY_AUGMENTEDMEDIASOURCE_CUSTOMCONTROL_FX);
                    *pBytesReturned = sizeof(KSPROPERTY_AUGMENTEDMEDIASOURCE_CUSTOMCONTROL_FX);
                }
                else
                {
                    return E_INVALIDARG;
                }
                break;
            }

            default:
                break;
            }
        }

        // defer to source device
        if (!isHandled)
        {
            RETURN_IF_FAILED(_CheckShutdownRequiresLock());
            wil::com_ptr_nothrow<IKsControl> spKsControl;
            if (SUCCEEDED(m_spDevSource->QueryInterface(IID_PPV_ARGS(&spKsControl))))
            {
                hr = spKsControl->KsProperty(pProperty, ulPropertyLength, pPropertyData, ulDataLength, pBytesReturned);
            }
            else
            {
                return E_NOTIMPL;
            }
        }
        

        return hr;
    }

    IFACEMETHODIMP AugmentedMediaSource::KsMethod(
        _In_reads_bytes_(ulMethodLength) PKSMETHOD pMethod,
        _In_ ULONG ulMethodLength,
        _Inout_updates_to_(ulDataLength, *pBytesReturned) LPVOID pMethodData,
        _In_ ULONG ulDataLength,
        _Out_ ULONG* pBytesReturned
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        // defer to source device
        wil::com_ptr_nothrow<IKsControl> spKsControl;
        if (SUCCEEDED(m_spDevSource->QueryInterface(IID_PPV_ARGS(&spKsControl))))
        {
            RETURN_IF_FAILED(spKsControl->KsMethod(pMethod, ulMethodLength, pMethodData, ulDataLength, pBytesReturned));
        }
        else
        {
            return E_NOTIMPL;
        }

        return S_OK;
    }

    IFACEMETHODIMP AugmentedMediaSource::KsEvent(
        _In_reads_bytes_opt_(ulEventLength) PKSEVENT pEvent,
        _In_ ULONG ulEventLength,
        _Inout_updates_to_(ulDataLength, *pBytesReturned) LPVOID pEventData,
        _In_ ULONG ulDataLength,
        _Out_opt_ ULONG* pBytesReturned
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        // defer to source device
        wil::com_ptr_nothrow<IKsControl> spKsControl;
        if (m_spDevSource->QueryInterface(IID_PPV_ARGS(&spKsControl)))
        {
            RETURN_IF_FAILED(spKsControl->KsEvent(pEvent, ulEventLength, pEventData, ulDataLength, pBytesReturned));
        }
        else
        {
            return E_NOTIMPL;
        }
        return S_OK;
    }

    IFACEMETHODIMP AugmentedMediaSource::SetDefaultAllocator(
        _In_  DWORD dwOutputStreamID,
        _In_  IUnknown* pAllocator)
    {
        winrt::slim_lock_guard lock(m_Lock);
        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        wil::com_ptr_nothrow<IMFSampleAllocatorControl> spAllocatorControl;

        RETURN_IF_FAILED(m_spDevSource->QueryInterface(IID_PPV_ARGS(&spAllocatorControl)));
        RETURN_IF_FAILED(spAllocatorControl->SetDefaultAllocator(m_streamList[0]->m_dwDevSourceStreamIdentifier, pAllocator));

        return S_OK;
    }

    IFACEMETHODIMP AugmentedMediaSource::GetAllocatorUsage(
        _In_  DWORD dwOutputStreamID,
        _Out_  DWORD* pdwInputStreamID,
        _Out_  MFSampleAllocatorUsage* peUsage)
    {
        winrt::slim_lock_guard lock(m_Lock);
        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        RETURN_HR_IF_NULL(E_POINTER, pdwInputStreamID);
        RETURN_HR_IF_NULL(E_POINTER, peUsage);

        // If the AugmentedMediaStream::ProcessSample() method does 'in place' transform then it 
        // should report MFSampleAllocatorUsage_DoesNotAllocate. However if it will process and 
        // place into a new sample it should report MFSampleAllocatorUsage_UsesProvidedAllocator
        *peUsage = MFSampleAllocatorUsage_DoesNotAllocate;
        *pdwInputStreamID = m_streamList[0]->m_dwStreamId;

        return S_OK;
    }

    /// Internal methods.
    _Requires_lock_held_(m_Lock)
    HRESULT AugmentedMediaSource::_CheckShutdownRequiresLock()
    {
        if (m_sourceState == SourceState::Shutdown)
        {
            return MF_E_SHUTDOWN;
        }

        if (m_spEventQueue == nullptr || m_streamList.get() == nullptr || m_spDevSource == nullptr)
        {
            return E_UNEXPECTED;
        }

        return S_OK;
    }

    void AugmentedMediaSource::OnMediaSourceEvent(_In_ IMFAsyncResult* pResult)
    {
        HRESULT hr = S_OK;
        MediaEventType met = MEUnknown;

        wil::com_ptr_nothrow<IMFMediaEvent> spEvent;
        wil::com_ptr_nothrow<IUnknown> spUnknown;
        wil::com_ptr_nothrow<IMFMediaSource> spMediaSource;

        DEBUG_MSG(L"OnMediaSourceEvent %p ", pResult);

        winrt::slim_lock_guard lock(m_Lock);

        CHECKNULL_GOTO(pResult, MF_E_UNEXPECTED, done);
        CHECKHR_GOTO(pResult->GetState(&spUnknown), done);
        CHECKHR_GOTO(spUnknown->QueryInterface(IID_PPV_ARGS(&spMediaSource)), done);

        CHECKHR_GOTO(spMediaSource->EndGetEvent(pResult, &spEvent), done);
        CHECKNULL_GOTO(spEvent, MF_E_UNEXPECTED, done);
        CHECKHR_GOTO(spEvent->GetType(&met), done);

        switch (met)
        {
        case MENewStream:
        case MEUpdatedStream:
            CHECKHR_GOTO(OnNewStream(spEvent.get(), met), done);
            break;
        case MESourceStarted:
            CHECKHR_GOTO(OnSourceStarted(spEvent.get()), done);
            break;
        case MESourceStopped:
            CHECKHR_GOTO(OnSourceStopped(spEvent.get()), done);
            break;
        default:
            // Forward all device source event out.
            CHECKHR_GOTO(m_spEventQueue->QueueEvent(spEvent.get()), done);
            break;
        }

    done:
        if (SUCCEEDED(_CheckShutdownRequiresLock()))
        {
            // Continue listening to source event
            hr = m_spDevSource->BeginGetEvent(m_xOnMediaSourceEvent.get(), m_spDevSource.get());

            if (FAILED(hr))
            {
                DEBUG_MSG(L"error when processing source event: met=%d, hr=0x%08x", met, hr);
            }
        }
    }

    _Requires_lock_held_(m_Lock)
    HRESULT AugmentedMediaSource::OnNewStream(IMFMediaEvent* pEvent, MediaEventType met)
    {
        RETURN_HR_IF_NULL(E_INVALIDARG, pEvent);

        PROPVARIANT propValue;
        PropVariantInit(&propValue);

        DEBUG_MSG(L"OnNewStream");

        RETURN_IF_FAILED(pEvent->GetValue(&propValue));
        RETURN_IF_FAILED((propValue.vt == VT_UNKNOWN ? S_OK : E_INVALIDARG));
        RETURN_IF_FAILED((propValue.punkVal != nullptr ? S_OK : E_INVALIDARG));

        wil::com_ptr_nothrow<IMFMediaStream> spMediaStream;
        RETURN_IF_FAILED(propValue.punkVal->QueryInterface(IID_PPV_ARGS(&spMediaStream)));

        wil::com_ptr_nothrow<IMFStreamDescriptor> spStreamDesc;
        RETURN_IF_FAILED(spMediaStream->GetStreamDescriptor(&spStreamDesc));

        DWORD dwStreamIdentifier = 0;
        RETURN_IF_FAILED(spStreamDesc->GetStreamIdentifier(&dwStreamIdentifier));

        for (DWORD i = 0; i < m_streamList.size(); i++)
        {
            if (dwStreamIdentifier == m_streamList[i]->m_dwDevSourceStreamIdentifier)
            {
                RETURN_IF_FAILED(m_streamList[i]->SetMediaStream(spMediaStream.get()));

                wil::com_ptr_nothrow<::IUnknown> spunkStream;
                RETURN_IF_FAILED(m_streamList[i]->QueryInterface(IID_PPV_ARGS(&spunkStream)));

                RETURN_IF_FAILED(m_spEventQueue->QueueEventParamUnk(
                    met,
                    GUID_NULL,
                    S_OK,
                    spunkStream.get()));
            }
        }
        
        return S_OK;
    }

    _Requires_lock_held_(m_Lock)
    HRESULT AugmentedMediaSource::OnSourceStarted(IMFMediaEvent* pEvent)
    {
        DEBUG_MSG(L"OnSourceStarted");

        m_sourceState = SourceState::Started;

        // Forward that the source started.
        RETURN_IF_FAILED(m_spEventQueue->QueueEvent(pEvent));

        return S_OK;
    }

    _Requires_lock_held_(m_Lock)
    HRESULT AugmentedMediaSource::OnSourceStopped(IMFMediaEvent* pEvent)
    {
        DEBUG_MSG(L"OnSourceStopped ");

        m_sourceState = SourceState::Stopped;

        for (size_t i = 0; i < m_streamList.size(); i++)
        {
            // Deselect the streams and send the stream stopped events.
            RETURN_IF_FAILED(m_streamList[i]->SetStreamState(MF_STREAM_STATE_STOPPED));
        }

        // Forward that the source stop.
        RETURN_IF_FAILED(m_spEventQueue->QueueEvent(pEvent));

        return S_OK;
    }

    _Requires_lock_held_(m_Lock)
    HRESULT AugmentedMediaSource::_ValidatePresentationDescriptor(_In_ IMFPresentationDescriptor* pPD)
    {
        DWORD cStreams = 0;
        bool anySelected = false;

        RETURN_HR_IF_NULL(E_INVALIDARG, pPD);

        // The caller's PD must have the same number of streams as ours.
        RETURN_IF_FAILED(pPD->GetStreamDescriptorCount(&cStreams));
        if (cStreams != m_streamList.size())
        {
            return E_INVALIDARG;
        }

        // The caller must select at least one stream.
        for (UINT32 i = 0; i < cStreams; ++i)
        {
            wil::com_ptr_nothrow<IMFStreamDescriptor> spSD;
            BOOL fSelected = FALSE;

            RETURN_IF_FAILED(pPD->GetStreamDescriptorByIndex(i, &fSelected, &spSD));
            anySelected |= !!fSelected;
        } 

        if (!anySelected)
        {
            return E_INVALIDARG;
        }

        return S_OK;
    }

    _Requires_lock_held_(m_Lock)
    HRESULT AugmentedMediaSource::_CreateSourceAttributes(_In_ IMFAttributes* pActivateAttributes)
    {
        // get devicesource attributes, and add additional if needed.
        RETURN_IF_FAILED(MFCreateAttributes(&m_spAttributes, 3));

        // copy deviceSource attributes.
        wil::com_ptr_nothrow<IMFAttributes> spSourceAttributes;
        wil::com_ptr_nothrow<IMFMediaSourceEx> spMediaSourceEx;
        RETURN_IF_FAILED(m_spDevSource->QueryInterface(IID_PPV_ARGS(&spMediaSourceEx)));
        RETURN_IF_FAILED(spMediaSourceEx->GetSourceAttributes(&spSourceAttributes));
        RETURN_IF_FAILED(spSourceAttributes->CopyAllItems(m_spAttributes.get()));

        // if ActivateAttributes is not null, copy and overwrite the same attributes from the underlying devicesource attributes.
        if (pActivateAttributes)
        {
            UINT32 uiCount = 0;

            RETURN_IF_FAILED(pActivateAttributes->GetCount(&uiCount));

            for (UINT32 i = 0; i < uiCount; i++)
            {
                GUID guid = GUID_NULL;
                wil::unique_prop_variant spPropVar;

                RETURN_IF_FAILED(pActivateAttributes->GetItemByIndex(i, &guid, &spPropVar));
                RETURN_IF_FAILED(m_spAttributes->SetItem(guid, spPropVar));
            }
        }

        // Create our source attribute store.
        RETURN_IF_FAILED(MFCreateAttributes(&m_spAttributes, 1));

        wil::com_ptr_nothrow<IMFSensorProfileCollection>  profileCollection;
        wil::com_ptr_nothrow<IMFSensorProfile> profile;

        // Create an empty profile collection...
        RETURN_IF_FAILED(MFCreateSensorProfileCollection(&profileCollection));

        // In this example since we have just one stream, we only have one
        // pin to add:  Pin = STREAM_ID.

        // Legacy profile is mandatory.  This is to ensure non-profile
        // aware applications can still function, but with degraded
        // feature sets.
        const DWORD STREAM_ID = 0;
        RETURN_IF_FAILED(MFCreateSensorProfile(KSCAMERAPROFILE_Legacy, 0 /*ProfileIndex*/, nullptr, &profile));
        RETURN_IF_FAILED(profile->AddProfileFilter(STREAM_ID, L"((RES==;FRT<=30,1;SUT==))"));
        RETURN_IF_FAILED(profileCollection->AddProfile(profile.get()));


        // Set the profile collection to the attribute store of the IMFTransform.
        RETURN_IF_FAILED(m_spAttributes->SetUnknown(
            MF_DEVICEMFT_SENSORPROFILE_COLLECTION,
            profileCollection.get()));

        // The MF_VIRTUALCAMERA_CONFIGURATION_APP_PACKAGE_FAMILY_NAME attribute specifies the 
        // virtual camera configuration app's PFN (Package Family Name).
        // Below we query programmatically the current application info (knowing that the app 
        // that activates the virtual camera registers it on the system and also acts as its 
        // configuration app).
        // If the MediaSource is associated with a specific UWP only, you may instead hardcode
        // a particular PFN.
        try
        {
            winrt::Windows::ApplicationModel::AppInfo appInfo = winrt::Windows::ApplicationModel::AppInfo::Current();
            DEBUG_MSG(L"AppInfo: %p ", appInfo);
            if (appInfo != nullptr)
            {
                DEBUG_MSG(L"PFN: %s \n", appInfo.PackageFamilyName().data());
                RETURN_IF_FAILED(m_spAttributes->SetString(MF_VIRTUALCAMERA_CONFIGURATION_APP_PACKAGE_FAMILY_NAME, appInfo.PackageFamilyName().data()));
            }
        }
        catch (...) { DEBUG_MSG(L"Not running in app package"); }
        

        return S_OK;
    }
}
