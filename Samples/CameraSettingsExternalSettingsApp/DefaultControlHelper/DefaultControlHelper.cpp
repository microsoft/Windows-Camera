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
        m_spConfigManager = wil::CoCreateInstance<IMFCameraConfigurationManager>(CLSID_CameraConfigurationManager);
        THROW_IF_FAILED(MFCreateAttributes(&m_spAttributes, 1));
        THROW_IF_FAILED(m_spAttributes->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, cameraSymbolicLink.c_str()));
    }

    DefaultControlManager::~DefaultControlManager()
    {
        if (m_spConfigManager != nullptr)
        {
            m_spConfigManager->Shutdown();
            m_spConfigManager = nullptr;
        }
    }

    wil::com_ptr<IMFCameraControlDefaultsCollection> DefaultControlManager::GetDefaultCollection()
    {
        wil::com_ptr<IMFCameraControlDefaultsCollection> spControlDefaultsCollection = nullptr;
        THROW_IF_FAILED(m_spConfigManager->LoadDefaults(m_spAttributes.get(), &spControlDefaultsCollection));
        return spControlDefaultsCollection;
    }

    winrt::DefaultControlHelper::DefaultController DefaultControlManager::CreateController(DefaultControllerType type, uint32_t id)
    {
        return winrt::make<DefaultController>(type, id, get_strong());
    }

    HRESULT DefaultControlManager::SaveDefaultCollection(IMFCameraControlDefaultsCollection* defaultCollection)
    {
        THROW_HR_IF_NULL(E_INVALIDARG, defaultCollection);
        return m_spConfigManager->SaveDefaults(defaultCollection);
    }

#pragma endregion

