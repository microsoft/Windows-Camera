// Copyright (C) Microsoft Corporation. All rights reserved.

#include "pch.h"

using namespace winrt::Windows::Devices::Enumeration;

HRESULT IsCameraInUse(
    _In_z_ LPCWSTR symbolicName,
    bool& inUse
)
{
    winrt::com_ptr<MyCameraNotificationCallback> cameraNotificationCallback;
    wil::com_ptr_nothrow<IMFSensorActivityMonitor> activityMonitor;
    wil::com_ptr_nothrow<IMFShutdown> spShutdown;

    cameraNotificationCallback = winrt::make_self<MyCameraNotificationCallback>();
    RETURN_IF_FAILED(cameraNotificationCallback->Initialize(symbolicName));

    // Create the IMFSensorActivityMonitor, passing in the IMFSensorActivitiesReportCallback.
    RETURN_IF_FAILED(MFCreateSensorActivityMonitor(cameraNotificationCallback.get(), &activityMonitor));

    // Start the monitor
    RETURN_IF_FAILED(activityMonitor->Start());

    // Call the method that checks to if the monitored device is in use.
    inUse = cameraNotificationCallback->IsInUse();

    // Stop the activity monitor.
    RETURN_IF_FAILED(activityMonitor->Stop());

    // Shutdown the monitor.
    RETURN_IF_FAILED(activityMonitor.query_to(&spShutdown));

    RETURN_IF_FAILED(spShutdown->Shutdown());

    return S_OK;;
}

int main()
{
    // Print every WIL log message to standard error.
    // https://github.com/microsoft/wil/wiki/Error-logging-and-observation#setresultloggingcallback-example
    wil::SetResultLoggingCallback([](wil::FailureInfo const& failure) noexcept
        {
            constexpr std::size_t sizeOfLogMessageWithNul = 2048;

            wchar_t logMessage[sizeOfLogMessageWithNul];
            if (SUCCEEDED(wil::GetFailureLogString(logMessage, sizeOfLogMessageWithNul, failure)))
            {
                std::fputws(logMessage, stderr);
            }
        });

    winrt::init_apartment();

    THROW_IF_FAILED(::MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET));
    auto cleanup = wil::scope_exit([&]
        {
            THROW_IF_FAILED(::MFShutdown());
        });

    auto devices = DeviceInformation::FindAllAsync(DeviceClass::VideoCapture).get();
    if (devices.Size() == 0)
    {
        printf("No cameras found, exiting");
        return 0;
    }

    for (auto&& device : devices)
    {
        printf("Found camera [%ws] \n", device.Id().c_str());

        bool inUse = false;
        THROW_IF_FAILED(IsCameraInUse(device.Id().c_str(), inUse));

        printf("Camera [%ws] %s \n", device.Id().c_str(), inUse ? "is in use" : "is not in use");
    }
}
