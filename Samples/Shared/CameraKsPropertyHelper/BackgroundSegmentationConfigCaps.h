// Copyright (c) Microsoft. All rights reserved.

#pragma once
#include "BackgroundSegmentationConfigCaps.g.h"
#include "KSHelper.h"


namespace winrt::CameraKsPropertyHelper::implementation
{
    struct BackgroundSegmentationConfigCaps : BackgroundSegmentationConfigCapsT<BackgroundSegmentationConfigCaps>
    {
        BackgroundSegmentationConfigCaps(KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_CONFIGCAPS configCaps)
            : m_configCaps(configCaps) {};

        Windows::Foundation::Size Resolution() { return Windows::Foundation::Size(static_cast<float>(m_configCaps.Resolution.cx), static_cast<float>(m_configCaps.Resolution.cy)); };
        int32_t MaxFrameRateNumerator() { return m_configCaps.MaxFrameRate.Numerator; };
        int32_t MaxFrameRateDenominator() { return m_configCaps.MaxFrameRate.Denominator; };
        Windows::Foundation::Size MaskResolution() { return Windows::Foundation::Size(static_cast<float>(m_configCaps.MaskResolution.cx), static_cast<float>(m_configCaps.MaskResolution.cy)); };
        winrt::guid SubType() { return m_configCaps.SubType; };

    private:
        KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_CONFIGCAPS m_configCaps;
    };
}
