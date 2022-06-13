// Copyright (c) Microsoft. All rights reserved.

#pragma once
#include "DigitalWindowMetadata.g.h"
#include "KSHelper.h"

namespace winrt::CameraKsPropertyHelper::implementation
{
    struct DigitalWindowMetadata :
        DigitalWindowMetadataT<DigitalWindowMetadata>,
        PropertyValuePayloadHolder<KSCAMERA_METADATA_DIGITALWINDOW>
    {
        DigitalWindowMetadata(Windows::Foundation::IPropertyValue property);

        double OriginX();
        
        double OriginY();
        
        double WindowSize();
        
        FrameMetadataKind FrameMetadataKind() { return FrameMetadataKind::MetadataId_DigitalWindow; }
    };
}
