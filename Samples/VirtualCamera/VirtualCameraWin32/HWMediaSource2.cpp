//
// Copyright (C) Microsoft Corporation. All rights reserved.
//
#include "pch.h"
#include "HWMediaSource2.h"

namespace winrt::WindowsSample::implementation
{
    HWMediaSource2::~HWMediaSource2()
    {
        Shutdown();
        if (m_dwSerialWorkQueueId != 0)
        {
            (void)MFUnlockWorkQueue(m_dwSerialWorkQueueId);
            m_dwSerialWorkQueueId = 0;
        }
    }
    /////////////////////////////////////////////////////////////////////////////////
    HRESULT HWMediaSource2::Initialize(LPCWSTR pwszSymLink)
    {
        winrt::slim_lock_guard lock(m_Lock);

        DEBUG_MSG(L"Initialize enter");
        wil::com_ptr_nothrow<IMFAttributes> spDeviceAttributes;
        RETURN_IF_FAILED(MFCreateAttributes(&spDeviceAttributes, 2));
        RETURN_IF_FAILED(spDeviceAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID));
        RETURN_IF_FAILED(spDeviceAttributes->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, pwszSymLink));
        RETURN_IF_FAILED(MFCreateDeviceSource(spDeviceAttributes.get(), &_devSource));

        RETURN_IF_FAILED(_devSource->CreatePresentationDescriptor(&_spPDesc));
        RETURN_IF_FAILED(_CreateSourceAttributes());
        RETURN_IF_FAILED(CreateMediaStreams());
        RETURN_IF_FAILED(MFCreateEventQueue(&_spEventQueue));

        RETURN_IF_FAILED(MFAllocateSerialWorkQueue(MFASYNC_CALLBACK_QUEUE_MULTITHREADED, &m_dwSerialWorkQueueId));

        auto ptr = winrt::make_self<CAsyncCallback<HWMediaSource2>>(this, &HWMediaSource2::OnMediaSourceEvent, m_dwSerialWorkQueueId);
        m_xOnMediaSourceEvent.attach(ptr.detach());
        RETURN_IF_FAILED(_devSource->BeginGetEvent(m_xOnMediaSourceEvent.get(), _devSource.get()));

        _sourceState = SourceState::Stopped;
        bInitialize = true;

        DEBUG_MSG(L"Initialize exit");
        return S_OK;
    }

    // IMFMediaEventGenerator methods.
    IFACEMETHODIMP HWMediaSource2::BeginGetEvent(
        _In_ IMFAsyncCallback* pCallback,
        _In_ IUnknown* punkState
    )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        RETURN_IF_FAILED(_spEventQueue->BeginGetEvent(pCallback, punkState));

        return S_OK;
    }

    IFACEMETHODIMP HWMediaSource2::EndGetEvent(
        _In_ IMFAsyncResult* pResult,
        _COM_Outptr_ IMFMediaEvent** ppEvent
    )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        RETURN_IF_FAILED(_spEventQueue->EndGetEvent(pResult, ppEvent));

        return S_OK;
    }

    IFACEMETHODIMP HWMediaSource2::GetEvent(
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

    IFACEMETHODIMP HWMediaSource2::QueueEvent(
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

    // IMFMediaSource methods
    IFACEMETHODIMP HWMediaSource2::CreatePresentationDescriptor(
        _COM_Outptr_ IMFPresentationDescriptor** ppPresentationDescriptor)
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_HR_IF_NULL(E_POINTER, ppPresentationDescriptor);
        *ppPresentationDescriptor = nullptr;

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        RETURN_IF_FAILED(_devSource->CreatePresentationDescriptor(ppPresentationDescriptor));

        return S_OK;
    }

    IFACEMETHODIMP  HWMediaSource2::GetCharacteristics(_Out_ DWORD* pdwCharacteristics)
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        RETURN_IF_FAILED(_devSource->GetCharacteristics(pdwCharacteristics));

        return S_OK;
    }

    IFACEMETHODIMP HWMediaSource2::Pause()
    {
        // Pause() not required/needed
        return MF_E_INVALID_STATE_TRANSITION;
    }

    IFACEMETHODIMP HWMediaSource2::Shutdown()
    {
        winrt::slim_lock_guard lock(m_Lock);

        _sourceState = SourceState::Shutdown;

        _spAttributes.reset();
        if (_spEventQueue != nullptr)
        {
            _spEventQueue->Shutdown();
            _spEventQueue.reset();
        }

        for (auto spStream : m_streamList)
        {
            spStream->Shutdown();
        }
        m_streamList.reset();
        _devSource->Shutdown();

        return S_OK;
    }

    IFACEMETHODIMP HWMediaSource2::Start(
        _In_ IMFPresentationDescriptor* pPresentationDescriptor,
        _In_opt_ const GUID* pguidTimeFormat,
        _In_ const PROPVARIANT* pvarStartPos
    )
    {
        winrt::slim_lock_guard lock(m_Lock);
        DWORD count = 0;

        wil::com_ptr_nothrow<IMFStreamDescriptor> streamDesc;

        if (pPresentationDescriptor == nullptr || pvarStartPos == nullptr)
        {
            return E_INVALIDARG;
        }
        
        if (pguidTimeFormat != nullptr && *pguidTimeFormat != GUID_NULL)
        {
            return MF_E_UNSUPPORTED_TIME_FORMAT;
        }

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        if (!(_sourceState != SourceState::Stopped || _sourceState != SourceState::Shutdown))
        {
            return MF_E_INVALID_STATE_TRANSITION;
        }


        // TODO: REmove
        // The caller's PD must have the same number of streams as ours.
        DWORD cStreams = 0;
        RETURN_IF_FAILED(pPresentationDescriptor->GetStreamDescriptorCount(&cStreams));

        // The caller must select at least one stream.
        for (UINT32 i = 0; i < cStreams; ++i)
        {
            wil::com_ptr_nothrow<IMFStreamDescriptor> spSD;
            BOOL fSelected = FALSE;

            RETURN_IF_FAILED(pPresentationDescriptor->GetStreamDescriptorByIndex(i, &fSelected, &spSD));
            if (fSelected)
            {
                DWORD streamId = 0;
                spSD->GetStreamIdentifier(&streamId);
                DEBUG_MSG(L"Selected stream Id: %d", streamId);
            }
        }

        RETURN_IF_FAILED(_devSource->Start(pPresentationDescriptor, pguidTimeFormat, pvarStartPos));

        return S_OK;
    }

    IFACEMETHODIMP HWMediaSource2::Stop(
        )
    {
        winrt::slim_lock_guard lock(m_Lock);
        HRESULT hr = S_OK;

        PROPVARIANT stopTime;

        MF_STREAM_STATE state;

        if (_sourceState != SourceState::Started)
        {
            return MF_E_INVALID_STATE_TRANSITION;
        }
        _sourceState = SourceState::Stopped;

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        RETURN_IF_FAILED(_devSource->Stop());

        return S_OK;
    }

    // IMFMediaSourceEx
    IFACEMETHODIMP HWMediaSource2::GetSourceAttributes(
            _COM_Outptr_ IMFAttributes** sourceAttributes
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_HR_IF_NULL(E_POINTER, sourceAttributes);
        *sourceAttributes = nullptr;

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        RETURN_IF_FAILED(MFCreateAttributes(sourceAttributes, 1));

        return _spAttributes->CopyAllItems(*sourceAttributes);
    }

    IFACEMETHODIMP HWMediaSource2::GetStreamAttributes(
            DWORD dwStreamIdentifier,
            _COM_Outptr_ IMFAttributes** ppAttributes
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_HR_IF_NULL(E_POINTER, ppAttributes);
        *ppAttributes = nullptr;

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        wil::com_ptr_nothrow<IMFMediaSourceEx> spMediaSourceEx;
        RETURN_IF_FAILED(_devSource.query_to<IMFMediaSourceEx>(&spMediaSourceEx));
        RETURN_IF_FAILED(spMediaSourceEx->GetStreamAttributes(dwStreamIdentifier, ppAttributes));

        return S_OK;
    }

    IFACEMETHODIMP HWMediaSource2::SetD3DManager(
            _In_opt_ IUnknown* pManager
        )
    {
        // Return code is ignored by the frame work, this is a
        // best effort attempt to inform the media source of the
        // DXGI manager to use if DX surface support is available.
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        wil::com_ptr_nothrow<IMFMediaSourceEx> spMediaSourceEx;
        RETURN_IF_FAILED(_devSource.query_to<IMFMediaSourceEx>(&spMediaSourceEx));
        RETURN_IF_FAILED(spMediaSourceEx->SetD3DManager(pManager));

        return S_OK;
    }

    // IMFGetService methods
    _Use_decl_annotations_
    IFACEMETHODIMP HWMediaSource2::GetService(
            _In_ REFGUID guidService,
            _In_ REFIID riid,
            _Out_ LPVOID* ppvObject
        )
    {
        winrt::slim_lock_guard lock(m_Lock);
        wil::com_ptr_nothrow<IMFGetService> spGetService;

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        if (_devSource.try_query_to<IMFGetService>(&spGetService))
        {
            RETURN_IF_FAILED(spGetService->GetService(guidService, riid, ppvObject));
        }
        else
        {
            return MF_E_UNSUPPORTED_SERVICE;
        }

        return S_OK;
    }

    // IKsControl methods
    _Use_decl_annotations_
    IFACEMETHODIMP HWMediaSource2::KsProperty(
            _In_reads_bytes_(ulPropertyLength) PKSPROPERTY pProperty,
            _In_ ULONG ulPropertyLength,
            _Inout_updates_to_(ulDataLength, *pBytesReturned) LPVOID pPropertyData,
            _In_ ULONG ulDataLength,
            _Out_ ULONG* pBytesReturned
        )
    {
        // ERROR_SET_NOT_FOUND is the standard error code returned
        // by the AV Stream driver framework when a miniport
        // driver does not register a handler for a KS operation.
        // We want to mimic the driver behavior here if we don't
        // support controls.
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        wil::com_ptr_nothrow<IKsControl> spKsControl;
        if (_devSource.try_query_to<IKsControl>(&spKsControl))
        {
            RETURN_IF_FAILED(spKsControl->KsProperty(pProperty, ulPropertyLength, pPropertyData, ulDataLength, pBytesReturned));
        }
        else
        {
            return HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND);
        }

        return S_OK;
    }

    _Use_decl_annotations_
        IFACEMETHODIMP HWMediaSource2::KsMethod(
            _In_reads_bytes_(ulMethodLength) PKSMETHOD pMethod,
            _In_ ULONG ulMethodLength,
            _Inout_updates_to_(ulDataLength, *pBytesReturned) LPVOID pMethodData,
            _In_ ULONG ulDataLength,
            _Out_ ULONG* pBytesReturned
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        wil::com_ptr_nothrow<IKsControl> spKsControl;
        if (_devSource.try_query_to<IKsControl>(&spKsControl))
        {
            RETURN_IF_FAILED(spKsControl->KsMethod(pMethod, ulMethodLength, pMethodData, ulDataLength, pBytesReturned));
        }
        else
        {
            return HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND);
        }

        return S_OK;
    }

    _Use_decl_annotations_
        IFACEMETHODIMP HWMediaSource2::KsEvent(
            _In_reads_bytes_opt_(ulEventLength) PKSEVENT pEvent,
            _In_ ULONG ulEventLength,
            _Inout_updates_to_(ulDataLength, *pBytesReturned) LPVOID pEventData,
            _In_ ULONG ulDataLength,
            _Out_opt_ ULONG* pBytesReturned
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        wil::com_ptr_nothrow<IKsControl> spKsControl;
        if (_devSource.try_query_to<IKsControl>(&spKsControl))
        {
            RETURN_IF_FAILED(spKsControl->KsEvent(pEvent, ulEventLength, pEventData, ulDataLength, pBytesReturned));
        }
        else
        {
            return HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND);
        }
        return S_OK;
    }

    IFACEMETHODIMP HWMediaSource2::SetDefaultAllocator(
        _In_  DWORD dwOutputStreamID,
        _In_  IUnknown* pAllocator) 
    {
        winrt::slim_lock_guard lock(m_Lock);
        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        wil::com_ptr_nothrow<IMFSampleAllocatorControl> spAllocatorControl;
        //if (SUCCEEDED(_devSource->QueryInterface(IID_PPV_ARGS(&spAllocatorControl))))
        RETURN_IF_FAILED(_devSource->QueryInterface(IID_PPV_ARGS(&spAllocatorControl)));
        {
            RETURN_IF_FAILED(spAllocatorControl->SetDefaultAllocator(dwOutputStreamID, pAllocator));
        }

        return S_OK;
    }

    IFACEMETHODIMP HWMediaSource2::GetAllocatorUsage(
        _In_  DWORD dwOutputStreamID,
        _Out_  DWORD* pdwInputStreamID,
        _Out_  MFSampleAllocatorUsage* peUsage)
    {
        winrt::slim_lock_guard lock(m_Lock);
        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        RETURN_HR_IF_NULL(E_POINTER, pdwInputStreamID);
        RETURN_HR_IF_NULL(E_POINTER, peUsage);

        wil::com_ptr_nothrow<IMFSampleAllocatorControl> spAllocatorControl;
        if (SUCCEEDED(_devSource->QueryInterface(IID_PPV_ARGS(&spAllocatorControl))))
        {
            RETURN_IF_FAILED(spAllocatorControl->GetAllocatorUsage(dwOutputStreamID, pdwInputStreamID, peUsage));
        }
        else
        {
            *pdwInputStreamID = dwOutputStreamID;
            *peUsage = MFSampleAllocatorUsage::MFSampleAllocatorUsage_UsesCustomAllocator;
        }

        return S_OK;
    }

    /// Internal methods.
    HRESULT HWMediaSource2::_CheckShutdownRequiresLock()
    {
        if (_sourceState == SourceState::Shutdown)
        {
            return MF_E_SHUTDOWN;
        }

        if (_spEventQueue == nullptr || _devSource == nullptr)
        {
            return E_UNEXPECTED;
        }

        return S_OK;
    }

    HRESULT HWMediaSource2::OnMediaSourceEvent(_In_ IMFAsyncResult* pResult)
    {
        MediaEventType met = MEUnknown;
        HRESULT hrEventStatus = S_OK;
        wil::com_ptr_nothrow<IMFMediaEvent> spEvent;
        wil::com_ptr_nothrow<IUnknown> spUnknown;
        wil::com_ptr_nothrow<IMFMediaSource> spMediaSource;

        DEBUG_MSG(L"OnMediaSourceEvent %p ", pResult);

        RETURN_HR_IF_NULL(MF_E_UNEXPECTED, pResult);
        RETURN_IF_FAILED(pResult->GetState(&spUnknown));
        RETURN_IF_FAILED(spUnknown->QueryInterface(IID_PPV_ARGS(&spMediaSource)));

        RETURN_IF_FAILED(spMediaSource->EndGetEvent(pResult, &spEvent));
        RETURN_HR_IF_NULL(MF_E_UNEXPECTED, spEvent);
        RETURN_IF_FAILED(spEvent->GetType(&met));
        if (SUCCEEDED(spEvent->GetStatus(&hrEventStatus)) && FAILED(hrEventStatus))
        {
            // Error status in any event would put the underlying SR in error state. Any further call would return this error 
            //SetAsyncStatus(hrEventStatus);
        }

        switch (met)
        {
        case MENewStream:
        case MEUpdatedStream:
            RETURN_IF_FAILED(OnNewStream(spEvent.get(), met));
            break;
        case MESourceStarted:
            RETURN_IF_FAILED(OnSourceStarted(spEvent.get()));
            break;
        case MESourceStopped:
            RETURN_IF_FAILED(OnSourceStopped(spEvent.get()));
            break;
        default:
            // Forward all device source event out.
            RETURN_IF_FAILED(_spEventQueue->QueueEvent(spEvent.get()));
            break;
        }

        {
            winrt::slim_lock_guard lock(m_Lock);
            if (SUCCEEDED(_CheckShutdownRequiresLock()))
            {
                // Continue listening to source event
                RETURN_IF_FAILED(_devSource->BeginGetEvent(m_xOnMediaSourceEvent.get(), _devSource.get()));
            }
        }

        return S_OK;
    }

    HRESULT HWMediaSource2::OnNewStream(IMFMediaEvent* pEvent, MediaEventType met)
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

        winrt::slim_lock_guard lock(m_Lock);
        {
            for (DWORD i = 0; i < m_streamList.size(); i++)
            {
                if (dwStreamIdentifier == m_streamList[i]->StreamIdentifier())
                {
                    RETURN_IF_FAILED(m_streamList[i]->SetMediaStream(spMediaStream.get()));

                    wil::com_ptr_nothrow<::IUnknown> spunkStream;
                    RETURN_IF_FAILED(m_streamList[i]->QueryInterface(IID_PPV_ARGS(&spunkStream)));

                    RETURN_IF_FAILED(_spEventQueue->QueueEventParamUnk(
                        met,
                        GUID_NULL,
                        S_OK,
                        spunkStream.get()));
                }
            }
        }
        return S_OK;
    }

    HRESULT HWMediaSource2::OnSourceStarted(IMFMediaEvent* pEvent)
    {
        winrt::slim_lock_guard lock(m_Lock);
        DEBUG_MSG(L"OnSourceStarted");

        _sourceState = SourceState::Started;

        // Forward that the source started.
        RETURN_IF_FAILED(_spEventQueue->QueueEvent(pEvent));

        return S_OK;
    }

    HRESULT HWMediaSource2::OnSourceStopped(IMFMediaEvent* pEvent)
    {
        winrt::slim_lock_guard lock(m_Lock);
        DEBUG_MSG(L"OnSourceStopped ");

        _sourceState = SourceState::Stopped;

        for (size_t i = 0; i < m_streamList.size(); i++)
        {
            // Deselect the streams and send the stream stopped events.
            RETURN_IF_FAILED(m_streamList[i]->SetStreamState(MF_STREAM_STATE_STOPPED));
        }

        // Forward that the source stop.
        RETURN_IF_FAILED(_spEventQueue->QueueEvent(pEvent));

        return S_OK;
    }

    HRESULT HWMediaSource2::CreateMediaStreams()
    {
        // stream should only get created once at initialization
        //ASSERT(!m_streamList.get());

        wil::com_ptr_nothrow<IMFPresentationDescriptor> spPDesc;
        RETURN_IF_FAILED(_devSource->CreatePresentationDescriptor(&spPDesc));

        DWORD dwStreamCount = 0;
        RETURN_IF_FAILED(spPDesc->GetStreamDescriptorCount(&dwStreamCount));
        m_streamList = wilEx::make_unique_cotaskmem_array<wil::com_ptr_nothrow<HWMediaStream2>>(dwStreamCount);
        RETURN_IF_NULL_ALLOC(m_streamList.get());

        for (DWORD i = 0; i < dwStreamCount; i++)
        {
            wil::com_ptr_nothrow<IMFStreamDescriptor> spStreamDesc;
            BOOL selected = FALSE;
            RETURN_IF_FAILED(spPDesc->GetStreamDescriptorByIndex(i, &selected , &spStreamDesc));

            auto ptr = winrt::make_self<HWMediaStream2>();
            m_streamList[i] = ptr.detach();
            RETURN_IF_FAILED(m_streamList[i]->Initialize(this, spStreamDesc.get(), m_dwSerialWorkQueueId));
        }

        return S_OK;
    }

    HRESULT HWMediaSource2::_CreateSourceAttributes()
    {
        if (_spAttributes == nullptr)
        {
            RETURN_IF_FAILED(MFCreateAttributes(&_spAttributes, 1));

            wil::com_ptr_nothrow<IMFMediaSourceEx> spMediaSourceEx;
            RETURN_IF_FAILED(_devSource.query_to<IMFMediaSourceEx>(&spMediaSourceEx));
            RETURN_IF_FAILED(spMediaSourceEx->GetSourceAttributes(&_spAttributes));

            try
            {
                winrt::Windows::ApplicationModel::AppInfo appInfo = winrt::Windows::ApplicationModel::AppInfo::Current();
                DEBUG_MSG(L"AppInfo: %p ", appInfo);
                if (appInfo != nullptr)
                {
                    DEBUG_MSG(L"PFN: %s \n", appInfo.PackageFamilyName().data());
                    RETURN_IF_FAILED(_spAttributes->SetString(MF_VIRTUALCAMERA_CONFIGURATION_APP_PACKAGE_FAMILY_NAME, appInfo.PackageFamilyName().data()));
                }
            }
            catch (...) { DEBUG_MSG(L"Not running in app package"); }
        }
        return S_OK;
    }
}