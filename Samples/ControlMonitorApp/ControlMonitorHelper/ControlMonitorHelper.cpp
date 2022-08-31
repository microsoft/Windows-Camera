//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//

#include "pch.h"
#include "ControlMonitorHelper.h"
#include "ControlMonitorManager.g.cpp"

namespace winrt::ControlMonitorHelper::implementation
{
    winrt::ControlMonitorHelper::ControlMonitorManager ControlMonitorManager::CreateCameraControlMonitor(winrt::hstring symlink)
    {
        auto result = winrt::make<ControlMonitorManager>(symlink);
        return result;
    }

    ControlMonitorManager::ControlMonitorManager(winrt::hstring cameraSymbolicLink)
    {
        m_spCallback = winrt::make_self<CameraControlCallback>(this);

        HRESULT hr = MFCreateCameraControlMonitor(cameraSymbolicLink.c_str(), m_spCallback.get(), &m_spMonitor);
        if (FAILED(hr))
        {
            throw winrt::hresult_error(hr, L"error from MFCreateCameraControlMonitor..");
        }

        hr = m_spMonitor->AddControlSubscription(PROPSETID_VIDCAP_VIDEOPROCAMP, KSPROPERTY_VIDEOPROCAMP_CONTRAST);
        if (FAILED(hr))
        {
            throw winrt::hresult_error(hr, L"error from IMFCameraControlMonitor::AddControlSubscription(PROPSETID_VIDCAP_VIDEOPROCAMP, 0)..");
        }

        hr = m_spMonitor->Start();
        if (FAILED(hr))
        {
            throw winrt::hresult_error(hr, L"error from Start IMFCameraControlMonitor..");
        }
    }

    ControlMonitorManager::~ControlMonitorManager() noexcept try
    {
        if (m_spMonitor != nullptr)
        {
            THROW_IF_FAILED(m_spMonitor->Stop());
            THROW_IF_FAILED(m_spMonitor->RemoveControlSubscription(PROPSETID_VIDCAP_VIDEOPROCAMP, KSPROPERTY_VIDEOPROCAMP_CONTRAST));
            m_spMonitor = nullptr;
        }
    } CATCH_FAIL_FAST()

    void ControlMonitorManager::Stop()
    {
        HRESULT hr = m_spMonitor->Stop();
        if (FAILED(hr))
        {
            throw winrt::hresult_error(hr, L"could not Stop IMFCameraControlMonitor..");
        }
    }
    void ControlMonitorManager::Start()
    {
        HRESULT hr = m_spMonitor->Start();
        if (FAILED(hr))
        {
            throw winrt::hresult_error(hr, L"could not Start IMFCameraControlMonitor..");
        }
    }

    //
    // IMFCameraControlNotify
    //
    IFACEMETHODIMP_(void) ControlMonitorManager::CameraControlCallback::OnChange(_In_ REFGUID controlSet, _In_ UINT32 id)
    {
        if (controlSet == PROPSETID_VIDCAP_VIDEOPROCAMP)
        {
            m_vidCapCameraControlChangedEvent(*m_pParent, id);
        }
    }

    IFACEMETHODIMP_(void) ControlMonitorManager::CameraControlCallback::OnError(_In_ HRESULT hrStatus)
    {
        UNREFERENCED_PARAMETER(hrStatus);
    }

    winrt::event_token ControlMonitorManager::VidCapCameraControlChanged(winrt::Windows::Foundation::EventHandler<uint32_t> const& handler)
    {
        return m_spCallback->m_vidCapCameraControlChangedEvent.add(handler);
    }
    void ControlMonitorManager::VidCapCameraControlChanged(winrt::event_token const& token) noexcept
    {
        m_spCallback->m_vidCapCameraControlChangedEvent.remove(token);
    }
}
