// Copyright (c) Microsoft. All rights reserved.

#pragma once
#include "FieldOfView2ConfigCapsPropertyPayload.g.h"
#include "KSHelper.h"

namespace winrt::CameraKsPropertyHelper::implementation
{
    struct FieldOfView2ConfigCapsPropertyPayload :
        FieldOfView2ConfigCapsPropertyPayloadT<FieldOfView2ConfigCapsPropertyPayload>,
        PropertyValuePayloadHolder<KsFieldOfView2ConfigCapsPropPayload>
    {
        FieldOfView2ConfigCapsPropertyPayload(Windows::Foundation::IPropertyValue property) :
            PropertyValuePayloadHolder(property)
        {
            for (int i = 0; i < m_payload->configCaps.DiscreteFoVStopsCount; i++)
            {
                m_discreteFoVStops.Append(m_payload->configCaps.DiscreteFoVStops[i]);
            }
        }

        int32_t DefaultDiagonalFieldOfViewInDegrees()
        {
            return m_payload->configCaps.DefaultDiagonalFieldOfViewInDegrees;
        }
        int32_t DiscreteFoVStopsCount()
        {
            return m_payload->configCaps.DiscreteFoVStopsCount;
        }
        winrt::Windows::Foundation::Collections::IVectorView<int32_t> DiscreteFoVStops()
        {
            return m_discreteFoVStops.GetView();
        }

        winrt::CameraKsPropertyHelper::ExtendedControlKind ExtendedControlKind() { return ExtendedControlKind::KSPROPERTY_CAMERACONTROL_EXTENDED_FIELDOFVIEW2_CONFIGCAPS; }

    private:

        Windows::Foundation::Collections::IVector<int32_t> m_discreteFoVStops = single_threaded_vector<int32_t>();
    };
}
