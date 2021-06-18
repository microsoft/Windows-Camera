// Copyright (C) Microsoft Corporation. All rights reserved.

#pragma once

namespace winrt::CameraKsPropertyHelper::implementation
{
    
    // Redefining structs normally available in ksmedia.h but not available due to WINAPI_PARTITION_APP
    //
#pragma region KsMediaRedefinition
    struct KSPROPERTY
    {
        byte Set[16];
        unsigned long Id;
        unsigned long Flags;

        KSPROPERTY(winrt::guid set, unsigned long id = 0, unsigned long flags = 0)
        {
            memcpy(Set, &set, sizeof(Set));
            Id = id;
            Flags = flags;
        }
        KSPROPERTY() : KSPROPERTY(winrt::guid()) {}
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

#pragma endregion BackgroundSegmentation
    //
    // end of ksmedia.h redefinitions
    

#pragma endregion KsMediaRedefinition


    // Helpers
    //

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
