// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <ks.h>
#include <ksmedia.h>

namespace winrt::CameraKsPropertyHelper::implementation
{

    // Redefining structs normally available in ksmedia.h but not available due to WINAPI_PARTITION_APP
    //
#pragma region KsMediaRedefinition

    struct KSPROPERTY_BOUNDS_LONG {
        ULONG UnsignedMinimum;
        ULONG UnsignedMaximum;
    };

    struct KSPROPERTY_DESCRIPTION {
        ULONG           AccessFlags;
        ULONG           DescriptionSize;
        KSIDENTIFIER    PropTypeSet;
        ULONG           MembersListCount;
        ULONG           Reserved;
    };

    struct KSPROPERTY_MEMBERSHEADER {
        ULONG   MembersFlags;
        ULONG   MembersSize;
        ULONG   MembersCount;
        ULONG   Flags;
    };

    struct KSPROPERTY_STEPPING_LONG {
        ULONG                       SteppingDelta;
        ULONG                       Reserved;
        KSPROPERTY_BOUNDS_LONG      Bounds;
    };

    //
    // end of ksmedia.h redefinitions
#pragma endregion KsMediaRedefinition

    struct FaceRectInfoBlobHeader
    {
        ULONG    Size;  // Size of this header + all FaceRectInfo following
        ULONG    Count; // Number of FaceRectInfo’s in the blob  
    };

    struct FaceRectInfo
    {
        RECT     Region;            // Relative coordinates on the frame that face detection is running (Q31 format)
        LONG     ConfidenceLevel;   // Confidence level of the region being a face ([0, 100])
    };

    struct FaceDetectionROIsMetadata
    {
        FaceRectInfoBlobHeader BlobHeader;
        FaceRectInfo ROIs[1];
    };

    // Helpers
    //

    constexpr
        LONGLONG
        BASE_Q(ULONG n)
    {
        return (((LONGLONG)1) << n);
    }

    // Returns the value from x in fixed - point Q31 format.
    constexpr
        double
        FROM_Q31(LONGLONG x)
    {
        return (double)x / BASE_Q(31);
    }

    // Returns the value from x in fixed - point Q24 format.
    constexpr
        double
        FROM_Q24(LONGLONG x)
    {
        return (double)x / BASE_Q(24);
    }

    struct VIDEOPROCAMP_MEMBERSLIST
    {
        KSPROPERTY_DESCRIPTION      desc;
        KSPROPERTY_MEMBERSHEADER    hdr;
        KSPROPERTY_STEPPING_LONG    step;
    };

    // wrapper for a basic camera extended property payload
    struct KsBasicCameraExtendedPropPayload
    {
        KSCAMERA_EXTENDEDPROP_HEADER header;
        KSCAMERA_EXTENDEDPROP_VALUE value;
    };

    // wrapper for a payload returned when the driver supports the KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION 
    // camera extended property with the KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_MASK capability
    struct KsBackgroundSegmentationPropPayload
    {
        KSCAMERA_EXTENDEDPROP_HEADER header;
        KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_CONFIGCAPS* pConfigCaps;
    };

    template <typename InternalPayloadType>
    class PropertyValuePayloadHolder
    {
    public:

        PropertyValuePayloadHolder(Windows::Foundation::IPropertyValue property)
        {
            m_propContainer = winrt::com_array<uint8_t>(sizeof(InternalPayloadType));
            property.GetUInt8Array(m_propContainer);
            InternalPayloadType* payload = reinterpret_cast<InternalPayloadType*>(&m_propContainer[0]);
            m_payload = payload;
        }

        uint64_t Capability() { return m_payload->header.Capability; }
        uint64_t Flags() { return m_payload->header.Flags; };
        uint32_t Size() { return m_payload->header.Size; };

    protected:
        InternalPayloadType* m_payload;
        Windows::Foundation::IPropertyValue m_property;
        winrt::com_array<uint8_t> m_propContainer;
    };
}
