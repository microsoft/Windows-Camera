// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

namespace winrt::CameraKsPropertyHelper::implementation
{

    // Redefining structs normally available in ksmedia.h but not available due to WINAPI_PARTITION_APP
    //
#pragma region KsMediaRedefinition

    static GUID PROPSETID_VIDCAP_VIDEOPROCAMP = { 0xC6E13360L, 0x30AC, 0x11d0, 0xa1, 0x8c, 0x00, 0xA0, 0xC9, 0x11, 0x89, 0x56 };
    static GUID KSPROPERTYSETID_ExtendedCameraControl = { 0x1CB79112, 0xC0D2, 0x4213, 0x9C, 0xA6, 0xCD, 0x4F, 0xDB, 0x92, 0x79, 0x72 };

    struct KSPROPERTY
    {
        union
        {
            struct
            {
                union
                {
                    byte Set[16];
                    GUID SetGuid;
                };
                unsigned long Id;
                unsigned long Flags;
            };
            LONGLONG allign;
        };

        KSPROPERTY(winrt::guid set, unsigned long id = 0, unsigned long flags = 0)
        {
            memcpy(Set, &set, sizeof(Set));
            Id = id;
            Flags = flags;
        }
        KSPROPERTY() : KSPROPERTY(winrt::guid()) {}
    };

    struct KSIDENTIFIER {
        union {
            KSPROPERTY Property;
            LONGLONG   Alignment;
        };
    };

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

    struct KSPROPERTY_VIDEOPROCAMP_S {
        KSPROPERTY Property;
        LONG   Value;                       // Value to set or get
        ULONG  Flags;                       // KSPROPERTY_VIDEOPROCAMP_FLAGS_*
        ULONG  Capabilities;                // KSPROPERTY_VIDEOPROCAMP_FLAGS_*
    };

    struct KSCAMERA_EXTENDEDPROP_HEADER {
        ULONG               Version;
        ULONG               PinId;
        ULONG               Size;
        ULONG               Result;
        ULONGLONG           Flags;
        ULONGLONG           Capability;
    };

    struct KSCAMERA_EXTENDEDPROP_VALUE {
        union
        {
            double          dbl;
            ULONGLONG       ull;
            ULONG           ul;
            ULARGE_INTEGER  ratio;
            LONG            l;
            LONGLONG        ll;
        };
    };

    struct KSCAMERA_METADATA_ITEMHEADER
    {
        ULONG   MetadataId;
        ULONG   Size;
    };

    // BackgroundSegmentation-related
    //
#pragma region BackgroundSegmentation

    struct KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_CONFIGCAPS
    {
        // Output width and height in pixels
        SIZE Resolution;

        // The maximum frame rate the driver can accommodate  for achieving 
        // background segmentation for each frame corresponding to Resolution
        struct
        {
            LONG Numerator;
            LONG Denominator;
        } MaxFrameRate;

        // The width and height of the mask produced when streaming
        // with a MediaType corresponding to Resolution and MaxFrameRate
        SIZE MaskResolution;

        // Optional subtype for which this configuration capability applies. If left 
        // to zero, all streams conforming the Resolution and MaxFrameRate will support 
        // background segmentation with the specified MaskResolution.
        GUID SubType;

    };

    struct KSCAMERA_METADATA_BACKGROUNDSEGMENTATIONMASK
    {
        KSCAMERA_METADATA_ITEMHEADER Header;
        RECT MaskCoverageBoundingBox;
        SIZE MaskResolution;
        RECT ForegroundBoundingBox;
        BYTE MaskData[1]; // Array of mask data of dimension MaskResolution.cx * MaskResolution.cy
    };

    //
    // end of BackgroundSegmentation-related
#pragma endregion BackgroundSegmentation

    //
    // end of ksmedia.h redefinitions
#pragma endregion KsMediaRedefinition

    struct FaceRectInfoBlobHeader
    {
        ULONG    Size;      // Size of this header + all FaceRectInfo following
        ULONG    Count;    // Number of FaceRectInfo’s in the blob  
    };

    struct FaceRectInfo
    {
        RECT     Region;     // Relative coordinates on the frame that face detection is running (Q31 format)
        LONG     ConfidenceLevel;    // Confidence level of the region being a face ([0, 100])
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

    // !@brief Returns the value from x in fixed - point Q24 format.
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

    struct KSCAMERA_EXTENDEDPROP_DIGITALWINDOW_SETTING
    {
        LONG        OriginX;                // in Q24
        LONG        OriginY;                // in Q24
        LONG        WindowSize;             // in Q24
        ULONG       Reserved;
    };

    struct KSCAMERA_METADATA_DIGITALWINDOW 
    {
        KSCAMERA_METADATA_ITEMHEADER Header;
        KSCAMERA_EXTENDEDPROP_DIGITALWINDOW_SETTING Window;
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
