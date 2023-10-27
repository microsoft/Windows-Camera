// Copyright (c) Microsoft. All rights reserved.

#pragma once
#include "VidProcExtendedPropertyPayload.g.h"
#include "KSHelper.h"


namespace winrt::CameraKsPropertyHelper::implementation
{
    struct VidProcExtendedPropertyPayload :
        VidProcExtendedPropertyPayloadT<VidProcExtendedPropertyPayload>,
        PropertyValuePayloadHolder<KsVidProcCameraExtendedPropPayload>
    {
        VidProcExtendedPropertyPayload(Windows::Foundation::IPropertyValue property, CameraKsPropertyHelper::ExtendedControlKind extendedControlKind)
            : PropertyValuePayloadHolder(property),
            m_extendedControlKind(extendedControlKind)
        {
        }

        int Min() { return m_payload->vidProcSetting.Min; }
        int Max() { return m_payload->vidProcSetting.Max; }
        int Step() { return m_payload->vidProcSetting.Step; }
        unsigned int Value() { return m_payload->vidProcSetting.VideoProc.Value.ul; }

        CameraKsPropertyHelper::ExtendedControlKind ExtendedControlKind() { return m_extendedControlKind; }

    private:
        CameraKsPropertyHelper::ExtendedControlKind m_extendedControlKind;
    };
}
