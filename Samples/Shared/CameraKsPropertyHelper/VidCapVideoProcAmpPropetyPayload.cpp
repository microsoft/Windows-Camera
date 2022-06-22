// Copyright (c) Microsoft. All rights reserved.

#include "pch.h"
#include "VidCapVideoProcAmpPropetyPayload.h"
#include "VidCapVideoProcAmpPropetyPayload.g.cpp"

namespace winrt::CameraKsPropertyHelper::implementation
{
    VidCapVideoProcAmpPropetyPayload::VidCapVideoProcAmpPropetyPayload(
        winrt::Windows::Foundation::IPropertyValue supportPayloadProperty,
        winrt::Windows::Foundation::IPropertyValue valuePayloadProperty,
        winrt::CameraKsPropertyHelper::VidCapVideoProcAmpKind vidCapVideoProcAmpKind)
        : m_vidCapVideoProcAmpKind(vidCapVideoProcAmpKind)
    {
        m_supportPropContainer = winrt::com_array<uint8_t>(sizeof(VIDEOPROCAMP_MEMBERSLIST));
        supportPayloadProperty.GetUInt8Array(m_supportPropContainer);
        m_supportPayload = reinterpret_cast<VIDEOPROCAMP_MEMBERSLIST*>(&m_supportPropContainer[0]);

        m_valuePropContainer = winrt::com_array<uint8_t>(sizeof(KSPROPERTY_VIDEOPROCAMP_S));
        valuePayloadProperty.GetUInt8Array(m_valuePropContainer);
        m_valuePayload = reinterpret_cast<KSPROPERTY_VIDEOPROCAMP_S*>(&m_valuePropContainer[0]);
    }

    double VidCapVideoProcAmpPropetyPayload::Min()
    {
        return (double)m_supportPayload->step.Bounds.UnsignedMinimum;
    }
    double VidCapVideoProcAmpPropetyPayload::Max()
    {
        return (double)m_supportPayload->step.Bounds.UnsignedMaximum;
    }
    double VidCapVideoProcAmpPropetyPayload::Step()
    {
        return (double)m_supportPayload->step.SteppingDelta;
    }
    int32_t VidCapVideoProcAmpPropetyPayload::Value()
    {
        return m_valuePayload->Value;
    }
}
