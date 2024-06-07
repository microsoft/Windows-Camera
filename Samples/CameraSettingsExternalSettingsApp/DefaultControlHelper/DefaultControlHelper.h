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
        ~DefaultControlManager();

        winrt::DefaultControlHelper::DefaultController CreateController(DefaultControllerType type, const uint32_t id);

        wil::com_ptr<IMFCameraControlDefaultsCollection> GetDefaultCollection();

        HRESULT SaveDefaultCollection(IMFCameraControlDefaultsCollection* collection);

    private:

        wil::com_ptr<IMFCameraConfigurationManager> m_spConfigManager;
        wil::com_ptr<IMFAttributes> m_spAttributes;
    };

    struct DefaultController : DefaultControllerT<DefaultController>
    {
        DefaultController(winrt::DefaultControlHelper::DefaultControllerType type, uint32_t id, winrt::com_ptr<DefaultControlManager> manager);
        winrt::Windows::Foundation::IReference<int32_t> TryGetStoredDefaultValue();
        void SetDefaultValue(int32_t const& value);

    private:
        uint32_t m_id = 0;
        winrt::guid m_setGuid;
        winrt::com_ptr<DefaultControlManager> m_controlManager;

        std::unique_ptr<IDefaultControllerInternal> m_internalController;
    };

    // This is a helper interface and classes to handle
    // different kind of camera control structures.
    struct IDefaultControllerInternal
    {
        virtual void FormatPayloadWithValue(void* pControl, ULONG controlSize, void* pPayload, ULONG payloadSize, int32_t const& value) = 0;
        virtual int32_t GetValueFromPayload(void* control, ULONG controlSize, void* pPayload, ULONG payloadSize) = 0;
        virtual wil::com_ptr<IMFCameraControlDefaults> GetOrAddDefaultToCollection(IMFCameraControlDefaultsCollection* defaultCollection, winrt::guid setGuid, uint32_t id) = 0;
        virtual ~IDefaultControllerInternal() = default;
    };

    class DefaultControllerVidCapVideoProcAmp : public IDefaultControllerInternal
    {
    public:
        virtual void FormatPayloadWithValue(void* pControl, ULONG controlSize, void* pPayload, ULONG payloadSize, int32_t const& value) override;
        virtual int32_t GetValueFromPayload(void* control, ULONG controlSize, void* pPayload, ULONG payloadSize) override;

        virtual wil::com_ptr<IMFCameraControlDefaults> GetOrAddDefaultToCollection(IMFCameraControlDefaultsCollection* defaultCollection, winrt::guid setGuid, uint32_t id);
        
        virtual ~DefaultControllerVidCapVideoProcAmp() = default;
    };

    class DefaultControllerExtendedControl : public IDefaultControllerInternal
    {
    public:
        virtual void FormatPayloadWithValue(void* pControl, ULONG controlSize, void* pPayload, ULONG payloadSize, int32_t const& value) override;
        virtual int32_t GetValueFromPayload(void* control, ULONG controlSize, void* pPayload, ULONG payloadSize) override;

        virtual wil::com_ptr<IMFCameraControlDefaults> GetOrAddDefaultToCollection(IMFCameraControlDefaultsCollection* defaultCollection, winrt::guid setGuid, uint32_t id);

        virtual ~DefaultControllerExtendedControl() = default;        
    };

    class DefaultControllerEVCompExtendedControl : public IDefaultControllerInternal
    {
    public:
        virtual void FormatPayloadWithValue(void* pControl, ULONG controlSize, void* pPayload, ULONG payloadSize, int32_t const& value) override;
        virtual int32_t GetValueFromPayload(void* control, ULONG controlSize, void* pPayload, ULONG payloadSize) override;

        virtual wil::com_ptr<IMFCameraControlDefaults> GetOrAddDefaultToCollection(IMFCameraControlDefaultsCollection* defaultCollection, winrt::guid setGuid, uint32_t id);

        virtual ~DefaultControllerEVCompExtendedControl() = default;
    };
}

namespace winrt::DefaultControlHelper::factory_implementation
{
    struct DefaultControlManager : DefaultControlManagerT<DefaultControlManager, implementation::DefaultControlManager>
    {
    };
}
