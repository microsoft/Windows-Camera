// Copyright (c) Microsoft. All rights reserved.

#pragma once
#include "BasicExtendedPropertyPayload.g.h"
#include "KSHelper.h"

namespace winrt::CameraKsPropertyHelper::implementation
{
    struct BasicExtendedPropertyPayload : 
        BasicExtendedPropertyPayloadT<BasicExtendedPropertyPayload>,
        PropertyValuePayloadHolder<KsBasicCameraExtendedPropPayload>
    {
        BasicExtendedPropertyPayload(Windows::Foundation::IPropertyValue property, CameraKsPropertyHelper::ExtendedControlKind extendedControlKind)
            : PropertyValuePayloadHolder(property),
            m_extendedControlKind(extendedControlKind)
        {}

        double dbl() { return m_payload->value.Value.dbl; }
        uint64_t ull() { return m_payload->value.Value.ull; }
        uint32_t ul() { return m_payload->value.Value.ul; }
        winrt::CameraKsPropertyHelper::Ratio ratio() { return { m_payload->value.Value.ratio.LowPart, m_payload->value.Value.ratio.HighPart }; }
        int32_t l() { return m_payload->value.Value.l; }
        int64_t ll() { return m_payload->value.Value.ll; }

        winrt::CameraKsPropertyHelper::ExtendedControlKind ExtendedControlKind() { return m_extendedControlKind; }

    private:
        CameraKsPropertyHelper::ExtendedControlKind m_extendedControlKind;
    };
}
