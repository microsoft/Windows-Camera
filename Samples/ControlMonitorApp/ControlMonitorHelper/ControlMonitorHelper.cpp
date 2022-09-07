//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//

#include "pch.h"
#include "ControlMonitorHelper.h"
#include "ControlMonitorManager.g.cpp"

namespace winrt::ControlMonitorHelper::implementation
{
    winrt::ControlMonitorHelper::ControlMonitorManager ControlMonitorManager::CreateCameraControlMonitor(winrt::hstring symlink, winrt::array_view<winrt::ControlMonitorHelper::ControlData const> controls)
    {
        auto result = winrt::make<ControlMonitorManager>(symlink, controls);
        return result;
    }

    ControlMonitorManager::ControlMonitorManager(winrt::hstring cameraSymbolicLink, array_view<winrt::ControlMonitorHelper::ControlData const> controls)
    {
        m_spCallback = winrt::make_self<CameraControlCallback>(this);

        HRESULT hr = MFCreateCameraControlMonitor(cameraSymbolicLink.c_str(), m_spCallback.get(), &m_spMonitor);
        if (FAILED(hr))
        {
            throw winrt::hresult_error(hr, L"error from MFCreateCameraControlMonitor..");
        }

        m_controls = controls;

        for each (auto&& var in m_controls)
        {
            GUID guid{};
            switch (var.controlKind)
            {
            case ControlKind::VidCapCameraControlKind:
                guid = PROPSETID_VIDCAP_CAMERACONTROL;
                break;
            case ControlKind::VidCapVideoProcAmpKind:
                guid = PROPSETID_VIDCAP_VIDEOPROCAMP;
                break;
            case ControlKind::ExtendedControlKind:
                guid = KSPROPERTYSETID_ExtendedCameraControl;
                break;
            default:
                throw winrt::hresult_error(E_UNEXPECTED, L"Invalid ControlKind");
            }

            hr = m_spMonitor->AddControlSubscription(guid, var.controlId);
            if (FAILED(hr))
            {
                throw winrt::hresult_error(hr, L"error from IMFCameraControlMonitor::AddControlSubscription(PROPSETID_VIDCAP_VIDEOPROCAMP, 0)..");
            }
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
            for each (auto && var in m_controls)
            {
                GUID guid{};
                switch (var.controlKind)
                {
                case ControlKind::VidCapCameraControlKind:
                    guid = PROPSETID_VIDCAP_CAMERACONTROL;
                    break;
                case ControlKind::VidCapVideoProcAmpKind:
                    guid = PROPSETID_VIDCAP_VIDEOPROCAMP;
                    break;
                case ControlKind::ExtendedControlKind:
                    guid = KSPROPERTYSETID_ExtendedCameraControl;
                    break;
                default:
                    throw winrt::hresult_error(E_UNEXPECTED, L"Invalid ControlKind");
                }

                THROW_IF_FAILED(m_spMonitor->RemoveControlSubscription(guid, var.controlId));
            }
            
            m_spMonitor = nullptr;
        }
    } CATCH_LOG()

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
            ControlData control;
            control.controlKind = ControlKind::VidCapVideoProcAmpKind;
            control.controlId = id;
            m_cameraControlChangedEvt(*m_pParent, control);
        }
        else if (controlSet == PROPSETID_VIDCAP_CAMERACONTROL)
        {
            ControlData control;
            control.controlKind = ControlKind::VidCapCameraControlKind;
            control.controlId = id;
            m_cameraControlChangedEvt(*m_pParent, control);
        }
        else if (controlSet == KSPROPERTYSETID_ExtendedCameraControl)
        {
            ControlData control;
            control.controlKind = ControlKind::ExtendedControlKind;
            control.controlId = id;
            m_cameraControlChangedEvt(*m_pParent, control);
        }
    }

    IFACEMETHODIMP_(void) ControlMonitorManager::CameraControlCallback::OnError(_In_ HRESULT hrStatus)
    {
        UNREFERENCED_PARAMETER(hrStatus);
    }

    winrt::event_token ControlMonitorManager::CameraControlChanged(winrt::Windows::Foundation::EventHandler<ControlData> const& handler)
    {
        return m_spCallback->m_cameraControlChangedEvt.add(handler);
    }
    void ControlMonitorManager::CameraControlChanged(winrt::event_token const& token) noexcept
    {
        m_spCallback->m_cameraControlChangedEvt.remove(token);
    }
}
