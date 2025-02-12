// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once

#include <ks.h>
#include <ksmedia.h>

namespace winrt::CameraKsPropertyHelper::implementation
{
    /*static GUID KSPROPERTYSETID_WindowsCameraEffect =
    { 0x1666d655, 0x21a6, 0x4982, 0x97, 0x28, 0x52, 0xc3, 0x9e, 0x86, 0x9f, 0x90 };*/

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

    // The struct below is defined in ksmedia.h with Windows SDK preview 25967 and above.
    // Redefined here for convenience until release SDK comes out so that you can compile against 
    // SDK 22621
    struct KSCAMERA_EXTENDEDPROP_FIELDOFVIEW2_CONFIGCAPS
    {
        WORD DefaultDiagonalFieldOfViewInDegrees; // The default FoV value for the driver/device
        WORD DiscreteFoVStopsCount;               // Count of use FoV entries in DiscreteFoVStops array
        WORD DiscreteFoVStops[360];               // Descending list of FoV Stops in degrees
        ULONG Reserved;
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

    // wrapper for a camera extended property payload that contains a header and a KSCAMERA_EXTENDEDPROP_VIDEOPROCSETTING
    struct KsVidProcCameraExtendedPropPayload
    {
        KSCAMERA_EXTENDEDPROP_HEADER header;
        KSCAMERA_EXTENDEDPROP_VIDEOPROCSETTING vidProcSetting;
    };

    // wrapper for a payload returned when the driver supports the KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION 
    // camera extended property with the KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_MASK capability
    struct KsBackgroundSegmentationPropPayload
    {
        KSCAMERA_EXTENDEDPROP_HEADER header;
        KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_CONFIGCAPS* pConfigCaps;
    };

    // wrapper for a payload returned when the driver supports the KSPROPERTY_CAMERACONTROL_EXTENDED_FIELDOFVIEW2_CONFIGCAPS 
    struct KsFieldOfView2ConfigCapsPropPayload
    {
        KSCAMERA_EXTENDEDPROP_HEADER header;
        KSCAMERA_EXTENDEDPROP_FIELDOFVIEW2_CONFIGCAPS configCaps;
    };

    // wrapper for a payload returned when the driver supports KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_SUPPORTED 
    struct KsWindowsStudioSupportedPropPayload
    {
        KSCAMERA_EXTENDEDPROP_HEADER header;
        KSPROPERTY* pSupportedControls;
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
        InternalPayloadType* Payload() { return m_payload; }

    protected:
        InternalPayloadType* m_payload;
        Windows::Foundation::IPropertyValue m_property;
        winrt::com_array<uint8_t> m_propContainer;
    };
}
