// Copyright (c) Microsoft. All rights reserved.

#pragma once
#include "BasicWindowsStudioPropertyPayload.g.h"
#include "KSHelper.h"

namespace winrt::CameraKsPropertyHelper::implementation
{
    struct BasicWindowsStudioPropertyPayload : 
        BasicWindowsStudioPropertyPayloadT<BasicWindowsStudioPropertyPayload>,
        PropertyValuePayloadHolder<KsBasicCameraExtendedPropPayload>
    {
        BasicWindowsStudioPropertyPayload(Windows::Foundation::IPropertyValue property, CameraKsPropertyHelper::WindowsStudioControlKind windowsStudioControlKind)
            : PropertyValuePayloadHolder(property),
            m_windowsStudioControlKind(windowsStudioControlKind)
        {}

        winrt::CameraKsPropertyHelper::WindowsStudioControlKind WindowsStudioControlKind() { return m_windowsStudioControlKind; }

    private:
        CameraKsPropertyHelper::WindowsStudioControlKind m_windowsStudioControlKind;
    };
}
