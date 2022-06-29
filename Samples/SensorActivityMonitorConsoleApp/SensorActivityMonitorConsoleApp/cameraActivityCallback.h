// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

class MyCameraNotificationCallback : public winrt::implements <MyCameraNotificationCallback, IMFSensorActivitiesReportCallback>
{
public:
    MyCameraNotificationCallback() = default;
    virtual ~MyCameraNotificationCallback() = default;

    // IMFSensorActivitiesReportCallback
    IFACEMETHODIMP OnActivitiesReport(_In_ IMFSensorActivitiesReport* sensorActivitiesReport) override;

    HRESULT Initialize(_In_z_ LPCWSTR symbolicName);

    bool IsInUse();

private:

    WCHAR   _symbolicName[MAX_PATH] = {};
    bool    _inUse = false;
    HANDLE  _event = nullptr;
};

