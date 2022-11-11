//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//

#include "pch.h"
#include "DefaultControlHelper.h"
#include "DefaultControlManager.g.cpp"
#include "DefaultController.g.cpp"

namespace winrt::DefaultControlHelper::implementation
{
#pragma region DefaultControlManager

    DefaultControlManager::DefaultControlManager(winrt::hstring cameraSymbolicLink)
    {
        m_configManager = wil::CoCreateInstance<IMFCameraConfigurationManager>(CLSID_CameraConfigurationManager);

        wil::com_ptr<IMFAttributes> attributes;
        THROW_IF_FAILED(MFCreateAttributes(&attributes, 1));
        THROW_IF_FAILED(attributes->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, cameraSymbolicLink.c_str()));

        THROW_IF_FAILED(m_configManager->LoadDefaults(attributes.get(), &m_controlDefaultsCollection));
    }

    winrt::DefaultControlHelper::DefaultController DefaultControlManager::CreateController(DefaultControllerType type, uint32_t id)
    {
        return winrt::make<DefaultController>(type, id, get_weak());
    }

    HRESULT DefaultControlManager::Save()
    {
        return m_configManager->SaveDefaults(m_controlDefaultsCollection.get());
    }

#pragma endregion

#pragma region DefaultController

    DefaultController::DefaultController(winrt::DefaultControlHelper::DefaultControllerType type,const uint32_t id, winrt::weak_ref<DefaultControlManager> manager)
    {
        m_id = id;
        m_controlManager = manager;

        // Creating an underlying helper controller based on the asked control type
        switch (type)
        {
        case winrt::DefaultControlHelper::DefaultControllerType::VideoProcAmp:
            m_internalController = std::make_unique< DefaultControllerVidCapVideoProcAmp>();
            break;
        
        case winrt::DefaultControlHelper::DefaultControllerType::CameraControl:
            m_internalController = std::make_unique<DefaultControllerCameraControl>();
            break;
        
        case winrt::DefaultControlHelper::DefaultControllerType::ExtendedCameraControl:
            switch (id)
            {
                case KSPROPERTY_CAMERACONTROL_EXTENDED_EVCOMPENSATION:
                    m_internalController = std::make_unique<DefaultControllerEVCompExtendedControl>();
                    break;

                default: 
                    m_internalController = std::make_unique<DefaultControllerExtendedControl>();
                    break;
            }
            
            break;
        default:
            break;
        }

        auto strongManager{ m_controlManager.get() };
        THROW_HR_IF_NULL(E_POINTER, strongManager);

        m_internalController->Initialize(strongManager, m_controlDefaults, id);

        // Getting range info if available
        MF_CAMERA_CONTROL_RANGE_INFO rangeInfo = {};

        try
        {
            THROW_IF_FAILED(m_controlDefaults->GetRangeInfo(&rangeInfo));
            m_rangeInfo.emplace(rangeInfo);
        }
        catch (const wil::ResultException& e)
        {
            // GetRangeInfo will return MF_E_NOT_FOUND if the control does
            // not support range information.
            if (e.GetErrorCode() != MF_E_NOT_FOUND)
            {
                THROW_EXCEPTION(e);
            }
        }
    };

    uint32_t DefaultController::DefaultValue()
    {
        KSPROPERTY_VIDEOPROCAMP_S* pControl = nullptr;
        ULONG                       cbControl = 0;
        KSPROPERTY_VIDEOPROCAMP_S* pData = nullptr;
        ULONG                       cbData = 0;

        uint32_t valueOut = 0;

        THROW_IF_FAILED(m_controlDefaults->LockControlData((void**)&pControl, &cbControl, (void**)&pData, &cbData));
        {
            auto unlock = wil::scope_exit([&]()
            {
                try
                {
                    // The Unlock call can fail if there's no existing configuration or the required flags/capabilities
                    // are otherwise wrong or unset.
                    THROW_IF_FAILED(m_controlDefaults->UnlockControlData());
                }
                catch (...)
                {
                    if (m_rangeInfo.has_value())
                    {
                        valueOut = m_rangeInfo->defaultValue;
                    }
                }
            });

            valueOut = pControl->Value;
        }

        return m_defaultValue = valueOut;
    }

    void DefaultController::DefaultValue(uint32_t const& value)
    {
        m_internalController->SaveDefault(m_controlDefaults, value);

        auto strongManager = m_controlManager.get();
        THROW_HR_IF_NULL(HRESULT_FROM_WIN32(ERROR_INVALID_STATE), strongManager);
        

        THROW_IF_FAILED(strongManager->Save());
        m_defaultValue = value;
    }

