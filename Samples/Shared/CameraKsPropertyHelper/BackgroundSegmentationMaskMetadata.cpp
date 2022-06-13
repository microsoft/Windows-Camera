// Copyright (c) Microsoft. All rights reserved.

#include "pch.h"
#include "BackgroundSegmentationMaskMetadata.h"
#include "BackgroundSegmentationMaskMetadata.g.cpp"
#include <MemoryBuffer.h>

using namespace winrt::Windows::Graphics::Imaging;

namespace winrt::CameraKsPropertyHelper::implementation
{
    BackgroundSegmentationMaskMetadata::BackgroundSegmentationMaskMetadata(Windows::Foundation::IPropertyValue property)
        : PropertyValuePayloadHolder(property)
    {
        if ((m_propContainer.size() - FIELD_OFFSET(KSCAMERA_METADATA_BACKGROUNDSEGMENTATIONMASK, MaskData) - (m_payload->MaskResolution.cx * m_payload->MaskResolution.cy)) != 0)
        {
            throw hresult_invalid_argument(L"MF_CAPTURE_METADATA_FRAME_BACKGROUND_MASK is not sized properly, expected: "
                + to_hstring(FIELD_OFFSET(KSCAMERA_METADATA_BACKGROUNDSEGMENTATIONMASK, MaskData) + (m_payload->MaskResolution.cx * m_payload->MaskResolution.cy))
                + L" but obtained: "
                + to_hstring(m_propContainer.size()));
        }
    }

    Windows::Graphics::Imaging::SoftwareBitmap BackgroundSegmentationMaskMetadata::MaskData()
    {
        const std::lock_guard<std::mutex> lock(m_lock);

        if (m_softwareBitmap == nullptr)
        {
            m_softwareBitmap = SoftwareBitmap(BitmapPixelFormat::Gray8, m_payload->MaskResolution.cx, m_payload->MaskResolution.cy);
            auto buffer = m_softwareBitmap.LockBuffer(BitmapBufferAccessMode::Write);
            auto bufferRef = buffer.CreateReference();
            if (bufferRef == nullptr)
            {
                throw winrt::hresult_access_denied(L"Cannot access pixel buffer of frame.");
            }
            UINT32 capacity = 0;
            BYTE* data = nullptr;
            auto pBufferByteAccess = bufferRef.as<::Windows::Foundation::IMemoryBufferByteAccess>();
            winrt::check_hresult(pBufferByteAccess->GetBuffer(&data, &capacity));
            RtlCopyMemory(data, m_payload->MaskData, m_payload->MaskResolution.cx * m_payload->MaskResolution.cy);
        }
        return m_softwareBitmap;
    }
}
