// Copyright (C) Microsoft Corporation. All rights reserved.

#include "pch.h"
#include "VirtualCameraRegistrar.h"
#include "VirtualCameraRegistrar.g.cpp"
#include "VirtualCameraProxy.h"
#include "Common.h"
#include "mfidl.h"

#include "winrt/Windows.Media.Devices.h"


namespace winrt::VirtualCameraManager_WinRT::implementation
{
    /// <summary>
    /// Create a virtual camera instance. 
    /// Note that the virtual camera instance gets registered only upon calling Start().
    /// </summary>
    /// <param name="strMediaSourceGuid"></param>
    /// <param name="lifetime"></param>
    /// <param name="access"></param>
    /// <param name="friendlyName"></param>
    /// <param name="strWrappedCameraSymbolicLink"></param>
    /// <param name="spVirtualCamera"></param>
    void CreateVirtualCamera(
        _In_ winrt::hstring const& strMediaSourceGuid,
        _In_ MFVirtualCameraLifetime lifetime,
        _In_ MFVirtualCameraAccess access,
        _In_ winrt::hstring const& friendlyName,
        _In_ winrt::VirtualCameraManager_WinRT::VirtualCameraKind const& virtualCameraKind,
        _In_ winrt::hstring const& strWrappedCameraSymbolicLink,
        _Inout_ wil::com_ptr_nothrow<IMFVirtualCamera>& spVirtualCamera)
    {
        if (spVirtualCamera.get() != nullptr)
        {
            spVirtualCamera->Stop();
        }
        // create the virtual camera
        THROW_IF_FAILED_MSG(MFCreateVirtualCamera(
            MFVirtualCameraType_SoftwareCameraSource,
            lifetime, // MFVirtualCameraLifetime_*
            access, // MFVirtualCameraAccess_* , note that if running from UWP app, system will throw an access_denied upon enabling the virtual camera
            friendlyName.data(),
            strMediaSourceGuid.data(),
            nullptr, /*Categories*/
            0,       /*CategoryCount*/
            &spVirtualCamera),
            "Failed to create virtual camera");

        // Create custom attribute, to be used by mediasource on activation to specify what kind of virtual camera we are creating
        THROW_IF_FAILED_MSG(spVirtualCamera->SetUINT32(VCAM_KIND, (UINT32)virtualCameraKind),
            "Failed to add device source info attribute onto the virtual camera");

        // if wrapping an existing camera, specify its symbolic link explicitly to the virtual camera 
        // and also store it into an IMFAttribute on the virtual camera
        if (!strWrappedCameraSymbolicLink.empty())
        {
            THROW_IF_FAILED_MSG(spVirtualCamera->AddDeviceSourceInfo(strWrappedCameraSymbolicLink.data()),
                "Failed to add device source info of existing camera to virtual camera");

            // Create custom attribute, to be used by mediasource on activation
            THROW_IF_FAILED_MSG(spVirtualCamera->SetString(VCAM_DEVICE_INFO, strWrappedCameraSymbolicLink.data()),
                "Failed to add device source info attribute onto the virtual camera");

            
            // --> Example code below that can be uncommented to set PLD information for the virtual camera (app registering the virtual camera needs high privilege)
            // 
            
            // attempt to set PLD info making this camera a front-facing camera 
            //ACPI_PLD_V2_BUFFER acpiPld = {};    /// See acpitable.h for definition.

            //// Initialize the PLD information.  All the other fields can be
            //// 0, except for the revision and the vertical/horizontal 
            //// offsets, those have to be initialized to describe the PLD 
            //// structure type and "no offset" value. 
            //acpiPld.Revision = 2;
            //acpiPld.VerticalOffset = 0xFFFF;
            //acpiPld.HorizontalOffset = 0xFFFF;

            //// Now set the PLD panel to indicate this camera is located on the front panel 
            //acpiPld.Panel = 4;// AcpiPldPanelFront

            //// swallow error (i.e. in case the registering app does not have high privilege)
            //// THROW_IF_FAILED_MSG(
            //spVirtualCamera->AddProperty((const ::DEVPROPKEY*)&DEVPKEY_Device_PhysicalDeviceLocation,
            //    DEVPROP_TYPE_BINARY,
            //    (const BYTE*)&acpiPld,
            //    sizeof(acpiPld));
            //"Failed to add acpiPld device property onto the virtual camera");

            // <-- end of example
            //
        }
    }