#pragma endregion

    void DefaultControllerVidCapVideoProcAmp::Initialize(const winrt::com_ptr<DefaultControlManager>& manager, wil::com_ptr<IMFCameraControlDefaults>& controlDefaults, const uint32_t id)
    {
        auto collection = manager->GetCollection();

        THROW_IF_FAILED(collection->GetOrAddControl(
            MF_CAMERA_CONTROL_CONFIGURATION_TYPE_POSTSTART,
            m_typeGuid,
            id,
            sizeof(KSPROPERTY_VIDEOPROCAMP_S),
            sizeof(KSPROPERTY_VIDEOPROCAMP_S),
            &controlDefaults));
    }

    void DefaultControllerVidCapVideoProcAmp::SaveDefault(const wil::com_ptr<IMFCameraControlDefaults>& controlDefaults, const uint32_t value)
    {
        KSPROPERTY_VIDEOPROCAMP_S* pControl = nullptr;
        ULONG                       cbControl = 0;
        KSPROPERTY_VIDEOPROCAMP_S* pData = nullptr;
        ULONG                       cbData = 0;

        THROW_IF_FAILED(controlDefaults->LockControlData((void**)&pControl, &cbControl, (void**)&pData, &cbData));

        pControl->Flags = KSPROPERTY_VIDEOPROCAMP_FLAGS_MANUAL;
        pControl->Property.Flags = KSPROPERTY_TYPE_SET;

        pControl->Value = value;

        pData->Property.Set = pControl->Property.Set;
        pData->Property.Id = pControl->Property.Id;
        pData->Property.Flags = pControl->Property.Flags;
        pData->Value = pControl->Value;
        pData->Flags = pControl->Flags;

        THROW_IF_FAILED(controlDefaults->UnlockControlData());
        
    }

    void DefaultControllerCameraControl::Initialize(const winrt::com_ptr<DefaultControlManager>& manager, wil::com_ptr<IMFCameraControlDefaults>& controlDefaults, const uint32_t id)
    {
        auto collection = manager->GetCollection();

        THROW_IF_FAILED(collection->GetOrAddControl(
            MF_CAMERA_CONTROL_CONFIGURATION_TYPE_POSTSTART,
            m_typeGuid,
            id,
            sizeof(KSPROPERTY_CAMERACONTROL_S),
            sizeof(KSPROPERTY_CAMERACONTROL_S),
            &controlDefaults));
    }

    void DefaultControllerCameraControl::SaveDefault(const wil::com_ptr<IMFCameraControlDefaults>& controlDefaults, const uint32_t value)
    {
        KSPROPERTY_CAMERACONTROL_S* pControl = nullptr;
        ULONG                       cbControl = 0;
        KSPROPERTY_CAMERACONTROL_S* pData = nullptr;
        ULONG                       cbData = 0;

        THROW_IF_FAILED(controlDefaults->LockControlData((void**)&pControl, &cbControl, (void**)&pData, &cbData));

        pControl->Flags = KSPROPERTY_CAMERACONTROL_FLAGS_MANUAL;
        pControl->Property.Flags = KSPROPERTY_TYPE_SET;

        pControl->Value = value;

        pData->Property.Set = pControl->Property.Set;
        pData->Property.Id = pControl->Property.Id;
        pData->Property.Flags = pControl->Property.Flags;
        pData->Value = pControl->Value;
        pData->Flags = pControl->Flags;

        THROW_IF_FAILED(controlDefaults->UnlockControlData());
    }


    void DefaultControllerExtendedControl::Initialize(const winrt::com_ptr<DefaultControlManager>& manager, wil::com_ptr<IMFCameraControlDefaults>& controlDefaults, const uint32_t id)
    {
        auto collection = manager->GetCollection();

        THROW_IF_FAILED(collection->GetOrAddExtendedControl(
            MF_CAMERA_CONTROL_CONFIGURATION_TYPE_POSTSTART, //
            id,
            KSCAMERA_EXTENDEDPROP_FILTERSCOPE,
            sizeof(KSCAMERA_EXTENDEDPROP_HEADER) + sizeof(KSCAMERA_EXTENDEDPROP_VALUE),
            &controlDefaults));
    }

    void DefaultControllerExtendedControl::SaveDefault(const wil::com_ptr<IMFCameraControlDefaults>& controlDefaults, const uint32_t value)
    {
        KSPROPERTY*                     ksProp = nullptr;
        ULONG                           ksPropSize = 0;
        KSCAMERA_EXTENDEDPROP_HEADER*   extPropHdr = nullptr;
        ULONG                           dataSize = 0;

        THROW_IF_FAILED(controlDefaults->LockControlData((void**)&ksProp, &ksPropSize, (void**)&extPropHdr, &dataSize));

        extPropHdr->Flags = value;

        THROW_IF_FAILED(controlDefaults->UnlockControlData());
    }

    void DefaultControllerEVCompExtendedControl::Initialize(const winrt::com_ptr<DefaultControlManager>& manager, wil::com_ptr<IMFCameraControlDefaults>& controlDefaults, const uint32_t id)
    {
        auto collection = manager->GetCollection();

        THROW_IF_FAILED(collection->GetOrAddExtendedControl(
            MF_CAMERA_CONTROL_CONFIGURATION_TYPE_POSTSTART, //
            id,
            KSCAMERA_EXTENDEDPROP_FILTERSCOPE,
            sizeof(KSCAMERA_EXTENDEDPROP_HEADER) + sizeof(KSCAMERA_EXTENDEDPROP_EVCOMPENSATION),
            &controlDefaults));
    }

    void DefaultControllerEVCompExtendedControl::SaveDefault(const wil::com_ptr<IMFCameraControlDefaults>& controlDefaults, const uint32_t value)
    {
        KSPROPERTY* ksProp = nullptr;
        ULONG                           ksPropSize = 0;
        KSCAMERA_EXTENDEDPROP_HEADER* extPropHdr = nullptr;
        KSCAMERA_EXTENDEDPROP_EVCOMPENSATION* evCompExtPropPayload = nullptr;
        ULONG                           dataSize = 0;

        THROW_IF_FAILED(controlDefaults->LockControlData((void**)&ksProp, &ksPropSize, (void**)&extPropHdr, &dataSize));
        evCompExtPropPayload = (KSCAMERA_EXTENDEDPROP_EVCOMPENSATION*)((PBYTE)extPropHdr + sizeof(KSCAMERA_EXTENDEDPROP_HEADER));
        evCompExtPropPayload->Value = value;

        THROW_IF_FAILED(controlDefaults->UnlockControlData());
    }

}
