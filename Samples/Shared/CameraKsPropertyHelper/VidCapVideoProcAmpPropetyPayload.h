// Copyright (c) Microsoft. All rights reserved.

#pragma once
#include "VidCapVideoProcAmpPropetyPayload.g.h"
#include "KSHelper.h"

namespace winrt::CameraKsPropertyHelper::implementation
{
    struct VidCapVideoProcAmpPropetyPayload : VidCapVideoProcAmpPropetyPayloadT<VidCapVideoProcAmpPropetyPayload>
    {
        VidCapVideoProcAmpPropetyPayload(winrt::Windows::Foundation::IPropertyValue supportPayloadProperty, winrt::Windows::Foundation::IPropertyValue valuePayloadProperty, VidCapVideoProcAmpKind vidCapVideoProcAmpKind);

        winrt::CameraKsPropertyHelper::VidCapVideoProcAmpKind VidCapVideoProcAmpKind() { return m_vidCapVideoProcAmpKind; }
        double Min();
        double Max();
        double Step();
        int32_t Value();

    private:
        winrt::CameraKsPropertyHelper::VidCapVideoProcAmpKind m_vidCapVideoProcAmpKind;
        VIDEOPROCAMP_MEMBERSLIST* m_supportPayload;
        KSPROPERTY_VIDEOPROCAMP_S* m_valuePayload;
        winrt::com_array<uint8_t> m_supportPropContainer;
        winrt::com_array<uint8_t> m_valuePropContainer;
    };
}
