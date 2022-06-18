// Copyright (C) Microsoft Corporation. All rights reserved.
#pragma once
#include "VirtualCameraMediaSource.h"

namespace winrt::VirtualCameraManager_WinRT::implementation
{
    // Redefining structs normally available in ksmedia.h but not available due to WINAPI_PARTITION_APP
    //
#pragma region KsMediaRedefinition
    struct KsProperty
    {
        byte Set[16];
        unsigned long Id;
        unsigned long Flags;

        KsProperty(winrt::guid set, unsigned long id = 0, unsigned long flags = 0)
        {
            memcpy(Set, &set, sizeof(Set));
            Id = id;
            Flags = flags;
        }
        KsProperty() : KsProperty(winrt::guid()) {}
    };

    struct KsSimpleCustomControlPropHeader
    {
        uint32_t ColorMode;
    };
    struct KsSimpleCustomControlPropValue
    {
        KsSimpleCustomControlPropHeader header;
    };

    struct KsAugmentedCustomControlPropHeader
    {
        uint64_t Capability;
        uint64_t Flags;
        uint32_t Size;
    };
    struct KsAugmentedCustomControlPropValue
    {
        KsAugmentedCustomControlPropHeader header;
    };
#pragma endregion KsMediaRedefinition

    /// <summary>
    /// Base class implementation for handling a projected KSProperty 
    /// </summary>
    /// <typeparam name="InternalPayloadType"></typeparam>
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

    protected:
        InternalPayloadType* m_payload;
        Windows::Foundation::IPropertyValue m_property;
        winrt::com_array<uint8_t> m_propContainer;
    };

    typedef struct _ACPI_PLD_V2_BUFFER {

        UINT32 Revision : 7;
        UINT32 IgnoreColor : 1;
        UINT32 Color : 24;
        UINT32 Panel : 3;         // Already supported by camera.
        UINT32 CardCageNumber : 8;
        UINT32 Reference : 1;
        UINT32 Rotation : 4;    // 0 - not rotated
                                // 1 - Rotate by 45 degrees clockwise (N/A to camera)
                                // 2 - Rotate by 90 degrees clockwise
                                // 3 - Rotate by 135 degrees clockwise (N/A to camera)
                                // 4 - Rotate by 180 degrees clockwise
                                // 5 - Rotate by 225 degrees clockwise (N/A to camera)
                                // 6 - Rotate by 270 degrees clockwise
        UINT32 Order : 5;
        UINT32 Reserved : 4;

        //
        // _PLD v2 definition fields.
        //

        USHORT VerticalOffset;
        USHORT HorizontalOffset;
    } ACPI_PLD_V2_BUFFER, * PACPI_PLD_V2_BUFFER;



#define DEVPROP_TYPE_BYTE                       0x00000003  // 8-bit unsigned int (BYTE)
#define DEVPROP_TYPEMOD_ARRAY                   0x00001000  // array of fixed-sized data elements
#define DEVPROP_TYPE_BINARY      (DEVPROP_TYPE_BYTE|DEVPROP_TYPEMOD_ARRAY)  // custom binary data

    typedef GUID  DEVPROPGUID, * PDEVPROPGUID;
    typedef ULONG DEVPROPID, * PDEVPROPID;

    typedef struct _DEVPROPKEY {
        DEVPROPGUID fmtid;
        DEVPROPID   pid;
    } DEVPROPKEY, * PDEVPROPKEY;

#define DEFINE_DEVPROPKEY(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8, pid) EXTERN_C const DEVPROPKEY DECLSPEC_SELECTANY name = { { l, w1, w2, { b1, b2,  b3,  b4,  b5,  b6,  b7,  b8 } }, pid }
DEFINE_DEVPROPKEY(DEVPKEY_Device_PhysicalDeviceLocation, 0x540b947e, 0x8b40, 0x45bc, 0xa8, 0xa2, 0x6a, 0x0b, 0x89, 0x4c, 0xbd, 0xa2, 9); /* DEVPROP_TYPE_BINARY */
}