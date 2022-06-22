// Copyright (c) Microsoft. All rights reserved.

#include "pch.h"
#include "DigitalWindowMetadata.h"
#include "DigitalWindowMetadata.g.cpp"

namespace winrt::CameraKsPropertyHelper::implementation
{
    DigitalWindowMetadata::DigitalWindowMetadata(Windows::Foundation::IPropertyValue property)
        : PropertyValuePayloadHolder(property)
    {}

    double DigitalWindowMetadata::OriginX()
    {
        return FROM_Q24(m_payload->Window.OriginX);
    }
    
    double DigitalWindowMetadata::OriginY()
    {
        return FROM_Q24(m_payload->Window.OriginY);
    }
    
    double DigitalWindowMetadata::WindowSize()
    {
        return FROM_Q24(m_payload->Window.WindowSize);
    }
    
}
