// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once
#include "Common.h"
#include "BasicAugmentedMediaSourceCustomPropertyPayload.g.h"

namespace winrt::VirtualCameraManager_WinRT::implementation
{
    struct BasicAugmentedMediaSourceCustomPropertyPayload
        : BasicAugmentedMediaSourceCustomPropertyPayloadT<BasicAugmentedMediaSourceCustomPropertyPayload>,
          PropertyValuePayloadHolder<KsAugmentedCustomControlPropValue>
    {
        BasicAugmentedMediaSourceCustomPropertyPayload(Windows::Foundation::IPropertyValue property,
            winrt::VirtualCameraManager_WinRT::AugmentedMediaSourceCustomControlKind controlKind)
            : PropertyValuePayloadHolder(property),
            m_controlKind(controlKind)
        {}

        uint64_t Capability() { return m_payload->header.Capability; }
        uint64_t Flags() { return m_payload->header.Flags; };
        uint32_t Size() { return m_payload->header.Size; };
        winrt::VirtualCameraManager_WinRT::AugmentedMediaSourceCustomControlKind ControlKind() { return m_controlKind; }

    private:
        winrt::VirtualCameraManager_WinRT::AugmentedMediaSourceCustomControlKind m_controlKind;
    };
}
