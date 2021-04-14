#include "pch.h"
#include "HWMediaSource.h"

namespace winrt::WindowsSample::implementation
{
    /////////////////////////////////////////////////////////////////////////////////
    HRESULT HWMediaSource::Initialize(LPCWSTR pwszSymLink)
    {
        winrt::slim_lock_guard lock(m_Lock);

        DEBUG_MSG(L"[HWMediaSource] Initialize enter");
        wil::com_ptr_nothrow<IMFAttributes> spDeviceAttributes;
        RETURN_IF_FAILED(MFCreateAttributes(&spDeviceAttributes, 2));
        RETURN_IF_FAILED(spDeviceAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID));
        RETURN_IF_FAILED(spDeviceAttributes->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, pwszSymLink));
        RETURN_IF_FAILED(MFCreateDeviceSource(spDeviceAttributes.get(), &_devSource));

        // create source reader
        wil::com_ptr_nothrow<IMFAttributes> spAttributes;
        RETURN_IF_FAILED(MFCreateAttributes(&spAttributes, 1));
        RETURN_IF_FAILED(MFCreateSourceReaderFromMediaSource(_devSource.get(), spAttributes.get(), &m_spSourceReader));


        // TODO: Project all streams, 
        // FOR CURRENT TESTING ONLY, this source will only project the first stream
        wil::com_ptr_nothrow<IMFPresentationDescriptor> spPDesc;
        RETURN_IF_FAILED(_devSource->CreatePresentationDescriptor(&spPDesc));

        wil::com_ptr_nothrow<IMFStreamDescriptor> streamDescriptor;
        BOOL selected = FALSE;
        RETURN_IF_FAILED(spPDesc->GetStreamDescriptorByIndex(0, &selected, &streamDescriptor));
        RETURN_IF_FAILED(MFCreatePresentationDescriptor(NUM_STREAMS, streamDescriptor.addressof(), &_spPresentationDescriptor));

        auto ptr = winrt::make_self<HWMediaStream>();
        m_spStream.attach(ptr.detach());
        RETURN_IF_FAILED(m_spStream->Initialize(this, _spPresentationDescriptor.get(), m_spSourceReader.get(), 0));

        RETURN_IF_FAILED(MFCreateEventQueue(&_spEventQueue));

        _sourceState = SourceState::Stopped;

        DEBUG_MSG(L"[HWMediaSource] Initialize exit");
        return S_OK;
    }

    // IMFMediaEventGenerator methods.
    IFACEMETHODIMP
        HWMediaSource::BeginGetEvent(
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
        HWMediaSource::EndGetEvent(
            _In_ IMFAsyncResult* pResult,
            _COM_Outptr_ IMFMediaEvent** ppEvent
        )
    {
        HRESULT hr = S_OK;
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        RETURN_IF_FAILED(_spEventQueue->EndGetEvent(pResult, ppEvent));

        return hr;
    }

    IFACEMETHODIMP
        HWMediaSource::GetEvent(
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
        HWMediaSource::QueueEvent(
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
    IFACEMETHODIMP
        HWMediaSource::CreatePresentationDescriptor(
            _COM_Outptr_ IMFPresentationDescriptor** ppPresentationDescriptor
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_HR_IF_NULL(E_POINTER, ppPresentationDescriptor);
        *ppPresentationDescriptor = nullptr;

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        RETURN_IF_FAILED(_spPresentationDescriptor->Clone(ppPresentationDescriptor));
        //RETURN_IF_FAILED(_devSource->CreatePresentationDescriptor(ppPresentationDescriptor));

        return S_OK;
    }

    IFACEMETHODIMP
        HWMediaSource::GetCharacteristics(
            _Out_ DWORD* pdwCharacteristics
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        _devSource->GetCharacteristics(pdwCharacteristics);

        return S_OK;
    }

    IFACEMETHODIMP
        HWMediaSource::Pause(
        )
    {
        // Pause() not required/needed
        return MF_E_INVALID_STATE_TRANSITION;
    }


    IFACEMETHODIMP
        HWMediaSource::Shutdown(
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        _sourceState = SourceState::Shutdown;

        _spAttributes.reset();
        if (_spEventQueue != nullptr)
        {
            _spEventQueue->Shutdown();
            _spEventQueue.reset();
        }

        if (m_spStream != nullptr)
        {
            m_spStream->Shutdown();
            m_spStream.reset();
        }

        return S_OK;
    }

    IFACEMETHODIMP
        HWMediaSource::Start(
            _In_ IMFPresentationDescriptor* pPresentationDescriptor,
            _In_opt_ const GUID* pguidTimeFormat,
            _In_ const PROPVARIANT* pvarStartPos
        )
    {
        winrt::slim_lock_guard lock(m_Lock);
        HRESULT hr = S_OK;
        DWORD count = 0;
        PROPVARIANT startTime;
        
        wil::com_ptr_nothrow<IMFStreamDescriptor> streamDesc;

        if (pPresentationDescriptor == nullptr || pvarStartPos == nullptr)
        {
            return E_INVALIDARG;
        }
        else if (pguidTimeFormat != nullptr && *pguidTimeFormat != GUID_NULL)
        {
            return MF_E_UNSUPPORTED_TIME_FORMAT;
        }

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());

        if (!(_sourceState != SourceState::Stopped || _sourceState != SourceState::Shutdown))
        {
            return MF_E_INVALID_STATE_TRANSITION;
        }

        _sourceState = SourceState::Started;

        // This checks the passed in PresentationDescriptor matches the member of streams we
        // have defined internally and that at least one stream is selected

        RETURN_IF_FAILED(pPresentationDescriptor->GetStreamDescriptorCount(&count));
        RETURN_IF_FAILED(InitPropVariantFromInt64(MFGetSystemTime(), &startTime));

        // Send event that the source started. Include error code in case it failed.
        RETURN_IF_FAILED(_spEventQueue->QueueEventParamVar(MESourceStarted,
            GUID_NULL,
            hr,
            &startTime));

        // TODO: fix this to show multipled stream
        // We're hardcoding this to the first descriptor
        // since this sample is a single stream sample.  For
        // multiple streams, we need to walk the list of streams
        // and for each selected stream, send the MEUpdatedStream
        // or MENewStream event along with the MEStreamStarted
        // event.
        BOOL selected = FALSE;
        RETURN_IF_FAILED(pPresentationDescriptor->GetStreamDescriptorByIndex(
            0,
            &selected,
            &streamDesc));

        DWORD streamIndex = 0;
        RETURN_IF_FAILED(streamDesc->GetStreamIdentifier(&streamIndex));
        if (streamIndex >= NUM_STREAMS)
        {
            return MF_E_INVALIDSTREAMNUMBER;
        }

        if (selected)
        {
            wil::com_ptr_nothrow<IUnknown> spunkStream;
            MediaEventType met = (_wasStreamPreviouslySelected ? MEUpdatedStream : MENewStream);

            // Update our internal PresentationDescriptor
            RETURN_IF_FAILED(m_spStream->SetStreamState(MF_STREAM_STATE_RUNNING));
            RETURN_IF_FAILED(m_spStream.query_to(&spunkStream));

            // Send the MEUpdatedStream/MENewStream to our source event
            // queue, return a pointer to the stream.
            RETURN_IF_FAILED(_spEventQueue->QueueEventParamUnk(
                met,
                GUID_NULL,
                S_OK,
                spunkStream.get()));

            // But for our stream started (MEStreamStarted), we post to our
            // stream event queue.
            RETURN_IF_FAILED(m_spStream->QueueEvent(MEStreamStarted,
                GUID_NULL,
                S_OK,
                &startTime));
        }
        _wasStreamPreviouslySelected = selected;

        return S_OK;
    }

    IFACEMETHODIMP
        HWMediaSource::Stop(
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
        RETURN_IF_FAILED(InitPropVariantFromInt64(MFGetSystemTime(), &stopTime));

        // Deselect the streams and send the stream stopped events.
        RETURN_IF_FAILED(m_spStream->GetStreamState(&state));
        _wasStreamPreviouslySelected = (state == MF_STREAM_STATE_RUNNING);
        RETURN_IF_FAILED(m_spStream->SetStreamState(MF_STREAM_STATE_STOPPED));

        RETURN_IF_FAILED(m_spStream->QueueEvent(MEStreamStopped, GUID_NULL, hr, &stopTime));
        RETURN_IF_FAILED(_spEventQueue->QueueEventParamVar(MESourceStopped, GUID_NULL, hr, &stopTime));

        return S_OK;
    }

    // IMFMediaSourceEx
    IFACEMETHODIMP
        HWMediaSource::GetSourceAttributes(
            _COM_Outptr_ IMFAttributes** sourceAttributes
        )
    {
        winrt::slim_lock_guard lock(m_Lock);

        RETURN_HR_IF_NULL(E_POINTER, sourceAttributes);
        *sourceAttributes = nullptr;

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        wil::com_ptr_nothrow<IMFMediaSourceEx> spMediaSourceEx;
        RETURN_IF_FAILED(_devSource.query_to<IMFMediaSourceEx>(&spMediaSourceEx));
        RETURN_IF_FAILED(spMediaSourceEx->GetSourceAttributes(sourceAttributes));

        try
        {
            winrt::Windows::ApplicationModel::AppInfo appInfo = winrt::Windows::ApplicationModel::AppInfo::Current();
            DEBUG_MSG(L"[HWMediaSource] AppInfo: %p ", appInfo);
            if (appInfo != nullptr)
            {
                DEBUG_MSG(L"PFN: %s \n", appInfo.PackageFamilyName().data());
                RETURN_IF_FAILED((*sourceAttributes)->SetString(MF_VIRTUALCAMERA_CONFIGURATION_APP_PACKAGE_FAMILY_NAME, appInfo.PackageFamilyName().data()));
            }
        }
        catch (...) { DEBUG_MSG(L"[HWMediaSource] No running in app package"); }
        
        return S_OK;
    }

    IFACEMETHODIMP
        HWMediaSource::GetStreamAttributes(
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
        RETURN_IF_FAILED(spMediaSourceEx->GetStreamAttributes(dwStreamIdentifier , ppAttributes));

        return S_OK;
    }

    IFACEMETHODIMP
        HWMediaSource::SetD3DManager(
            _In_opt_ IUnknown* pManager
        )
    {
        // Return code is ignored by the frame work, this is a
        // best effort attempt to inform the media source of the
        // DXGI manager to use if DX surface support is available.

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        wil::com_ptr_nothrow<IMFMediaSourceEx> spMediaSourceEx;
        RETURN_IF_FAILED(_devSource.query_to<IMFMediaSourceEx>(&spMediaSourceEx));
        RETURN_IF_FAILED(spMediaSourceEx->SetD3DManager(pManager));

        return S_OK;
    }

    // IMFGetService methods
    _Use_decl_annotations_
        IFACEMETHODIMP
        HWMediaSource::GetService(
            _In_ REFGUID guidService,
            _In_ REFIID riid,
            _Out_ LPVOID* ppvObject
        )
    {
        winrt::slim_lock_guard lock(m_Lock);
        wil::com_ptr_nothrow<IMFGetService> spGetService;

        RETURN_IF_FAILED(_CheckShutdownRequiresLock());
        if(_devSource.try_query_to<IMFGetService>(&spGetService))
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
        IFACEMETHODIMP
        HWMediaSource::KsProperty(
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

    /// Internal methods.
    HRESULT HWMediaSource::_CheckShutdownRequiresLock()
    {
        if (_sourceState == SourceState::Shutdown)
        {
            return MF_E_SHUTDOWN;
        }

        if (_spEventQueue == nullptr ||  _devSource == nullptr)
        {
            return E_UNEXPECTED;
        }

        return S_OK;
    }
}



