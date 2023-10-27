// Copyright (c) Microsoft. All rights reserved.

#pragma once
#include "BackgroundSegmentationMaskMetadata.g.h"
#include "KSHelper.h"
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Foundation.h>
#include <mutex>

namespace winrt::CameraKsPropertyHelper::implementation
{
    struct BackgroundSegmentationMaskMetadata :
        BackgroundSegmentationMaskMetadataT<BackgroundSegmentationMaskMetadata>,
        PropertyValuePayloadHolder<KSCAMERA_METADATA_BACKGROUNDSEGMENTATIONMASK>
    {
        BackgroundSegmentationMaskMetadata(Windows::Foundation::IPropertyValue property);

        Windows::Graphics::Imaging::BitmapBounds MaskCoverageBoundingBox()
        {
            return
            {
                static_cast<uint32_t>(m_payload->MaskCoverageBoundingBox.left), // X
                static_cast<uint32_t>(m_payload->MaskCoverageBoundingBox.top), // Y
                static_cast<uint32_t>(m_payload->MaskCoverageBoundingBox.right - m_payload->MaskCoverageBoundingBox.left + 1),// Width
                static_cast<uint32_t>(m_payload->MaskCoverageBoundingBox.bottom - m_payload->MaskCoverageBoundingBox.top + 1) // Height
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
        Windows::Graphics::Imaging::BitmapBounds ForegroundBoundingBox()
        {
            return
            {
                static_cast<uint32_t>(m_payload->ForegroundBoundingBox.left), // X
                static_cast<uint32_t>(m_payload->ForegroundBoundingBox.top), // Y
                static_cast<uint32_t>(m_payload->ForegroundBoundingBox.right - m_payload->ForegroundBoundingBox.left + 1), // Width
                static_cast<uint32_t>(m_payload->ForegroundBoundingBox.bottom - m_payload->ForegroundBoundingBox.top + 1) // Height
            };
        }

        Windows::Graphics::Imaging::SoftwareBitmap MaskData();

        FrameMetadataKind FrameMetadataKind() { return FrameMetadataKind::MetadataId_BackgroundSegmentationMask; }

    private:
        std::mutex m_lock;
        Windows::Graphics::Imaging::SoftwareBitmap m_softwareBitmap = nullptr;
    };
}
