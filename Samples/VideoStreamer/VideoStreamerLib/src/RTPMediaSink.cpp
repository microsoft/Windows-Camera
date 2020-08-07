// Copyright (C) Microsoft Corporation. All rights reserved.
#include "RTPStreamSink.h"

class RTPMediaSink
    : public winrt::implements<RTPMediaSink, winrt::Windows::Media::IMediaExtension, IMFMediaSink, IMFClockStateSink>
{
protected:
    long m_cRef;
    bool m_bIsShutdown;
    std::vector<winrt::com_ptr<INetworkMediaStreamSink>> m_spStreamSinks;
    winrt::com_ptr<IMFPresentationClock> m_spClock;

    RTPMediaSink(std::vector<IMFMediaType*> streamMediaTypes)
        :m_bIsShutdown(false)
        , m_cRef(1)
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
        /* [out] */ __RPC__out DWORD* pdwCharacteristics)
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
        /* [out] */ __RPC__deref_out_opt IMFStreamSink** ppStreamSink)
    {
        RETURNIFSHUTDOWN;
        return MF_E_STREAMSINKS_FIXED;
    }

    STDMETHODIMP RemoveStreamSink(
        /* [in] */ DWORD dwStreamSinkIdentifier)
    {
        RETURNIFSHUTDOWN;
        return MF_E_STREAMSINKS_FIXED;
    }

    STDMETHODIMP GetStreamSinkCount(
        /* [out] */ __RPC__out DWORD* pcStreamSinkCount)
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
        /* [out] */ __RPC__deref_out_opt IMFStreamSink** ppStreamSink)
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
        /* [out] */ __RPC__deref_out_opt IMFStreamSink** ppStreamSink)
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
        /* [in] */ __RPC__in_opt IMFPresentationClock* pPresentationClock)
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
        /* [out] */ __RPC__deref_out_opt IMFPresentationClock** ppPresentationClock)
    {
        RETURNIFSHUTDOWN;
        if (!ppPresentationClock) return E_POINTER;
        m_spClock.copy_to(ppPresentationClock);
        return S_OK;
    }

    STDMETHODIMP Shutdown(void)
    {
        RETURNIFSHUTDOWN;
        m_bIsShutdown = true;
        for (auto strm : m_spStreamSinks)
        {
            strm->Shutdown();
        }
        return S_OK;
    }

    //IMediaExtension
    STDMETHODIMP SetProperties(
        winrt::Windows::Foundation::Collections::IPropertySet const& configuration
    )
    {
        return S_OK;
    }

    // IMFClockStateSink methods
    STDMETHODIMP OnClockStart(MFTIME hnsSystemTime, LONGLONG llClockStartOffset)
    {
        for (auto strm : m_spStreamSinks)
        {
            strm->Start(hnsSystemTime, llClockStartOffset);
        }
        return S_OK;
    }
    STDMETHODIMP OnClockStop(MFTIME hnsSystemTime)
    {
        for (auto strm : m_spStreamSinks)
        {
            strm->Stop(hnsSystemTime);
        }
        return S_OK;
    }
    STDMETHODIMP OnClockPause(MFTIME hnsSystemTime)
    {
        for (auto strm : m_spStreamSinks)
        {
            strm->Pause(hnsSystemTime);
        }
        return S_OK;
    }
    STDMETHODIMP OnClockRestart(MFTIME hnsSystemTime)
    {
        for (auto strm : m_spStreamSinks)
        {
            strm->Start(hnsSystemTime, 0);
        }
        return S_OK;
    }
    STDMETHODIMP OnClockSetRate(MFTIME hnsSystemTime, float flRate)
    {
        return S_OK;
    }
};

IMFMediaSink* CreateRTPMediaSink(std::vector<IMFMediaType*> mediaTypes)
{
    return RTPMediaSink::CreateInstance(mediaTypes);
}