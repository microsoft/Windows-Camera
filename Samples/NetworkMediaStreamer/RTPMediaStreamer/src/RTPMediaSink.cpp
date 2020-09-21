// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for more information

#include <pch.h>

class RTPMediaSink
    : public winrt::implements<RTPMediaSink, winrt::Windows::Media::IMediaExtension, IMFMediaSink, IMFClockStateSink>
{
protected:
    bool m_bIsShutdown;
    std::vector<winrt::com_ptr<INetworkMediaStreamSink>> m_spStreamSinks;
    winrt::com_ptr<IMFPresentationClock> m_spClock;

    RTPMediaSink(std::vector<winrt::com_ptr<IMFMediaType>> streamMediaTypes)
        :m_bIsShutdown(false)
    {
        m_spStreamSinks.resize(streamMediaTypes.size());

        for (DWORD i = 0; i < streamMediaTypes.size(); i++)
        {
            m_spStreamSinks[i].attach(RTPVideoStreamSink::CreateInstance(streamMediaTypes[i].get(), this, i));
        }
    }
    virtual ~RTPMediaSink() = default;
public:
    static IMFMediaSink* CreateInstance(std::vector<winrt::com_ptr<IMFMediaType>> streamMediaTypes)
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
        /* [out] */ __RPC__out DWORD* pdwCharacteristics) noexcept override
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
        /* [out] */ __RPC__deref_out_opt IMFStreamSink** ppStreamSink) noexcept override
    {
        RETURNIFSHUTDOWN;
        return MF_E_STREAMSINKS_FIXED;
    }

    STDMETHODIMP RemoveStreamSink(
        /* [in] */ DWORD dwStreamSinkIdentifier) noexcept override
    {
        RETURNIFSHUTDOWN;
        return MF_E_STREAMSINKS_FIXED;
    }

    STDMETHODIMP GetStreamSinkCount(
        /* [out] */ __RPC__out DWORD* pcStreamSinkCount) noexcept override
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
        /* [out] */ __RPC__deref_out_opt IMFStreamSink** ppStreamSink) noexcept override
    {
        RETURNIFSHUTDOWN;
        if (!ppStreamSink)
        {
            return E_POINTER;
        }
        if (dwIndex > 0)
        {
            return MF_E_INVALIDINDEX;
        }
        m_spStreamSinks[dwIndex].as<IMFStreamSink>().copy_to(ppStreamSink);
        return S_OK;
    }

    STDMETHODIMP GetStreamSinkById(
        /* [in] */ DWORD dwStreamSinkIdentifier,
        /* [out] */ __RPC__deref_out_opt IMFStreamSink** ppStreamSink) noexcept override
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
        /* [in] */ __RPC__in_opt IMFPresentationClock* pPresentationClock) noexcept override
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
        /* [out] */ __RPC__deref_out_opt IMFPresentationClock** ppPresentationClock) noexcept override
    {
        RETURNIFSHUTDOWN;
        if (!ppPresentationClock)
        {
            return E_POINTER;
        }
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
    STDMETHODIMP OnClockStart(MFTIME hnsSystemTime, LONGLONG llClockStartOffset) noexcept override
    {
        HRESULT hr = S_OK;
        for (auto strm : m_spStreamSinks)
        {
            auto hr1 = strm->Start(hnsSystemTime, llClockStartOffset);
            hr = FAILED(hr1) ? hr1 : hr;
        }
        return hr;
    }
    STDMETHODIMP OnClockStop(MFTIME hnsSystemTime) noexcept override
    {
        HRESULT hr = S_OK;
        for (auto strm : m_spStreamSinks)
        {
            auto hr1 = strm->Stop(hnsSystemTime);
            hr = FAILED(hr1) ? hr1 : hr;
        }
        return hr;
    }
    STDMETHODIMP OnClockPause(MFTIME hnsSystemTime) noexcept override
    {
        HRESULT hr = S_OK;
        for (auto strm : m_spStreamSinks)
        {
            auto hr1 = strm->Pause(hnsSystemTime);
            hr = FAILED(hr1) ? hr1 : hr;
        }
        return hr;
    }
    STDMETHODIMP OnClockRestart(MFTIME hnsSystemTime) noexcept override
    {
        HRESULT hr = S_OK;
        for (auto strm : m_spStreamSinks)
        {
            auto hr1 = strm->Start(hnsSystemTime, 0);
            hr = FAILED(hr1) ? hr1 : hr;
        }
        return hr;
    }
    STDMETHODIMP OnClockSetRate(MFTIME hnsSystemTime, float flRate) noexcept override
    {
        return S_OK;
    }
};

RTPMEDIASTREAMER_API STDMETHODIMP CreateRTPMediaSink(IMFMediaType** apMediaTypes, DWORD dwMediaTypeCount, IMFMediaSink** ppMediaSink) try
{
    winrt::check_pointer(ppMediaSink);
    std::vector<winrt::com_ptr<IMFMediaType>> mediaTypes(dwMediaTypeCount);
    for (DWORD i = 0; i < dwMediaTypeCount; i++)
    {
        winrt::check_pointer(apMediaTypes[i]);
        mediaTypes[i].copy_from(apMediaTypes[i]);
    }
    *ppMediaSink = RTPMediaSink::CreateInstance(mediaTypes);
    return S_OK;
}HRESULT_EXCEPTION_BOUNDARY_FUNC