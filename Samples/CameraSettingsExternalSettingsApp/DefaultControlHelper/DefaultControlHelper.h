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

        wil::com_ptr_t<IMFCameraConfigurationManager>           m_configManager;
        wil::com_ptr_t<IMFCameraControlDefaultsCollection>      m_controlDefaultsCollection;
    };

    struct DefaultController : DefaultControllerT<DefaultController>
    {
        DefaultController(winrt::DefaultControlHelper::DefaultControllerType type, uint32_t id, winrt::weak_ref<DefaultControlManager> manager);

        uint32_t DefaultValue();
        void DefaultValue(uint32_t const& value);

    private:

        uint32_t                                        m_defaultValue = 0;
        std::optional<MF_CAMERA_CONTROL_RANGE_INFO>     m_rangeInfo;
        wil::com_ptr<IMFCameraControlDefaults>          m_controlDefaults;

        
        uint32_t                                        m_id = 0;
        winrt::weak_ref<DefaultControlManager>          m_controlManager;

        std::unique_ptr<IDefaultControllerInternal>     m_defaultController;
    };

    // This is a helper interface and classes to handle
    // different kind of camera control structures.
    struct IDefaultControllerInternal
    {
        virtual void SaveDefault(const wil::com_ptr<IMFCameraControlDefaults>&, const uint32_t) = 0;
        virtual void LoadDefault() = 0;
        virtual void Initialize(const winrt::com_ptr<DefaultControlManager>&, wil::com_ptr<IMFCameraControlDefaults>&, const uint32_t) = 0;

        virtual ~IDefaultControllerInternal() = default;
    };

    class DefaultControllerVidCapVideoProcAmp : public IDefaultControllerInternal
    {
    public:
        virtual void SaveDefault(const wil::com_ptr<IMFCameraControlDefaults>&, const uint32_t value) override;
        virtual void LoadDefault() override;

        virtual void Initialize(winrt::com_ptr<DefaultControlManager> const& manager, wil::com_ptr<IMFCameraControlDefaults>&, const uint32_t id) override;
        
        virtual ~DefaultControllerVidCapVideoProcAmp() = default;
    private:
        const winrt::guid                                     m_typeGuid{ PROPSETID_VIDCAP_VIDEOPROCAMP };

    };

    class DefaultControllerExtendedControl : public IDefaultControllerInternal
    {
    public:
        virtual void SaveDefault(const wil::com_ptr<IMFCameraControlDefaults>&, const uint32_t value) override;
        virtual void LoadDefault() override;

        virtual void Initialize (const winrt::com_ptr<DefaultControlManager>& manager, wil::com_ptr<IMFCameraControlDefaults>&, const uint32_t id) override;

        virtual ~DefaultControllerExtendedControl() = default;

    private:
        winrt::guid                                     m_typeGuid{ KSPROPERTYSETID_ExtendedCameraControl };
    };
}

namespace winrt::DefaultControlHelper::factory_implementation
{
    struct DefaultControlManager : DefaultControlManagerT<DefaultControlManager, implementation::DefaultControlManager>
    {
    };
}
