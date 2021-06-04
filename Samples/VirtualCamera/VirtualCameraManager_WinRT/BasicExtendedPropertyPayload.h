// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once
#include "Common.h"
#include "BasicExtendedPropertyPayload.g.h"

namespace winrt::VirtualCameraManager_WinRT::implementation
{
    struct BasicExtendedPropertyPayload 
        : BasicExtendedPropertyPayloadT<BasicExtendedPropertyPayload>,
          PropertyValuePayloadHolder<KsBasicCameraExtendedPropPayload>
    {
        BasicExtendedPropertyPayload(Windows::Foundation::IPropertyValue property, winrt::VirtualCameraManager_WinRT::ExtendedControlKind extendedControlKind)
            : PropertyValuePayloadHolder(property),
              m_extendedControlKind(extendedControlKind)
        {}

        winrt::VirtualCameraManager_WinRT::ExtendedControlKind ExtendedControlKind() { return m_extendedControlKind; }

    private:
        winrt::VirtualCameraManager_WinRT::ExtendedControlKind m_extendedControlKind;
    };
}
