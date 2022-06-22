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

        winrt::CameraKsPropertyHelper::ExtendedControlKind ExtendedControlKind() { return m_extendedControlKind; }

    private:
        CameraKsPropertyHelper::ExtendedControlKind m_extendedControlKind;
    };
}
