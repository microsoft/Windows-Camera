// Copyright (C) Microsoft Corporation. All rights reserved.
#include <pch.h>

class RTPMediaSink
    : public winrt::implements<RTPMediaSink, winrt::Windows::Media::IMediaExtension, IMFMediaSink, IMFClockStateSink>
{
protected:
    bool m_bIsShutdown;
    std::vector<winrt::com_ptr<INetworkMediaStreamSink>> m_spStreamSinks;
    winrt::com_ptr<IMFPresentationClock> m_spClock;

    RTPMediaSink(std::vector<IMFMediaType*> streamMediaTypes)
        :m_bIsShutdown(false)
    {
        m_spStreamSinks.resize(streamMediaTypes.size());

        for (DWORD i = 0; i < streamMediaTypes.size(); i++)
        {
            m_spStreamSinks[i].attach(RTPVideoStreamSink::CreateInstance(streamMediaTypes[i], this, i));
        }
    }
    virtual ~RTPMediaSink() = default;
public:
    static IMFMediaSink* CreateInstance(std::vector<IMFMediaType*> streamMediaTypes)
    {
        if (streamMediaTypes.size())
        {
            return new RTPMediaSink(streamMediaTypes);
        }
        else
        {
            return nullptr;
        }
    }

    //IMFMediaSink
    STDMETHODIMP GetCharacteristics(
        /* [out] */ __RPC__out DWORD* pdwCharacteristics) noexcept
    {
        RETURNIFSHUTDOWN;
        if (!pdwCharacteristics)
        {
            return E_POINTER;
        }
        *pdwCharacteristics = MEDIASINK_FIXED_STREAMS | MEDIASINK_RATELESS;
        return S_OK;
    }

    STDMETHODIMP AddStreamSink(
        /* [in] */ DWORD dwStreamSinkIdentifier,
        /* [in] */ __RPC__in_opt IMFMediaType* pMediaType,
        /* [out] */ __RPC__deref_out_opt IMFStreamSink** ppStreamSink) noexcept
    {
        RETURNIFSHUTDOWN;
        return MF_E_STREAMSINKS_FIXED;
    }

    STDMETHODIMP RemoveStreamSink(
        /* [in] */ DWORD dwStreamSinkIdentifier) noexcept
    {
        RETURNIFSHUTDOWN;
        return MF_E_STREAMSINKS_FIXED;
    }

    STDMETHODIMP GetStreamSinkCount(
        /* [out] */ __RPC__out DWORD* pcStreamSinkCount) noexcept
    {
        RETURNIFSHUTDOWN;
        if (!pcStreamSinkCount)
        {
            return E_POINTER;
        }
        else
        {
            *pcStreamSinkCount = (DWORD)m_spStreamSinks.size();
            return S_OK;
        }
    }

    STDMETHODIMP GetStreamSinkByIndex(
        /* [in] */ DWORD dwIndex,
        /* [out] */ __RPC__deref_out_opt IMFStreamSink** ppStreamSink) noexcept
    {
        RETURNIFSHUTDOWN;
        if (!ppStreamSink) return E_POINTER;
        if (dwIndex > 0)
        {
            return MF_E_INVALIDINDEX;
        }
        m_spStreamSinks[dwIndex].as<IMFStreamSink>().copy_to(ppStreamSink);
        return S_OK;
    }

    STDMETHODIMP GetStreamSinkById(
        /* [in] */ DWORD dwStreamSinkIdentifier,
        /* [out] */ __RPC__deref_out_opt IMFStreamSink** ppStreamSink) noexcept
    {
        RETURNIFSHUTDOWN;
        if (dwStreamSinkIdentifier > m_spStreamSinks.size())
        {
            return MF_E_INVALIDSTREAMNUMBER;
        }
        else
        {
            m_spStreamSinks[dwStreamSinkIdentifier].as<IMFStreamSink>().copy_to(ppStreamSink);
            return S_OK;
        }
    }

    STDMETHODIMP SetPresentationClock(
        /* [in] */ __RPC__in_opt IMFPresentationClock* pPresentationClock) noexcept
    {
        RETURNIFSHUTDOWN;
        if (m_spClock)
        {
            m_spClock->RemoveClockStateSink(this);
            m_spClock = nullptr;
        }
        m_spClock.copy_from(pPresentationClock);
        return m_spClock->AddClockStateSink(this);

    }

    STDMETHODIMP GetPresentationClock(
        /* [out] */ __RPC__deref_out_opt IMFPresentationClock** ppPresentationClock) noexcept
    {
        RETURNIFSHUTDOWN;
        if (!ppPresentationClock) return E_POINTER;
        m_spClock.copy_to(ppPresentationClock);
        return S_OK;
    }

    STDMETHODIMP Shutdown(void) noexcept
    {
        RETURNIFSHUTDOWN;
        HRESULT hr = S_OK;
        m_bIsShutdown = true;
        for (auto strm : m_spStreamSinks)
        {
            auto hr1 = strm->Shutdown();
            hr = FAILED(hr1)? hr1: hr;
        }
        return hr;
    }

    //IMediaExtension
    STDMETHODIMP SetProperties(
        winrt::Windows::Foundation::Collections::IPropertySet const& configuration) noexcept
    {
        return S_OK;
    }

    // IMFClockStateSink methods
    STDMETHODIMP OnClockStart(MFTIME hnsSystemTime, LONGLONG llClockStartOffset) noexcept
    {
        HRESULT hr = S_OK;
        for (auto strm : m_spStreamSinks)
        {
            auto hr1 = strm->Start(hnsSystemTime, llClockStartOffset);
            hr = FAILED(hr1) ? hr1 : hr;
        }
        return hr;
    }
    STDMETHODIMP OnClockStop(MFTIME hnsSystemTime) noexcept
    {
        HRESULT hr = S_OK;
        for (auto strm : m_spStreamSinks)
        {
            auto hr1 = strm->Stop(hnsSystemTime);
            hr = FAILED(hr1) ? hr1 : hr;
        }
        return hr;
    }
    STDMETHODIMP OnClockPause(MFTIME hnsSystemTime) noexcept
    {
        HRESULT hr = S_OK;
        for (auto strm : m_spStreamSinks)
        {
            auto hr1 = strm->Pause(hnsSystemTime);
            hr = FAILED(hr1) ? hr1 : hr;
        }
        return hr;
    }
    STDMETHODIMP OnClockRestart(MFTIME hnsSystemTime) noexcept
    {
        HRESULT hr = S_OK;
        for (auto strm : m_spStreamSinks)
        {
            auto hr1 = strm->Start(hnsSystemTime, 0);
            hr = FAILED(hr1) ? hr1 : hr;
        }
        return hr;
    }
    STDMETHODIMP OnClockSetRate(MFTIME hnsSystemTime, float flRate) noexcept
    {
        return S_OK;
    }
};

RTPMEDIASTREAMER_API STDMETHODIMP CreateRTPMediaSink(std::vector<IMFMediaType*> mediaTypes, IMFMediaSink** ppMediaSink)
{
    HRESULT_EXCEPTION_BOUNDARY_START;
    winrt::check_pointer(ppMediaSink);
    *ppMediaSink = RTPMediaSink::CreateInstance(mediaTypes);
    HRESULT_EXCEPTION_BOUNDARY_END;
}