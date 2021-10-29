//
// Copyright (C) Microsoft Corporation. All rights reserved.
//
#include "pch.h"
#include "HWMediaSource.h"

namespace winrt::WindowsSample::implementation
{
    HWMediaSource::~HWMediaSource()
    {
        Shutdown();
        if (m_dwSerialWorkQueueId != 0)
        {
            (void)MFUnlockWorkQueue(m_dwSerialWorkQueueId);
            m_dwSerialWorkQueueId = 0;
        }
    }
    /////////////////////////////////////////////////////////////////////////////////
    HRESULT HWMediaSource::Initialize(LPCWSTR pwszSymLink)
    {
        winrt::slim_lock_guard lock(m_Lock);

        DEBUG_MSG(L"Initialize enter");
        wil::com_ptr_nothrow<IMFAttributes> spDeviceAttributes;
        RETURN_IF_FAILED(MFCreateAttributes(&spDeviceAttributes, 2));
        RETURN_IF_FAILED(spDeviceAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID));
        RETURN_IF_FAILED(spDeviceAttributes->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, pwszSymLink));
        RETURN_IF_FAILED(MFCreateDeviceSource(spDeviceAttributes.get(), &m_spDevSource));

        RETURN_IF_FAILED(m_spDevSource->CreatePresentationDescriptor(&m_spPDesc));
        RETURN_IF_FAILED(_CreateSourceAttributes());
        RETURN_IF_FAILED(_CreateMediaStreams());
        RETURN_IF_FAILED(MFCreateEventQueue(&m_spEventQueue));

        RETURN_IF_FAILED(MFAllocateSerialWorkQueue(MFASYNC_CALLBACK_QUEUE_MULTITHREADED, &m_dwSerialWorkQueueId));

        auto ptr = winrt::make_self<CAsyncCallback<HWMediaSource>>(this, &HWMediaSource::OnMediaSourceEvent, m_dwSerialWorkQueueId);
        m_xOnMediaSourceEvent.attach(ptr.detach());
        RETURN_IF_FAILED(m_spDevSource->BeginGetEvent(m_xOnMediaSourceEvent.get(), m_spDevSource.get()));

        m_sourceState = SourceState::Stopped;

        DEBUG_MSG(L"Initialize exit");
        return S_OK;
    }

    // IMFMediaEventGenerator methods.
    IFACEMETHODIMP HWMediaSource::BeginGetEvent(
        _In_ IMFAsyncCallback* pCallback,
        _In_ IUnknown* punkState
    )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        RETURN_IF_FAILED(m_spEventQueue->BeginGetEvent(pCallback, punkState));

        return S_OK;
    }

    IFACEMETHODIMP HWMediaSource::EndGetEvent(
        _In_ IMFAsyncResult* pResult,
        _COM_Outptr_ IMFMediaEvent** ppEvent
    )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        RETURN_IF_FAILED(m_spEventQueue->EndGetEvent(pResult, ppEvent));

        return S_OK;
    }

    IFACEMETHODIMP HWMediaSource::GetEvent(
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

    IFACEMETHODIMP HWMediaSource::QueueEvent(
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

    // IMFMediaSource methods
    IFACEMETHODIMP HWMediaSource::CreatePresentationDescriptor(
            _COM_Outptr_ IMFPresentationDescriptor** ppPresentationDescriptor
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_HR_IF_NULL(E_POINTER, ppPresentationDescriptor);
        *ppPresentationDescriptor = nullptr;

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        RETURN_IF_FAILED(m_spDevSource->CreatePresentationDescriptor(ppPresentationDescriptor));

        return S_OK;
    }

    IFACEMETHODIMP  HWMediaSource::GetCharacteristics(_Out_ DWORD* pdwCharacteristics)
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        RETURN_IF_FAILED(m_spDevSource->GetCharacteristics(pdwCharacteristics));

        return S_OK;
    }

    IFACEMETHODIMP HWMediaSource::Pause()
    {
        // Pause() not required/needed
        return MF_E_INVALID_STATE_TRANSITION;
    }

    IFACEMETHODIMP HWMediaSource::Shutdown()
    {
        winrt::slim_lock_guard lock(m_Lock);

        m_sourceState = SourceState::Shutdown;

        m_spAttributes.reset();
        if (m_spEventQueue != nullptr)
        {
            m_spEventQueue->Shutdown();
            m_spEventQueue.reset();
        }

        for (auto spStream : m_streamList)
        {
            spStream->Shutdown();
        }
        m_streamList.reset();
        m_spDevSource->Shutdown();

        return S_OK;
    }

    IFACEMETHODIMP HWMediaSource::Start(
            _In_ IMFPresentationDescriptor* pPresentationDescriptor,
            _In_opt_ const GUID* pguidTimeFormat,
            _In_ const PROPVARIANT* pvarStartPos
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

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

        if (!(m_sourceState != SourceState::Stopped || m_sourceState != SourceState::Shutdown))
        {
            return MF_E_INVALID_STATE_TRANSITION;
        }

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

        RETURN_IF_FAILED(m_spDevSource->Start(pPresentationDescriptor, pguidTimeFormat, pvarStartPos));

        return S_OK;
    }

    IFACEMETHODIMP HWMediaSource::Stop()
    {
        winrt::slim_lock_guard lock(m_Lock);

        if (m_sourceState != SourceState::Started)
        {
            return MF_E_INVALID_STATE_TRANSITION;
        }
        m_sourceState = SourceState::Stopped;

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        RETURN_IF_FAILED(m_spDevSource->Stop());

        return S_OK;
    }

    // IMFMediaSourceEx
    IFACEMETHODIMP HWMediaSource::GetSourceAttributes(
            _COM_Outptr_ IMFAttributes** sourceAttributes
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_HR_IF_NULL(E_POINTER, sourceAttributes);
        *sourceAttributes = nullptr;

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        RETURN_IF_FAILED(MFCreateAttributes(sourceAttributes, 1));

        return m_spAttributes->CopyAllItems(*sourceAttributes);
    }

    IFACEMETHODIMP HWMediaSource::GetStreamAttributes(
            DWORD dwStreamIdentifier,
            _COM_Outptr_ IMFAttributes** ppAttributes
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_HR_IF_NULL(E_POINTER, ppAttributes);
        *ppAttributes = nullptr;

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        wil::com_ptr_nothrow<IMFMediaSourceEx> spMediaSourceEx;
        RETURN_IF_FAILED(m_spDevSource->QueryInterface(IID_PPV_ARGS(&spMediaSourceEx)));
        RETURN_IF_FAILED(spMediaSourceEx->GetStreamAttributes(dwStreamIdentifier, ppAttributes));

        return S_OK;
    }

    IFACEMETHODIMP HWMediaSource::SetD3DManager(
            _In_opt_ IUnknown* pManager
        )
    {
        // Return code is ignored by the framework, this is a
        // best effort attempt to inform the media source of the
        // DXGI manager to use if DX surface support is available.
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        wil::com_ptr_nothrow<IMFMediaSourceEx> spMediaSourceEx;
        RETURN_IF_FAILED(m_spDevSource->QueryInterface(IID_PPV_ARGS(&spMediaSourceEx)));
        RETURN_IF_FAILED(spMediaSourceEx->SetD3DManager(pManager));

        return S_OK;
    }

    // IMFGetService methods
    IFACEMETHODIMP HWMediaSource::GetService(
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

    // IKsControl methods
    IFACEMETHODIMP HWMediaSource::KsProperty(
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
        if (SUCCEEDED(m_spDevSource->QueryInterface(IID_PPV_ARGS(&spKsControl))))
        {
            RETURN_IF_FAILED(spKsControl->KsProperty(pProperty, ulPropertyLength, pPropertyData, ulDataLength, pBytesReturned));
        }
        else
        {
            return HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND);
        }

        return S_OK;
    }

    IFACEMETHODIMP HWMediaSource::KsMethod(
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
        if (SUCCEEDED(m_spDevSource->QueryInterface(IID_PPV_ARGS(&spKsControl))))
        {
            RETURN_IF_FAILED(spKsControl->KsMethod(pMethod, ulMethodLength, pMethodData, ulDataLength, pBytesReturned));
        }
        else
        {
            return HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND);
        }

        return S_OK;
    }

    IFACEMETHODIMP HWMediaSource::KsEvent(
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
        if (m_spDevSource->QueryInterface(IID_PPV_ARGS(&spKsControl)))
        {
            RETURN_IF_FAILED(spKsControl->KsEvent(pEvent, ulEventLength, pEventData, ulDataLength, pBytesReturned));
        }
        else
        {
            return HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND);
        }
        return S_OK;
    }

    IFACEMETHODIMP HWMediaSource::SetDefaultAllocator(
        _In_  DWORD dwOutputStreamID,
        _In_  IUnknown* pAllocator) 
    {
        winrt::slim_lock_guard lock(m_Lock);
        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        wil::com_ptr_nothrow<IMFSampleAllocatorControl> spAllocatorControl;

        RETURN_IF_FAILED(m_spDevSource->QueryInterface(IID_PPV_ARGS(&spAllocatorControl)));
        RETURN_IF_FAILED(spAllocatorControl->SetDefaultAllocator(dwOutputStreamID, pAllocator));

        return S_OK;
    }

    IFACEMETHODIMP HWMediaSource::GetAllocatorUsage(
        _In_  DWORD dwOutputStreamID,
        _Out_  DWORD* pdwInputStreamID,
        _Out_  MFSampleAllocatorUsage* peUsage)
    {
        winrt::slim_lock_guard lock(m_Lock);
        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        RETURN_HR_IF_NULL(E_POINTER, pdwInputStreamID);
        RETURN_HR_IF_NULL(E_POINTER, peUsage);

        wil::com_ptr_nothrow<IMFSampleAllocatorControl> spAllocatorControl;
        if (SUCCEEDED(m_spDevSource->QueryInterface(IID_PPV_ARGS(&spAllocatorControl))))
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
    HRESULT HWMediaSource::_CheckShutdownRequiresLock()
    {
        if (m_sourceState == SourceState::Shutdown)
        {
            return MF_E_SHUTDOWN;
        }

        if (m_spEventQueue == nullptr || m_spDevSource == nullptr)
        {
            return E_UNEXPECTED;
        }

        return S_OK;
    }

    HRESULT HWMediaSource::OnMediaSourceEvent(_In_ IMFAsyncResult* pResult)
    {
        MediaEventType met = MEUnknown;

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
            RETURN_IF_FAILED(m_spEventQueue->QueueEvent(spEvent.get()));
            break;
        }

        {
            winrt::slim_lock_guard lock(m_Lock);
            if (SUCCEEDED(_CheckShutdownRequiresLock()))
            {
                // Continue listening to source event
                RETURN_IF_FAILED(m_spDevSource->BeginGetEvent(m_xOnMediaSourceEvent.get(), m_spDevSource.get()));
            }
        }

        return S_OK;
    }

    HRESULT HWMediaSource::OnNewStream(IMFMediaEvent* pEvent, MediaEventType met)
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

                    RETURN_IF_FAILED(m_spEventQueue->QueueEventParamUnk(
                        met,
                        GUID_NULL,
                        S_OK,
                        spunkStream.get()));
                }
            }
        }
        return S_OK;
    }

    HRESULT HWMediaSource::OnSourceStarted(IMFMediaEvent* pEvent)
    {
        winrt::slim_lock_guard lock(m_Lock);
        DEBUG_MSG(L"OnSourceStarted");

        m_sourceState = SourceState::Started;

        // Forward that the source started.
        RETURN_IF_FAILED(m_spEventQueue->QueueEvent(pEvent));

        return S_OK;
    }

    HRESULT HWMediaSource::OnSourceStopped(IMFMediaEvent* pEvent)
    {
        winrt::slim_lock_guard lock(m_Lock);
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

    HRESULT HWMediaSource::_CreateMediaStreams()
    {
        wil::com_ptr_nothrow<IMFPresentationDescriptor> spPDesc;
        RETURN_IF_FAILED(m_spDevSource->CreatePresentationDescriptor(&spPDesc));

        DWORD dwStreamCount = 0;
        RETURN_IF_FAILED(spPDesc->GetStreamDescriptorCount(&dwStreamCount));
        m_streamList = wilEx::make_unique_cotaskmem_array<wil::com_ptr_nothrow<HWMediaStream>>(dwStreamCount);
        RETURN_IF_NULL_ALLOC(m_streamList.get());

        for (DWORD i = 0; i < dwStreamCount; i++)
        {
            wil::com_ptr_nothrow<IMFStreamDescriptor> spStreamDesc;
            BOOL selected = FALSE;
            RETURN_IF_FAILED(spPDesc->GetStreamDescriptorByIndex(i, &selected , &spStreamDesc));

            auto ptr = winrt::make_self<HWMediaStream>();
            m_streamList[i] = ptr.detach();
            RETURN_IF_FAILED(m_streamList[i]->Initialize(this, spStreamDesc.get(), m_dwSerialWorkQueueId));
        }

        return S_OK;
    }

    HRESULT HWMediaSource::_CreateSourceAttributes()
    {
        if (m_spAttributes == nullptr)
        {
            RETURN_IF_FAILED(MFCreateAttributes(&m_spAttributes, 1));

            wil::com_ptr_nothrow<IMFMediaSourceEx> spMediaSourceEx;
            RETURN_IF_FAILED(m_spDevSource->QueryInterface(IID_PPV_ARGS(&spMediaSourceEx)));
            RETURN_IF_FAILED(spMediaSourceEx->GetSourceAttributes(&m_spAttributes));

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
        }
        return S_OK;
    }
}