    winrt::VirtualCameraManager_WinRT::VirtualCameraProxy
        VirtualCameraRegistrar::RegisterNewVirtualCamera(
            winrt::VirtualCameraManager_WinRT::VirtualCameraKind const& virtualCameraKind,
            winrt::VirtualCameraManager_WinRT::VirtualCameraLifetime const& lifetime,
            winrt::VirtualCameraManager_WinRT::VirtualCameraAccess const& access,
            hstring const& friendlyName,
            hstring const& wrappedCameraSymbolicLink)
    {
        wil::com_ptr_nothrow<IMFVirtualCamera> spVirtualCamera;

        CreateVirtualCamera(
            VIRTUALCAMERAMEDIASOURCE_CLSID,
            (MFVirtualCameraLifetime)lifetime,
            (MFVirtualCameraAccess)access,
            friendlyName,
            virtualCameraKind,
            wrappedCameraSymbolicLink,
            spVirtualCamera);

        auto result = make<implementation::VirtualCameraProxy>(
            virtualCameraKind,
            lifetime,
            access,
            wrappedCameraSymbolicLink,
            spVirtualCamera);

        return result;
    }

    winrt::VirtualCameraManager_WinRT::VirtualCameraProxy
        VirtualCameraRegistrar::RetakeExistingVirtualCamera(
            VirtualCameraKind virtualCameraKind,
            VirtualCameraLifetime lifetime,
            VirtualCameraAccess access,
            hstring const& friendlyName,
            hstring const& symbolicLink,
            hstring const& wrappedCameraSymbolicLink)
    {
        wil::com_ptr_nothrow<IMFVirtualCamera> spVirtualCamera;

        CreateVirtualCamera(
            VIRTUALCAMERAMEDIASOURCE_CLSID,
            (MFVirtualCameraLifetime)lifetime,
            (MFVirtualCameraAccess)access,
            friendlyName,
            virtualCameraKind,
            wrappedCameraSymbolicLink,
            spVirtualCamera);

        auto result = make<implementation::VirtualCameraProxy>(
            virtualCameraKind,
            lifetime,
            access,
            friendlyName,
            symbolicLink,
            wrappedCameraSymbolicLink,
            spVirtualCamera);

        return result;
    }

    winrt::Windows::Foundation::Collections::IVector<winrt::Windows::Devices::Enumeration::DeviceInformation>
        VirtualCameraRegistrar::GetExistingVirtualCameraDevices()
    {
        auto returnList = single_threaded_vector<winrt::Windows::Devices::Enumeration::DeviceInformation>();

        auto taskResult = winrt::Windows::Devices::Enumeration::DeviceInformation::FindAllAsync(
            winrt::Windows::Media::Devices::MediaDevice::GetVideoCaptureSelector());

        // we wait for max 20s to fetch the list of available camera devices
        static const int waitTimeInSeconds = 20;
        switch (taskResult.wait_for(std::chrono::seconds(waitTimeInSeconds)))
        {
            case winrt::Windows::Foundation::AsyncStatus::Completed:
            case winrt::Windows::Foundation::AsyncStatus::Error:
                break;
            case winrt::Windows::Foundation::AsyncStatus::Canceled:
            case winrt::Windows::Foundation::AsyncStatus::Started:
                taskResult.Cancel();
                throw winrt::hresult_error(HRESULT_FROM_WIN32(ERROR_TIMEOUT), L"Cancel task timeout in seconds: " + winrt::to_hstring(waitTimeInSeconds));
        }

        winrt::Windows::Devices::Enumeration::DeviceInformationCollection deviceList{ nullptr };
        deviceList = taskResult.get();

        // for each camera device, check if it is a virtual camera by looking for a specific device interface and a DEVPROPKEY
        for (uint32_t i = 0; i < deviceList.Size(); i++)
        {
            static const winrt::hstring _DEVPKEY_DeviceInterface_IsVirtualCamera(L"{6EDC630D-C2E3-43B7-B2D1-20525A1AF120},3");
            auto device = deviceList.GetAt(i);
            winrt::Windows::Foundation::Collections::IVector<winrt::hstring> requestedProps{ winrt::single_threaded_vector<winrt::hstring>({_DEVPKEY_DeviceInterface_IsVirtualCamera}) };
            auto deviceInterface = winrt::Windows::Devices::Enumeration::DeviceInformation::CreateFromIdAsync(
                device.Id(),
                requestedProps,
                winrt::Windows::Devices::Enumeration::DeviceInformationKind::DeviceInterface
            ).get();

            if (deviceInterface.Properties().HasKey(_DEVPKEY_DeviceInterface_IsVirtualCamera))
            {
                auto prop = deviceInterface.Properties().Lookup(_DEVPKEY_DeviceInterface_IsVirtualCamera);
                if (prop != nullptr)
                {
                    auto props = device.Properties();
                    returnList.Append(device);
                }
            }
        }
        return returnList;
    }
}
