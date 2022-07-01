// Copyright (C) Microsoft Corporation. All rights reserved.

#include "pch.h"

// Utility functions
namespace 
{
    HRESULT PrintProcessActivity(_In_ IMFSensorProcessActivity* processActivity)
    {
        BOOL fStreaming = FALSE;
        MFSensorDeviceMode mode = MFSensorDeviceMode::MFSensorDeviceMode_Controller;
        ULONG pid = 0;

        RETURN_IF_FAILED(processActivity->GetStreamingState(&fStreaming));
        RETURN_IF_FAILED(processActivity->GetStreamingMode(&mode));
        RETURN_IF_FAILED(processActivity->GetProcessId(&pid));

        printf("\t Process %d [streaming:%s,mode:%s] \n", pid, (fStreaming ? "true" : "false"), ((mode == MFSensorDeviceMode_Controller) ? "controller" : "shared"));
    }
}

HRESULT MyCameraNotificationCallback::CreateInstance(_In_z_ LPCWSTR symbolicName, _COM_Outptr_ MyCameraNotificationCallback** value) noexcept try
{
    auto callback = winrt::make_self<MyCameraNotificationCallback>();
    RETURN_IF_FAILED(callback->Initialize(symbolicName));
    *value = callback.detach();

    return S_OK;
}
CATCH_RETURN()

HRESULT MyCameraNotificationCallback::CreateInstance(_COM_Outptr_ MyCameraNotificationCallback** value) noexcept try
{
    auto callback = winrt::make_self<MyCameraNotificationCallback>();
    RETURN_IF_FAILED(callback->Initialize(nullptr));
    *value = callback.detach();

    return S_OK;
}
CATCH_RETURN()

HRESULT MyCameraNotificationCallback::Initialize(_In_opt_z_ LPCWSTR symbolicName)
{
    if (symbolicName)
    {
        _type = CallbackType::OneShot;
        RETURN_IF_FAILED(StringCchCopy(_symbolicName, MAX_PATH, symbolicName));
    }
    else
    {
        _type = CallbackType::Monitor;
    }

    return S_OK;
}

IFACEMETHODIMP MyCameraNotificationCallback::OnActivitiesReport(_In_ IMFSensorActivitiesReport* sensorActivitiesReport)
{
    ULONG totalReportCount = 0;

    // Early exit if we did not have any reports
    RETURN_IF_FAILED(sensorActivitiesReport->GetCount(&totalReportCount));
    if (totalReportCount == 0)
    {
        printf("\nNo reports\n");
        return S_OK;
    }

    switch (_type)
    {
    case MyCameraNotificationCallback::CallbackType::OneShot:
        RETURN_IF_FAILED_WITH_EXPECTED(HandleOneShotReport(sensorActivitiesReport), MF_E_NOT_FOUND);
        break;
    case MyCameraNotificationCallback::CallbackType::Monitor:
        RETURN_IF_FAILED(HandleMonitorReport(sensorActivitiesReport, totalReportCount));
        break;
    default:
        break;
    }

    return S_OK;
}

bool MyCameraNotificationCallback::IsInUse( )
{
    if (_event.wait(500))
    {
        return _inUse;
    }

    return false;
}

HRESULT MyCameraNotificationCallback::HandleOneShotReport(_In_ IMFSensorActivitiesReport* sensorActivitiesReport)
{
    bool inUse = false;
    wil::com_ptr_nothrow<IMFSensorActivityReport> sensorActivity;
    ULONG cProcCount = 0;

    printf("\nGetActivityReportByDeviceName [%ws] \n", _symbolicName);
    RETURN_IF_FAILED_WITH_EXPECTED(sensorActivitiesReport->GetActivityReportByDeviceName(_symbolicName, &sensorActivity),MF_E_NOT_FOUND);

    RETURN_IF_FAILED(sensorActivity->GetProcessCount(&cProcCount));
    for (ULONG i = 0; i < cProcCount; i++)
    {
        BOOL fStreaming = FALSE;
        wil::com_ptr_nothrow<IMFSensorProcessActivity> processActivity;

        RETURN_IF_FAILED(sensorActivity->GetProcessActivity(i, &processActivity));
        RETURN_IF_FAILED(PrintProcessActivity(processActivity.get()));
        
        RETURN_IF_FAILED(processActivity->GetStreamingState(&fStreaming));

        if (fStreaming)
        {
            inUse = true;
            break;
        }
    }

    // Set flag that the device is in use and then signal event
    _inUse = inUse;
    if (cProcCount > 0)
    {
        _event.SetEvent();
    }

    return S_OK;
}

HRESULT MyCameraNotificationCallback::HandleMonitorReport(_In_ IMFSensorActivitiesReport* sensorActivitiesReport, const ULONG reportCount)
{
    wil::com_ptr_nothrow<IMFSensorActivityReport> sensorActivity;

    printf("\n\nPrinting all reports[totalcount=%lu] \n", reportCount);

    for (ULONG idx = 0; idx < reportCount; idx++)
    {
        WCHAR symbolicName[MAX_PATH] = {};
        WCHAR friendlyName[MAX_PATH] = {};
        ULONG count = 0;
        ULONG written = 0;
        wil::com_ptr_nothrow<IMFSensorActivityReport> activityReport;
        RETURN_IF_FAILED(sensorActivitiesReport->GetActivityReport(idx, &activityReport));

        RETURN_IF_FAILED(activityReport->GetSymbolicLink(symbolicName, MAX_PATH, &written));
        RETURN_IF_FAILED(activityReport->GetProcessCount(&count));
        RETURN_IF_FAILED(activityReport->GetFriendlyName(friendlyName, MAX_PATH, &written));

        printf("%ws\n\t [symlink=%ws, processcount=%lu] \n", friendlyName, symbolicName, count);
        for (ULONG i = 0; i < count; i++)
        {
            wil::com_ptr_nothrow<IMFSensorProcessActivity> processActivity;

            RETURN_IF_FAILED(activityReport->GetProcessActivity(i, &processActivity));
            RETURN_IF_FAILED(PrintProcessActivity(processActivity.get()));
        }
    }
    return S_OK;
}