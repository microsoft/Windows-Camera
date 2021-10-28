// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once
#include "Common.h"
#include "BasicAugmentedMediaSourceCustomPropertyPayload.g.h"

namespace winrt::VirtualCameraManager_WinRT::implementation
{
    struct BasicAugmentedMediaSourceCustomPropertyPayload
        : BasicAugmentedMediaSourceCustomPropertyPayloadT<BasicAugmentedMediaSourceCustomPropertyPayload>,
          PropertyValuePayloadHolder<KsBasicCameraExtendedPropPayload>
    {
        BasicAugmentedMediaSourceCustomPropertyPayload(Windows::Foundation::IPropertyValue property,
            winrt::VirtualCameraManager_WinRT::AugmentedMediaSourceCustomControlKind controlKind)
            : PropertyValuePayloadHolder(property),
            m_controlKind(controlKind)
        {}

        winrt::VirtualCameraManager_WinRT::AugmentedMediaSourceCustomControlKind ControlKind() { return m_controlKind; }

    private:
        winrt::VirtualCameraManager_WinRT::AugmentedMediaSourceCustomControlKind m_controlKind;
    };
}
