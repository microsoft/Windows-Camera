// Copyright (c) Microsoft. All rights reserved.

#pragma once
#include "WindowsStudioSupportedPropertyPayload.g.h"
#include "KSHelper.h"

namespace winrt::CameraKsPropertyHelper::implementation
{
    struct WindowsStudioSupportedPropertyPayload : 
        WindowsStudioSupportedPropertyPayloadT<WindowsStudioSupportedPropertyPayload>,
        PropertyValuePayloadHolder<KsWindowsStudioSupportedPropPayload>
    {
        WindowsStudioSupportedPropertyPayload(Windows::Foundation::IPropertyValue property, CameraKsPropertyHelper::WindowsStudioControlKind windowsStudioControlKind)
            : PropertyValuePayloadHolder(property),
            m_windowsStudioControlKind(windowsStudioControlKind)
        {
            int controlCount = (m_payload->header.Size - sizeof(KSCAMERA_EXTENDEDPROP_HEADER)) / sizeof(KSPROPERTY);

            for (int i = 0; i < controlCount; i++)
            {
                auto pKsProperty = reinterpret_cast<KSPROPERTY*>(&m_propContainer[0] + sizeof(KSCAMERA_EXTENDEDPROP_HEADER) + i * sizeof(KSPROPERTY));
                //auto ksProperty = make<KsProperty>(*pKsProperty);
                m_supportedControls.Append({ pKsProperty->Set, pKsProperty->Id, pKsProperty->Flags });
            }
        }

        Windows::Foundation::Collections::IVectorView<CameraKsPropertyHelper::KsProperty> SupportedControls() { return m_supportedControls.GetView(); }
        winrt::CameraKsPropertyHelper::WindowsStudioControlKind WindowsStudioControlKind() { return m_windowsStudioControlKind; }

    private:
        Windows::Foundation::Collections::IVector<CameraKsPropertyHelper::KsProperty> m_supportedControls = single_threaded_vector<CameraKsPropertyHelper::KsProperty>();
        CameraKsPropertyHelper::WindowsStudioControlKind m_windowsStudioControlKind;
    };
}
