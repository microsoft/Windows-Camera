// Copyright (C) Microsoft Corporation. All rights reserved.

#include "pch.h"

using namespace Microsoft::WRL;

HRESULT MyCameraNotificationCallback::Initialize(_In_z_ LPCWSTR symbolicName)
{
    _event = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
    RETURN_LAST_ERROR_IF_NULL(_event);

    return StringCchCopy(_symbolicName, MAX_PATH, symbolicName);
}

IFACEMETHODIMP MyCameraNotificationCallback::OnActivitiesReport(_In_ IMFSensorActivitiesReport* sensorActivitiesReport)
{
    bool inUse = false;
    wil::com_ptr_nothrow<IMFSensorActivityReport> sensorActivity;
    ULONG totalReportCount = 0;

    // There's two ways to look the activity reports.
    // option 1. One can ask all the reports and iterate through them, and filter the results if wanted
    //
    // option 2. Get only the report for specific device by using the symbolic link name.

    // option 1:
    printf("\nIterating all activity reports \n");

    RETURN_IF_FAILED(sensorActivitiesReport->GetCount(&totalReportCount));
    
    printf("%s@%d: [totalcount=%lu] \n", __FUNCTION__, __LINE__, totalReportCount);

    for (ULONG idx = 0; idx < totalReportCount; idx++)
    {
        WCHAR symbolicName[MAX_PATH] = {};
        ULONG count = 0;
        wil::com_ptr_nothrow<IMFSensorActivityReport> activityReport;
        RETURN_IF_FAILED(sensorActivitiesReport->GetActivityReport(idx, &activityReport));

        RETURN_IF_FAILED(activityReport->GetSymbolicLink(symbolicName, MAX_PATH, &count));
        RETURN_IF_FAILED(activityReport->GetProcessCount(&count));

        printf("%s@%d: [symlink=%ws, processcount=%lu] \n", __FUNCTION__, __LINE__, symbolicName, count);
        for (ULONG i = 0; i < count; i++)
        {
            BOOL fStreaming = FALSE;
            wil::com_ptr_nothrow<IMFSensorProcessActivity> processActivity;

            RETURN_IF_FAILED(activityReport->GetProcessActivity(i, &processActivity));
            RETURN_IF_FAILED(processActivity->GetStreamingState(&fStreaming));
            printf("%s@%d: [streaming:%s] \n", __FUNCTION__, __LINE__, (fStreaming ? "true" :"false"));

            if (fStreaming)
            {
                inUse = true;
                break;
            }
        }
    }

    // option 2:
    printf("\nGetActivityReportByDeviceName [%ws] \n", _symbolicName);
    RETURN_IF_FAILED(sensorActivitiesReport->GetActivityReportByDeviceName(_symbolicName, &sensorActivity));
    
    ULONG cProcCount = 0;

    RETURN_IF_FAILED(sensorActivity->GetProcessCount(&cProcCount));
    for (ULONG i = 0; i < cProcCount; i++)
    {
        BOOL fStreaming = FALSE;
        wil::com_ptr_nothrow<IMFSensorProcessActivity> processActivity;

        RETURN_IF_FAILED(sensorActivity->GetProcessActivity(i, &processActivity));
        RETURN_IF_FAILED(processActivity->GetStreamingState(&fStreaming));
        printf("%s@%d: [streaming:%s] \n", __FUNCTION__, __LINE__, (fStreaming ? "true" : "false"));

        if (fStreaming)
        {
            inUse = true;
            break;
        }
    }

    // Set flag that the device is in use and then signal event
    _inUse = inUse;
    if (totalReportCount > 0)
    {
        SetEvent(_event);
    }

    return S_OK;
}

bool MyCameraNotificationCallback::IsInUse( )
{
    DWORD   dwWait = 0;

    dwWait = WaitForSingleObject(_event, 500);
    switch (dwWait)
    {
    case WAIT_OBJECT_0:
        return _inUse;
    default:
        return false;
    }
}
