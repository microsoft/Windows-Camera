// Copyright (c) Microsoft. All rights reserved.

#pragma once
#include "BackgroundSegmentationPropertyPayload.g.h"
#include "BackgroundSegmentationConfigCaps.h"


namespace winrt::CameraKsPropertyHelper::implementation
{
    struct BackgroundSegmentationPropertyPayload : 
        BackgroundSegmentationPropertyPayloadT<BackgroundSegmentationPropertyPayload>,
        PropertyValuePayloadHolder<KsBackgroundSegmentationPropPayload>
    {
        BackgroundSegmentationPropertyPayload(Windows::Foundation::IPropertyValue property)
            : PropertyValuePayloadHolder(property)
        {
            int configCapsCount = (m_propContainer.size() - sizeof(KSCAMERA_EXTENDEDPROP_HEADER)) / sizeof(KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_CONFIGCAPS);

            for (int i = 0; i < configCapsCount; i++)
            {
                auto nativeConfigCaps = reinterpret_cast<KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_CONFIGCAPS*>(&m_propContainer[0] + sizeof(KSCAMERA_EXTENDEDPROP_HEADER) + i * sizeof(KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_CONFIGCAPS));
                auto runtimeConfigCaps = make<BackgroundSegmentationConfigCaps>(*nativeConfigCaps);
                m_configCaps.Append(runtimeConfigCaps);
            }
        }

        Windows::Foundation::Collections::IVectorView<CameraKsPropertyHelper::BackgroundSegmentationConfigCaps> ConfigCaps() { return m_configCaps.GetView(); }

        CameraKsPropertyHelper::ExtendedControlKind ExtendedControlKind() { return ExtendedControlKind::KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION; }

    private:
        Windows::Foundation::Collections::IVector<CameraKsPropertyHelper::BackgroundSegmentationConfigCaps> m_configCaps = single_threaded_vector<CameraKsPropertyHelper::BackgroundSegmentationConfigCaps>();
        CameraKsPropertyHelper::ExtendedControlKind m_extendedControlKind;
    };
}
