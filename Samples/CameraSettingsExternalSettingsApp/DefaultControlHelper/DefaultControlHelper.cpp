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
        return winrt::make<DefaultController>(type, id, get_strong());
    }

    HRESULT DefaultControlManager::Save()
    {
        return m_configManager->SaveDefaults(m_controlDefaultsCollection.get());
    }

#pragma endregion

#pragma region DefaultController

    DefaultController::DefaultController(winrt::DefaultControlHelper::DefaultControllerType type, const uint32_t id, winrt::com_ptr<DefaultControlManager> manager)
    {
        m_id = id;
        m_controlManager = manager;

        // Creating an underlying helper controller based on the asked control type
        switch (type)
        {
        case winrt::DefaultControlHelper::DefaultControllerType::VideoProcAmp:
            m_internalController = std::make_unique< DefaultControllerVidCapVideoProcAmp>();
            m_setGuid = PROPSETID_VIDCAP_VIDEOPROCAMP;
            break;
        
        case winrt::DefaultControlHelper::DefaultControllerType::CameraControl:
            m_internalController = std::make_unique<DefaultControllerVidCapVideoProcAmp>();
            m_setGuid = PROPSETID_VIDCAP_CAMERACONTROL;
            break;
        
        case winrt::DefaultControlHelper::DefaultControllerType::ExtendedCameraControl:
            m_setGuid = KSPROPERTYSETID_ExtendedCameraControl;
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

        // check if there is an existing entry for this control to store a default value
        auto collection = m_controlManager.get()->GetCollection();
        auto count = collection->GetControlCount();
        m_hasDefaultValueStored = false;
        if (count > 0)
        {
            wil::com_ptr<IMFCameraControlDefaults> existingControlDefaults;
            for (ULONG i = 0; i < count && !m_hasDefaultValueStored; i++)
            {
                byte* pProperty = nullptr;
                ULONG cbProperty = 0;
                byte* pControlHeader = nullptr;
                ULONG cbData = 0;

                collection->GetControl(i, &existingControlDefaults);
                THROW_IF_FAILED(existingControlDefaults->LockControlData((void**)&pProperty, &cbProperty, (void**)&pControlHeader, &cbData));

                // we assume we get a KSProperty at the very least
                if (cbProperty < sizeof(KSPROPERTY))
                {
                    continue;
                }
                THROW_IF_NULL_ALLOC(pProperty);
                KSPROPERTY* pKsProperty = (KSPROPERTY*)pProperty;

                if (pKsProperty->Id == id && IsEqualGUID(pKsProperty->Set, m_setGuid))
                {
                    m_hasDefaultValueStored = true;
                }
                THROW_IF_FAILED(existingControlDefaults->UnlockControlData());
            }
        }
    };

    int32_t DefaultController::DefaultValue()
    {
        int32_t resultValue = INT32_MAX;
        if (m_hasDefaultValueStored)
        {
            m_internalController->Initialize(m_controlManager, m_controlDefaults, m_setGuid, m_id);
            try
            {
                void* pControl = nullptr;
                ULONG cbControl = 0;
                void* pData = nullptr;
                ULONG cbData = 0;

                THROW_IF_FAILED(m_controlDefaults->LockControlData(&pControl, &cbControl, &pData, &cbData));
                {
                    THROW_IF_NULL_ALLOC(pControl);
                    THROW_IF_NULL_ALLOC(pData);
                    auto unlock = wil::scope_exit([&]()
                        {
                            // The Unlock call can fail if there's no existing configuration or the required flags/capabilities
                            // are otherwise wrong or unset.
                            THROW_IF_FAILED(m_controlDefaults->UnlockControlData());
                        });

                    resultValue = m_internalController->GetStoredDefaultValue((void*)pControl, cbControl, (void*)pData, cbData);

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
                }
            }
            catch (...)
            {
                if (m_rangeInfo.has_value())
                {
                    resultValue = m_rangeInfo->defaultValue;
                }
            }
        }
        
        return resultValue;
    }

    void DefaultController::DefaultValue(int32_t const& value)
    {
        void* pControl = nullptr;
        ULONG cbControl = 0;
        void* pData = nullptr;
        ULONG cbData = 0;

        auto collection = m_controlManager->GetCollection();

        m_internalController->Initialize(m_controlManager, m_controlDefaults, m_setGuid, m_id);

        THROW_IF_FAILED(m_controlDefaults->LockControlData(&pControl, &cbControl, &pData, &cbData));
        {
            THROW_IF_NULL_ALLOC(pControl);
            THROW_IF_NULL_ALLOC(pData);
            auto unlock = wil::scope_exit([&]()
                {
                    // The Unlock call can fail if there's no existing configuration or the required flags/capabilities
                    // are otherwise wrong or unset.
                    THROW_IF_FAILED(m_controlDefaults->UnlockControlData());
                });
            m_internalController->SaveDefault((void*)pControl, cbControl, (void*)pData, cbData, value);
        }
        THROW_IF_FAILED(m_controlManager->Save());
        m_hasDefaultValueStored = true;
    }

#pragma endregion

    void DefaultControllerVidCapVideoProcAmp::Initialize(winrt::com_ptr<DefaultControlManager> manager, wil::com_ptr<IMFCameraControlDefaults>& defaults, winrt::guid setGuid, uint32_t id)
    {
        auto collection = manager->GetCollection();

        THROW_IF_FAILED(collection->GetOrAddControl(
            MF_CAMERA_CONTROL_CONFIGURATION_TYPE_POSTSTART,
            setGuid,
            id,
            sizeof(KSPROPERTY_VIDEOPROCAMP_S),
            sizeof(KSPROPERTY_VIDEOPROCAMP_S),
            &defaults));
    }

    void DefaultControllerVidCapVideoProcAmp::SaveDefault(void* pControl, ULONG controlSize, void* pData, ULONG dataSize, int32_t const& value)
    {
        THROW_HR_IF(E_UNEXPECTED, controlSize < sizeof(KSPROPERTY_VIDEOPROCAMP_S));
        THROW_HR_IF(E_UNEXPECTED, dataSize < sizeof(KSPROPERTY_VIDEOPROCAMP_S));
        KSPROPERTY_VIDEOPROCAMP_S* pVidCapControl = (KSPROPERTY_VIDEOPROCAMP_S*)pControl;
        KSPROPERTY_VIDEOPROCAMP_S* pVidCapData = (KSPROPERTY_VIDEOPROCAMP_S*)pData;

        pVidCapControl->Flags = KSPROPERTY_VIDEOPROCAMP_FLAGS_MANUAL;
        pVidCapControl->Property.Flags = KSPROPERTY_TYPE_SET;

        pVidCapControl->Value = value;

        pVidCapData->Property.Set = pVidCapControl->Property.Set;
        pVidCapData->Property.Id = pVidCapControl->Property.Id;
        pVidCapData->Property.Flags = pVidCapControl->Property.Flags;
        pVidCapData->Value = pVidCapControl->Value;
        pVidCapData->Flags = pVidCapControl->Flags;
    }

    int32_t DefaultControllerVidCapVideoProcAmp::GetStoredDefaultValue(void* pControl, ULONG controlSize, void*, ULONG)
    {
        THROW_HR_IF(E_UNEXPECTED, controlSize < sizeof(KSPROPERTY_VIDEOPROCAMP_S));
        KSPROPERTY_VIDEOPROCAMP_S* pVidCapControl = (KSPROPERTY_VIDEOPROCAMP_S*)pControl;

        return pVidCapControl->Value;
    }

    void DefaultControllerExtendedControl::Initialize(winrt::com_ptr<DefaultControlManager> manager, wil::com_ptr<IMFCameraControlDefaults>& defaults, winrt::guid, uint32_t id)
    {
        auto collection = manager->GetCollection();

        THROW_IF_FAILED(collection->GetOrAddExtendedControl(
            MF_CAMERA_CONTROL_CONFIGURATION_TYPE_POSTSTART,
            id,
            KSCAMERA_EXTENDEDPROP_FILTERSCOPE,
            sizeof(KSCAMERA_EXTENDEDPROP_HEADER) + sizeof(KSCAMERA_EXTENDEDPROP_VALUE),
            &defaults));
    }

    void DefaultControllerExtendedControl::SaveDefault(void* pControl, ULONG controlSize, void* pData, ULONG dataSize, int32_t const& value)
    {
        THROW_HR_IF(E_UNEXPECTED, controlSize < sizeof(KSPROPERTY));
        THROW_HR_IF(E_UNEXPECTED, dataSize < sizeof(KSCAMERA_EXTENDEDPROP_HEADER));
        KSPROPERTY* pProperty = (KSPROPERTY*)pControl;
        KSCAMERA_EXTENDEDPROP_HEADER* pExtendedHeader = (KSCAMERA_EXTENDEDPROP_HEADER*)pData;

        pProperty->Flags = KSPROPERTY_TYPE_SET;
        pExtendedHeader->Flags = value;
    }

    int32_t DefaultControllerExtendedControl::GetStoredDefaultValue(void*, ULONG, void* pData, ULONG dataSize)
    {
        THROW_HR_IF(E_UNEXPECTED, dataSize < sizeof(KSCAMERA_EXTENDEDPROP_HEADER));
        KSCAMERA_EXTENDEDPROP_HEADER* pExtendedHeader = (KSCAMERA_EXTENDEDPROP_HEADER*)pData;

        return (int32_t)pExtendedHeader->Flags;
    }

    void DefaultControllerEVCompExtendedControl::Initialize(winrt::com_ptr<DefaultControlManager> manager, wil::com_ptr<IMFCameraControlDefaults>& defaults, winrt::guid, uint32_t id)
    {
        auto collection = manager->GetCollection();

        THROW_IF_FAILED(collection->GetOrAddExtendedControl(
            MF_CAMERA_CONTROL_CONFIGURATION_TYPE_POSTSTART,
            id,
            KSCAMERA_EXTENDEDPROP_FILTERSCOPE,
            sizeof(KSCAMERA_EXTENDEDPROP_HEADER) + sizeof(KSCAMERA_EXTENDEDPROP_EVCOMPENSATION),
            &defaults));
    }

    void DefaultControllerEVCompExtendedControl::SaveDefault(void* pControl, ULONG controlSize, void* pData, ULONG dataSize, int32_t const& value)
    {
        THROW_HR_IF(E_UNEXPECTED, controlSize < sizeof(KSPROPERTY));
        THROW_HR_IF(E_UNEXPECTED, dataSize < (sizeof(KSCAMERA_EXTENDEDPROP_HEADER) + sizeof(KSCAMERA_EXTENDEDPROP_EVCOMPENSATION)));
        KSPROPERTY* pProperty = (KSPROPERTY*)pControl;
        KSCAMERA_EXTENDEDPROP_HEADER* pExtendedHeader = (KSCAMERA_EXTENDEDPROP_HEADER*)pData;
        KSCAMERA_EXTENDEDPROP_EVCOMPENSATION* evCompExtPropPayload = (KSCAMERA_EXTENDEDPROP_EVCOMPENSATION*)((PBYTE)pData + sizeof(KSCAMERA_EXTENDEDPROP_HEADER));;

        pProperty->Flags = KSPROPERTY_TYPE_SET;
        pExtendedHeader->Flags = 0;
        pExtendedHeader->PinId = KSCAMERA_EXTENDEDPROP_FILTERSCOPE;
        pExtendedHeader->Size = (sizeof(KSCAMERA_EXTENDEDPROP_HEADER) + sizeof(KSCAMERA_EXTENDEDPROP_EVCOMPENSATION));
        
        evCompExtPropPayload = (KSCAMERA_EXTENDEDPROP_EVCOMPENSATION*)((PBYTE)pExtendedHeader + sizeof(KSCAMERA_EXTENDEDPROP_HEADER));
        evCompExtPropPayload->Value = value;
        evCompExtPropPayload->Mode = 0;
    }

    int32_t DefaultControllerEVCompExtendedControl::GetStoredDefaultValue(void*, ULONG, void* pData, ULONG dataSize)
    {
        THROW_HR_IF(E_UNEXPECTED, dataSize < (sizeof(KSCAMERA_EXTENDEDPROP_HEADER) + sizeof(KSCAMERA_EXTENDEDPROP_EVCOMPENSATION)));
        KSCAMERA_EXTENDEDPROP_EVCOMPENSATION* evCompExtPropPayload = (KSCAMERA_EXTENDEDPROP_EVCOMPENSATION*)((PBYTE)pData + sizeof(KSCAMERA_EXTENDEDPROP_HEADER));;

        return evCompExtPropPayload->Value;
    }
}
