// Copyright (c) Microsoft. All rights reserved.

#pragma once
#include "BackgroundSegmentationMaskMetadata.g.h"
#include "KSHelper.h"
#include <winrt/Windows.Graphics.Imaging.h>
#include <mutex>

namespace winrt::CameraKsPropertyHelper::implementation
{
    struct BackgroundSegmentationMaskMetadata :
        BackgroundSegmentationMaskMetadataT<BackgroundSegmentationMaskMetadata>,
        PropertyValuePayloadHolder<KSCAMERA_METADATA_BACKGROUNDSEGMENTATIONMASK>
    {
        BackgroundSegmentationMaskMetadata(Windows::Foundation::IPropertyValue property);

        Rect MaskCoverageBoundingBox()
        {
            return
            {
                m_payload->MaskCoverageBoundingBox.left,
                m_payload->MaskCoverageBoundingBox.top,
                m_payload->MaskCoverageBoundingBox.right,
                m_payload->MaskCoverageBoundingBox.bottom
            };
        }
        Windows::Foundation::Size MaskResolution()
        {
            return
            {
                static_cast<float>(m_payload->MaskResolution.cx),
                static_cast<float>(m_payload->MaskResolution.cy)
            };
        }
        Rect ForegroundBoundingBox()
        {
            return
            {
                m_payload->ForegroundBoundingBox.left,
                m_payload->ForegroundBoundingBox.top,
                m_payload->ForegroundBoundingBox.right,
                m_payload->ForegroundBoundingBox.bottom
            };
        }

        Windows::Graphics::Imaging::SoftwareBitmap MaskData();

        FrameMetadataKind FrameMetadataKind() { return FrameMetadataKind::MetadataId_BackgroundSegmentationMask; }

    private:
        std::mutex m_lock;
        Windows::Graphics::Imaging::SoftwareBitmap m_softwareBitmap = nullptr;
    };
}
