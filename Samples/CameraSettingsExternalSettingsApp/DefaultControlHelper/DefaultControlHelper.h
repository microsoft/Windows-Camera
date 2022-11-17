//
// Copyright (c) Microsoft Corporation.  All rights reserved.
//

#pragma once

#include "DefaultControlManager.g.h"
#include "DefaultController.g.h"

namespace winrt::DefaultControlHelper::implementation
{

    struct IDefaultControllerInternal;

    struct DefaultControlManager : DefaultControlManagerT<DefaultControlManager>
    {
        DefaultControlManager(winrt::hstring cameraSymbolicLink);

        winrt::DefaultControlHelper::DefaultController CreateController(DefaultControllerType type, const uint32_t id);

        wil::com_ptr_t<IMFCameraControlDefaultsCollection> GetCollection() { return m_controlDefaultsCollection; }

        HRESULT Save();

    private:

        wil::com_ptr_t<IMFCameraConfigurationManager> m_configManager;
        wil::com_ptr_t<IMFCameraControlDefaultsCollection> m_controlDefaultsCollection;
    };

    struct DefaultController : DefaultControllerT<DefaultController>
    {
        DefaultController(winrt::DefaultControlHelper::DefaultControllerType type, uint32_t id, winrt::com_ptr<DefaultControlManager> manager);
        bool HasDefaultValueStored() { return m_hasDefaultValueStored; }
        int32_t DefaultValue();
        void DefaultValue(int32_t const& value);

    private:

        bool m_hasDefaultValueStored = false;
        std::optional<MF_CAMERA_CONTROL_RANGE_INFO> m_rangeInfo;
        wil::com_ptr<IMFCameraControlDefaults> m_controlDefaults;

        uint32_t m_id = 0;
        winrt::guid m_setGuid;
        winrt::com_ptr<DefaultControlManager> m_controlManager;

        std::unique_ptr<IDefaultControllerInternal> m_internalController;
    };

    // This is a helper interface and classes to handle
    // different kind of camera control structures.
    struct IDefaultControllerInternal
    {
        virtual void SaveDefault(void* pControl, ULONG controlSize, void* pData, ULONG dataSize, int32_t const& value) = 0;
        virtual int32_t GetStoredDefaultValue(void* control, ULONG controlSize, void* data, ULONG dataSize) = 0;
        virtual void Initialize(winrt::com_ptr<DefaultControlManager> manager, wil::com_ptr<IMFCameraControlDefaults>& defaults, winrt::guid setGuid, uint32_t id) = 0;
        virtual ~IDefaultControllerInternal() = default;
    };

    class DefaultControllerVidCapVideoProcAmp : public IDefaultControllerInternal
    {
    public:
        virtual void SaveDefault(void* pControl, ULONG controlSize, void* pData, ULONG dataSize, int32_t const& value) override;
        virtual int32_t GetStoredDefaultValue(void* control, ULONG controlSize, void* data, ULONG dataSize) override;

        virtual void Initialize(winrt::com_ptr<DefaultControlManager> manager, wil::com_ptr<IMFCameraControlDefaults>& defaults, winrt::guid setGuid, uint32_t id) override;
        
        virtual ~DefaultControllerVidCapVideoProcAmp() = default;
    };

    class DefaultControllerExtendedControl : public IDefaultControllerInternal
    {
    public:
        virtual void SaveDefault(void* pControl, ULONG controlSize, void* pData, ULONG dataSize, int32_t const& value) override;
        virtual int32_t GetStoredDefaultValue(void* control, ULONG controlSize, void* data, ULONG dataSize) override;

        virtual void Initialize(winrt::com_ptr<DefaultControlManager> manager, wil::com_ptr<IMFCameraControlDefaults>& defaults, winrt::guid setGuid, uint32_t id) override;

        virtual ~DefaultControllerExtendedControl() = default;        
    };

    class DefaultControllerEVCompExtendedControl : public IDefaultControllerInternal
    {
    public:
        virtual void SaveDefault(void* pControl, ULONG controlSize, void* pData, ULONG dataSize, int32_t const& value) override;
        virtual int32_t GetStoredDefaultValue(void* control, ULONG controlSize, void* data, ULONG dataSize) override;

        virtual void Initialize(winrt::com_ptr<DefaultControlManager> manager, wil::com_ptr<IMFCameraControlDefaults>& defaults, winrt::guid setGuid, uint32_t id) override;

        virtual ~DefaultControllerEVCompExtendedControl() = default;
    };
}

namespace winrt::DefaultControlHelper::factory_implementation
{
    struct DefaultControlManager : DefaultControlManagerT<DefaultControlManager, implementation::DefaultControlManager>
    {
    };
}
