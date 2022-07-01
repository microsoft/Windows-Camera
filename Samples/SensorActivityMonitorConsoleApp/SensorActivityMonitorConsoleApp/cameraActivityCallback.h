// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

// This class uses C++/WinRT to implement Classic COM, see https://docs.microsoft.com/en-us/windows/uwp/cpp-and-winrt-apis/author-coclasses#enabling-classic-com-support
class MyCameraNotificationCallback : public winrt::implements <MyCameraNotificationCallback, IMFSensorActivitiesReportCallback>
{
public:
    
    static HRESULT CreateInstance(_In_z_ LPCWSTR symbolicName, _COM_Outptr_ MyCameraNotificationCallback** value) noexcept;
    static HRESULT CreateInstance(_COM_Outptr_ MyCameraNotificationCallback** value) noexcept;

    // IMFSensorActivitiesReportCallback
    IFACEMETHODIMP OnActivitiesReport(_In_ IMFSensorActivitiesReport* sensorActivitiesReport) override;

    bool IsInUse();

private:

    enum class CallbackType
    {
        OneShot,
        Monitor
    };

    HRESULT Initialize(_In_opt_z_ LPCWSTR symbolicName);
    HRESULT HandleOneShotReport(_In_ IMFSensorActivitiesReport* sensorActivitiesReport);
    HRESULT HandleMonitorReport(_In_ IMFSensorActivitiesReport* sensorActivitiesReport, const ULONG reportCount);


    WCHAR   _symbolicName[MAX_PATH] = {};
    bool    _inUse = false;
    wil::slim_event  _event;
    CallbackType _type = CallbackType::OneShot;
};

