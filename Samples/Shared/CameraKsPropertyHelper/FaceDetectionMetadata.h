// Copyright (c) Microsoft. All rights reserved.

#pragma once
#include "FaceDetectionMetadata.g.h"
#include "KSHelper.h"
#include <winrt/Windows.Graphics.Imaging.h>
#include <mutex>

namespace winrt::CameraKsPropertyHelper::implementation
{
    struct FaceDetectionMetadata :
        FaceDetectionMetadataT<FaceDetectionMetadata>,
        PropertyValuePayloadHolder<FaceDetectionROIsMetadata>
    {
        FaceDetectionMetadata(Windows::Foundation::IPropertyValue property);

        Windows::Foundation::Collections::IVectorView<CameraKsPropertyHelper::FaceDetectionMetadataEntry> Entries() { return m_entries.GetView(); }

        FrameMetadataKind FrameMetadataKind() { return FrameMetadataKind::MetadataId_FaceROIs; }

    private:
        std::mutex m_lock;
        Windows::Foundation::Collections::IVector<CameraKsPropertyHelper::FaceDetectionMetadataEntry> m_entries = single_threaded_vector<CameraKsPropertyHelper::FaceDetectionMetadataEntry>();
    };
}
