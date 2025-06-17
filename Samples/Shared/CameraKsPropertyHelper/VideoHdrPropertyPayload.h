// Copyright (c) Microsoft. All rights reserved.

#pragma once
#include "VideoHdrPropertyPayload.g.h"
#include "KSHelper.h"

namespace winrt::CameraKsPropertyHelper::implementation
{
    struct VideoHdrPropertyPayload : 
        VideoHdrPropertyPayloadT<VideoHdrPropertyPayload>, 
        PropertyValuePayloadHolder<KsBasicCameraExtendedPropPayload>
    {
        VideoHdrPropertyPayload(Windows::Foundation::IPropertyValue property)
            : PropertyValuePayloadHolder(property)
        {

        }

        CameraKsPropertyHelper::ExtendedControlKind ExtendedControlKind() { return ExtendedControlKind::KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION; }

    private:
        CameraKsPropertyHelper::ExtendedControlKind m_extendedControlKind;
    };
}
