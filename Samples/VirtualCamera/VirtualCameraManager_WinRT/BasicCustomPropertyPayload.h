// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once
#include "Common.h"
#include "BasicCustomPropertyPayload.g.h"

namespace winrt::VirtualCameraManager_WinRT::implementation
{
    struct BasicCustomPropertyPayload 
        : BasicCustomPropertyPayloadT<BasicCustomPropertyPayload>,
          PropertyValuePayloadHolder<KsBasicCameraExtendedPropPayload>
    {
        BasicCustomPropertyPayload(Windows::Foundation::IPropertyValue property,
            winrt::VirtualCameraManager_WinRT::CustomControlKind customControlKind)
            : PropertyValuePayloadHolder(property),
              m_customControlKind(customControlKind) 
        {}

        winrt::VirtualCameraManager_WinRT::CustomControlKind CustomControlKind() { return m_customControlKind; }

    private:
        winrt::VirtualCameraManager_WinRT::CustomControlKind m_customControlKind;
    };
}
