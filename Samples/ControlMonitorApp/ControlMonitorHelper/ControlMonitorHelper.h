//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//

#pragma once
#include "ControlMonitorManager.g.h"

namespace winrt::ControlMonitorHelper::implementation
{
    struct ControlMonitorManager : ControlMonitorManagerT<ControlMonitorManager>
    {
        static winrt::ControlMonitorHelper::ControlMonitorManager CreateCameraControlMonitor(winrt::hstring symlink);
        ControlMonitorManager(winrt::hstring cameraSymbolicLink);
        virtual ~ControlMonitorManager() noexcept;
        void Stop();
        void Start();

    public:
        winrt::event_token VidCapCameraControlChanged(winrt::Windows::Foundation::EventHandler<uint32_t> const& handler);
        void VidCapCameraControlChanged(winrt::event_token const& token) noexcept;
    private:

        struct CameraControlCallback : public winrt::implements<CameraControlCallback, IMFCameraControlNotify>
        {
            CameraControlCallback(ControlMonitorManager* parent)
            {
                m_pParent = parent;
            }

            //
            // IMFCameraControlNotify
            IFACEMETHODIMP_(void) OnChange(_In_ REFGUID controlSet, _In_ UINT32 id) override;
            IFACEMETHODIMP_(void) OnError(_In_ HRESULT hrStatus) override;

            LONG m_lRef = 0;
            wil::critical_section m_lock;
            winrt::event<Windows::Foundation::EventHandler<uint32_t>> m_vidCapCameraControlChangedEvent;
            ControlMonitorManager* m_pParent = nullptr;
        };
        winrt::com_ptr<CameraControlCallback> m_spCallback;
        wil::com_ptr<IMFCameraControlMonitor> m_spMonitor;
    };
}

namespace winrt::ControlMonitorHelper::factory_implementation
{
    struct ControlMonitorManager : ControlMonitorManagerT<ControlMonitorManager, implementation::ControlMonitorManager>
    {
    };
}
