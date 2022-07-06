// Copyright (C) Microsoft Corporation. All rights reserved.

#include "pch.h"


using namespace winrt::Windows::Devices::Enumeration;

HRESULT IsCameraInUse(
    _In_z_ LPCWSTR symbolicName,
    bool& inUse
)
{
    wil::com_ptr<MyCameraNotificationCallback> cameraNotificationCallback;
    wil::com_ptr_nothrow<IMFSensorActivityMonitor> activityMonitor;
    wil::com_ptr_nothrow<IMFShutdown> spShutdown;

    RETURN_IF_FAILED(MyCameraNotificationCallback::CreateInstance(symbolicName, &cameraNotificationCallback));

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

    return S_OK;
}

HRESULT MonitorCameras()
{
    wil::com_ptr<MyCameraNotificationCallback> cameraNotificationCallback;
    wil::com_ptr_nothrow<IMFSensorActivityMonitor> activityMonitor;
    wil::com_ptr_nothrow<IMFShutdown> spShutdown;

    RETURN_IF_FAILED(MyCameraNotificationCallback::CreateInstance(&cameraNotificationCallback));

    // Create the IMFSensorActivityMonitor, passing in the IMFSensorActivitiesReportCallback.
    RETURN_IF_FAILED(MFCreateSensorActivityMonitor(cameraNotificationCallback.get(), &activityMonitor));

    // Starting a new thread that will monitor camera activities until asked to stop.
    wil::slim_event stopEvt;

    std::thread t1([&] {
        // Start the monitor
        THROW_IF_FAILED(activityMonitor->Start());

        stopEvt.wait();

        THROW_IF_FAILED(activityMonitor->Stop());

        });

    bool stop = false;

    do
    {
        std::wstring input;
        printf("\n To stop monitor press 'q' \n");
        std::wcin >> input;

        if (input == L"q")
        {
            stop = true;
        }

    } while (!stop);

    stopEvt.SetEvent();

    t1.join();

    // Shutdown the monitor.
    RETURN_IF_FAILED(activityMonitor.query_to(&spShutdown));

    RETURN_IF_FAILED(spShutdown->Shutdown());

    return S_OK;
}

int promptMenu()
{
    int selection = 0;
    printf("\n select option: \n\t 1 - check if camera is in use \n\t 2 - monitor all camera usage \n");
    std::wcin >> selection;

    return selection;
}

void DoOneShotInUseCheck()
{
    auto devices = DeviceInformation::FindAllAsync(DeviceClass::VideoCapture).get();
    if (devices.Size() == 0)
    {
        printf("No cameras found, exiting");
        return;
    }

    int selectedCamera = 0;

    // Show list of all cameras and do in use check for the selection
    printf("Select camera: \n");
    int index = 0;
    for (auto&& device : devices)
    {
        printf("\n\t [%d]\t %ws \t [%ws]", index++, device.Name().c_str(), device.Id().c_str());
    }
    printf("\n");
    std::wcin >> selectedCamera;
    auto device = devices.GetAt(selectedCamera);

    bool inUse = false;
    THROW_IF_FAILED(IsCameraInUse(device.Id().c_str(), inUse));

    printf("\n%ws [%ws] %s \n", device.Name().c_str(), device.Id().c_str(), inUse ? "is in use" : "is not in use");
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

    auto selection = promptMenu();

    switch (selection)
    {
    case 1:
    {
        DoOneShotInUseCheck();
        break;
    }
    case 2:
    {
        //  Start thread to monitor camera activities
        THROW_IF_FAILED(MonitorCameras());
        break;
    }
    default:
        printf("Bad user input, exiting");
        break;
    }
}