#pragma region DefaultController

    DefaultController::DefaultController(
        winrt::DefaultControlHelper::DefaultControllerType type, 
        const uint32_t id, 
        winrt::com_ptr<DefaultControlManager> manager)
    {
        m_id = id;
        m_controlManager = manager;

        // Creating an underlying helper controller based on the asked control type
        switch (type)
        {
            case winrt::DefaultControlHelper::DefaultControllerType::VideoProcAmp:
                m_internalController = std::make_unique<DefaultControllerVidCapVideoProcAmp>();
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
    };

    winrt::Windows::Foundation::IReference<int32_t> DefaultController::TryGetStoredDefaultValue()
    {
        winrt::Windows::Foundation::IReference<int32_t> resultValue = nullptr;
        bool hasDefaultValueStored = false;

        // check if there is an existing entry for this control to store a default value
        auto collection = m_controlManager->GetDefaultCollection();
        auto count = collection->GetControlCount();

        if (count > 0)
        {
            wil::com_ptr<IMFCameraControlDefaults> existingControlDefaults;
            for (ULONG i = 0; i < count && !hasDefaultValueStored; i++)
            {
                byte* pProperty = nullptr;
                ULONG cbProperty = 0;
                byte* pControlHeader = nullptr;
                ULONG cbData = 0;

                collection->GetControl(i, &existingControlDefaults);
                THROW_IF_FAILED(existingControlDefaults->LockControlData(
                    (void**)&pProperty,
                    &cbProperty,
                    (void**)&pControlHeader,
                    &cbData));

                // we assume we get a KSProperty at the very least
                if (cbProperty < sizeof(KSPROPERTY))
                {
                    continue;
                }
                THROW_IF_NULL_ALLOC(pProperty);
                KSPROPERTY* pKsProperty = (KSPROPERTY*)pProperty;

                // if we find a KSProperty payload matching ID and PropertySet storing a default value..
                if (pKsProperty->Id == m_id && IsEqualGUID(pKsProperty->Set, m_setGuid))
                {
                    resultValue = winrt::Windows::Foundation::IReference<int>(
                        m_internalController->GetValueFromPayload(
                            (void*)pProperty,
                            cbProperty,
                            (void*)pControlHeader,
                            cbData));

                    hasDefaultValueStored = true;
                }
                HRESULT hr = (existingControlDefaults->UnlockControlData());
                if (FAILED(hr))
                {
                    assert(hr != S_OK);
                }
            }
        }
        
        return resultValue;
    }

    void DefaultController::SetDefaultValue(int32_t const& value)
    {
        void* pControl = nullptr;
        ULONG cbControl = 0;
        void* pData = nullptr;
        ULONG cbData = 0;

        auto collection = m_controlManager->GetDefaultCollection();

        auto defaults = m_internalController->GetOrAddDefaultToCollection(collection.get(), m_setGuid, m_id);

        THROW_IF_FAILED(defaults->LockControlData(&pControl, &cbControl, &pData, &cbData));
        {
            THROW_IF_NULL_ALLOC(pControl);
            THROW_IF_NULL_ALLOC(pData);
            auto unlock = wil::scope_exit([&]()
                {
                    // The Unlock call can fail if there's no existing configuration or the required flags/capabilities
                    // are otherwise wrong or unset.
                    THROW_IF_FAILED(defaults->UnlockControlData());
                });
            m_internalController->FormatPayloadWithValue((void*)pControl, cbControl, (void*)pData, cbData, value);
        }

        THROW_IF_FAILED(m_controlManager->SaveDefaultCollection(collection.get()));
    }

#pragma endregion
    
    wil::com_ptr<IMFCameraControlDefaults> DefaultControllerVidCapVideoProcAmp::GetOrAddDefaultToCollection(
        IMFCameraControlDefaultsCollection* defaultCollection, 
        winrt::guid setGuid, 
        uint32_t id)
    {
        wil::com_ptr<IMFCameraControlDefaults> spDefaults = nullptr;
        
        THROW_HR_IF_NULL(E_INVALIDARG, defaultCollection);

        THROW_IF_FAILED(defaultCollection->GetOrAddControl(
            MF_CAMERA_CONTROL_CONFIGURATION_TYPE_POSTSTART,
            setGuid,
            id,
            sizeof(KSPROPERTY_VIDEOPROCAMP_S),
            sizeof(KSPROPERTY_VIDEOPROCAMP_S),
            &spDefaults));

        return spDefaults;
    }

    void DefaultControllerVidCapVideoProcAmp::FormatPayloadWithValue(void* pControl, ULONG controlSize, void* pPayload, ULONG payloadSize, int32_t const& value)
    {
        THROW_HR_IF(E_UNEXPECTED, controlSize < sizeof(KSPROPERTY_VIDEOPROCAMP_S));
        THROW_HR_IF(E_UNEXPECTED, payloadSize < sizeof(KSPROPERTY_VIDEOPROCAMP_S));
        KSPROPERTY_VIDEOPROCAMP_S* pVidCapControl = (KSPROPERTY_VIDEOPROCAMP_S*)pControl;
        KSPROPERTY_VIDEOPROCAMP_S* pVidCapData = (KSPROPERTY_VIDEOPROCAMP_S*)pPayload;

        pVidCapControl->Flags = KSPROPERTY_VIDEOPROCAMP_FLAGS_MANUAL;
        pVidCapControl->Property.Flags = KSPROPERTY_TYPE_SET;

        pVidCapControl->Value = value;

        pVidCapData->Property.Set = pVidCapControl->Property.Set;
        pVidCapData->Property.Id = pVidCapControl->Property.Id;
        pVidCapData->Property.Flags = pVidCapControl->Property.Flags;
        pVidCapData->Value = pVidCapControl->Value;
        pVidCapData->Flags = pVidCapControl->Flags;
    }

    int32_t DefaultControllerVidCapVideoProcAmp::GetValueFromPayload(void* pControl, ULONG controlSize, void*, ULONG)
    {
        THROW_HR_IF(E_UNEXPECTED, controlSize < sizeof(KSPROPERTY_VIDEOPROCAMP_S));
        KSPROPERTY_VIDEOPROCAMP_S* pVidCapControl = (KSPROPERTY_VIDEOPROCAMP_S*)pControl;

        return pVidCapControl->Value;
    }

    wil::com_ptr<IMFCameraControlDefaults> DefaultControllerExtendedControl::GetOrAddDefaultToCollection(
        IMFCameraControlDefaultsCollection* defaultCollection,
        winrt::guid setGuid,
        uint32_t id)
    {
        // we are not using this parameter since we exercise the API tailored for extended property GetOrAddExtendedControl()
        UNREFERENCED_PARAMETER(setGuid); 

        wil::com_ptr<IMFCameraControlDefaults> spDefaults = nullptr;

        THROW_HR_IF_NULL(E_INVALIDARG, defaultCollection);
        MF_CAMERA_CONTROL_CONFIGURATION_TYPE controlConfigType = MF_CAMERA_CONTROL_CONFIGURATION_TYPE_POSTSTART;
        if (id == KSPROPERTY_CAMERACONTROL_EXTENDED_VIDEOHDR)
        {
            controlConfigType = MF_CAMERA_CONTROL_CONFIGURATION_TYPE_PRESTART;
        }
        THROW_IF_FAILED(defaultCollection->GetOrAddExtendedControl(
            controlConfigType,
            id,
            KSCAMERA_EXTENDEDPROP_FILTERSCOPE,
            sizeof(KSCAMERA_EXTENDEDPROP_HEADER) + sizeof(KSCAMERA_EXTENDEDPROP_VALUE),
            &spDefaults));

        return spDefaults;
    }

    void DefaultControllerExtendedControl::FormatPayloadWithValue(void* pControl, ULONG controlSize, void* pPayload, ULONG payloadSize, int32_t const& value)
    {
        THROW_HR_IF(E_UNEXPECTED, controlSize < sizeof(KSPROPERTY));
        THROW_HR_IF(E_UNEXPECTED, payloadSize < sizeof(KSCAMERA_EXTENDEDPROP_HEADER));
        KSPROPERTY* pProperty = (KSPROPERTY*)pControl;
        KSCAMERA_EXTENDEDPROP_HEADER* pExtendedHeader = (KSCAMERA_EXTENDEDPROP_HEADER*)pPayload;
        KSCAMERA_EXTENDEDPROP_VALUE* pExtendedValue = (KSCAMERA_EXTENDEDPROP_VALUE*)(pExtendedHeader+1);
        *pExtendedValue = {};

        pProperty->Flags = KSPROPERTY_TYPE_SET;
        pExtendedHeader->Flags = value;
    }

    int32_t DefaultControllerExtendedControl::GetValueFromPayload(void*, ULONG, void* pPayload, ULONG payloadSize)
    {
        THROW_HR_IF(E_UNEXPECTED, payloadSize < sizeof(KSCAMERA_EXTENDEDPROP_HEADER));
        KSCAMERA_EXTENDEDPROP_HEADER* pExtendedHeader = (KSCAMERA_EXTENDEDPROP_HEADER*)pPayload;

        return (int32_t)pExtendedHeader->Flags;
    }

    wil::com_ptr<IMFCameraControlDefaults> DefaultControllerEVCompExtendedControl::GetOrAddDefaultToCollection(
        IMFCameraControlDefaultsCollection* defaultCollection,
        winrt::guid setGuid,
        uint32_t id)
    {
        // we are not using this parameter since we exercise the API tailored for extended property GetOrAddExtendedControl()
        UNREFERENCED_PARAMETER(setGuid);

        wil::com_ptr<IMFCameraControlDefaults> spDefaults = nullptr;

        THROW_HR_IF_NULL(E_INVALIDARG, defaultCollection);

        THROW_IF_FAILED(defaultCollection->GetOrAddExtendedControl(
            MF_CAMERA_CONTROL_CONFIGURATION_TYPE_POSTSTART,
            id,
            KSCAMERA_EXTENDEDPROP_FILTERSCOPE,
            sizeof(KSCAMERA_EXTENDEDPROP_HEADER) + sizeof(KSCAMERA_EXTENDEDPROP_EVCOMPENSATION),
            &spDefaults));

        return spDefaults;
    }

    void DefaultControllerEVCompExtendedControl::FormatPayloadWithValue(void* pControl, ULONG controlSize, void* pPayload, ULONG payloadSize, int32_t const& value)
    {
        THROW_HR_IF(E_UNEXPECTED, controlSize < sizeof(KSPROPERTY));
        THROW_HR_IF(E_UNEXPECTED, payloadSize < (sizeof(KSCAMERA_EXTENDEDPROP_HEADER) + sizeof(KSCAMERA_EXTENDEDPROP_EVCOMPENSATION)));
        KSPROPERTY* pProperty = (KSPROPERTY*)pControl;
        KSCAMERA_EXTENDEDPROP_HEADER* pExtendedHeader = (KSCAMERA_EXTENDEDPROP_HEADER*)pPayload;
        KSCAMERA_EXTENDEDPROP_EVCOMPENSATION* evCompExtPropPayload = (KSCAMERA_EXTENDEDPROP_EVCOMPENSATION*)(pExtendedHeader + 1);

        pProperty->Flags = KSPROPERTY_TYPE_SET;
        pExtendedHeader->Flags = 0;
        pExtendedHeader->PinId = KSCAMERA_EXTENDEDPROP_FILTERSCOPE;
        pExtendedHeader->Size = (sizeof(KSCAMERA_EXTENDEDPROP_HEADER) + sizeof(KSCAMERA_EXTENDEDPROP_EVCOMPENSATION));
        
        evCompExtPropPayload = (KSCAMERA_EXTENDEDPROP_EVCOMPENSATION*)((PBYTE)pExtendedHeader + sizeof(KSCAMERA_EXTENDEDPROP_HEADER));
        evCompExtPropPayload->Value = value;
        evCompExtPropPayload->Mode = 0;
    }

    int32_t DefaultControllerEVCompExtendedControl::GetValueFromPayload(void*, ULONG, void* pPayload, ULONG payloadSize)
    {
        THROW_HR_IF(E_UNEXPECTED, payloadSize < (sizeof(KSCAMERA_EXTENDEDPROP_HEADER) + sizeof(KSCAMERA_EXTENDEDPROP_EVCOMPENSATION)));
        KSCAMERA_EXTENDEDPROP_EVCOMPENSATION* evCompExtPropPayload = (KSCAMERA_EXTENDEDPROP_EVCOMPENSATION*)((PBYTE)pPayload + sizeof(KSCAMERA_EXTENDEDPROP_HEADER));;

        return evCompExtPropPayload->Value;
    }
}
