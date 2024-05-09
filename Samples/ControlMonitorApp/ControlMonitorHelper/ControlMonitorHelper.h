//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//

#pragma once
#include "ControlMonitorManager.g.h"

namespace winrt::ControlMonitorHelperWinRT::implementation
{
    inline winrt::hstring FormatString(LPCWSTR szFormat, ...)
    {
        WCHAR szBuffer[MAX_PATH] = { 0 };
        va_list pArgs;
        va_start(pArgs, szFormat);
        StringCbVPrintf(szBuffer, sizeof(szBuffer), szFormat, pArgs);
        va_end(pArgs);

        return winrt::hstring(szBuffer);
    };

    // Windows Studio Effects custom KsProperties live under this property set
    static GUID KSPROPERTYSETID_WindowsStudioEffects = { 0x1666d655, 0x21a6, 0x4982, 0x97, 0x28, 0x52, 0xc3, 0x9e, 0x86, 0x9f, 0x90 };

    struct ControlMonitorManager : ControlMonitorManagerT<ControlMonitorManager>
    {
        static winrt::ControlMonitorHelperWinRT::ControlMonitorManager CreateCameraControlMonitor(winrt::hstring symlink, array_view<winrt::ControlMonitorHelperWinRT::ControlData const> controls);
        ControlMonitorManager(winrt::hstring cameraSymbolicLink, array_view<winrt::ControlMonitorHelperWinRT::ControlData const> controls);
        virtual ~ControlMonitorManager() noexcept;
        void Stop();
        void Start();

    public:
        winrt::event_token CameraControlChanged(winrt::Windows::Foundation::EventHandler<ControlData> const& handler);
        void CameraControlChanged(winrt::event_token const& token) noexcept;
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
            winrt::event<Windows::Foundation::EventHandler<ControlData>> m_cameraControlChangedEvt;
            ControlMonitorManager* m_pParent = nullptr;
        };
        winrt::com_ptr<CameraControlCallback> m_spCallback;
        wil::com_ptr<IMFCameraControlMonitor> m_spMonitor;
        winrt::array_view<ControlData const> m_controls;
    };
}

namespace winrt::ControlMonitorHelperWinRT::factory_implementation
{
    struct ControlMonitorManager : ControlMonitorManagerT<ControlMonitorManager, implementation::ControlMonitorManager>
    {
    };
}
