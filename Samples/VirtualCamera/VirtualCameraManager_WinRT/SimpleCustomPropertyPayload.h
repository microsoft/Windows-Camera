// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once
#include "Common.h"
#include "SimpleCustomPropertyPayload.g.h"

namespace winrt::VirtualCameraManager_WinRT::implementation
{
    struct SimpleCustomPropertyPayload
        : SimpleCustomPropertyPayloadT < SimpleCustomPropertyPayload > ,
          PropertyValuePayloadHolder<KsSimpleCustomControlPropValue>
    {
        SimpleCustomPropertyPayload(Windows::Foundation::IPropertyValue property,
            winrt::VirtualCameraManager_WinRT::SimpleCustomControlKind controlKind)
            : PropertyValuePayloadHolder(property),
              m_customControlKind(controlKind)
        {}

        uint32_t ColorMode() { return m_payload->header.ColorMode; };
        winrt::VirtualCameraManager_WinRT::SimpleCustomControlKind ControlKind() { return m_customControlKind; }

    private:
        winrt::VirtualCameraManager_WinRT::SimpleCustomControlKind m_customControlKind;
    };
}
