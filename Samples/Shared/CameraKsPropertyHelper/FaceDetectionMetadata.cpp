// Copyright (c) Microsoft. All rights reserved.

#include "pch.h"
#include "FaceDetectionMetadata.h"
#include "FaceDetectionMetadata.g.cpp"
#include <MemoryBuffer.h>

using namespace winrt::Windows::Graphics::Imaging;

namespace winrt::CameraKsPropertyHelper::implementation
{
    FaceDetectionMetadata::FaceDetectionMetadata(Windows::Foundation::IPropertyValue property)
        : PropertyValuePayloadHolder(property)
    {
        if (m_propContainer.size() < FIELD_OFFSET(FaceDetectionROIsMetadata, ROIs))
        {
            throw hresult_invalid_argument(L"MF_CAPTURE_METADATA_FACEROIS is not sized properly, expected at least: "
                + to_hstring(FIELD_OFFSET(FaceDetectionROIsMetadata, ROIs))
                + L" but obtained: "
                + to_hstring(m_propContainer.size()));
        }
        if (m_propContainer.size() >= FIELD_OFFSET(FaceDetectionROIsMetadata, ROIs))
        {
            if (m_propContainer.size() != FIELD_OFFSET(FaceDetectionROIsMetadata, ROIs) + (m_payload->BlobHeader.Count * sizeof(FaceRectInfo)))
            {
                throw hresult_invalid_argument(L"MF_CAPTURE_METADATA_FACEROIS is not sized properly, expected exactly: "
                    + to_hstring((int)(FIELD_OFFSET(FaceDetectionROIsMetadata, ROIs) + (m_payload->BlobHeader.Count * sizeof(FaceRectInfo))))
                    + L" but obtained: "
                    + to_hstring(m_propContainer.size()));
            }
            for (ULONG i = 0; i < m_payload->BlobHeader.Count; i++)
            {
                auto entry = *(reinterpret_cast<FaceRectInfo*>(&m_propContainer[0]
                    + sizeof(FaceRectInfoBlobHeader)
                    + i * sizeof(FaceRectInfo)));
                
                FaceDetectionMetadataEntry metadataEntry;

                metadataEntry.ROI.left = FROM_Q31(entry.Region.left);
                metadataEntry.ROI.right = FROM_Q31(entry.Region.right);
                metadataEntry.ROI.top = FROM_Q31(entry.Region.top);
                metadataEntry.ROI.bottom = FROM_Q31(entry.Region.bottom);
                metadataEntry.Confidence = entry.ConfidenceLevel;

                m_entries.Append(metadataEntry);
            }
        }
    }
}
