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

    struct KsCameraExtendedPropHeader {
        ULONG               Version;
        ULONG               PinId;
        ULONG               Size;
        ULONG               Result;
        ULONGLONG           Flags;
        ULONGLONG           Capability;
    };

    struct KsCameraExtendedPropValue {
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

    struct KsBasicCameraExtendedPropPayload
    {
        KsCameraExtendedPropHeader header;
        KsCameraExtendedPropValue value;
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

        uint64_t Capability() { return m_payload->header.Capability; }
        uint64_t Flags() { return m_payload->header.Flags; };
        uint32_t Size() { return m_payload->header.Size; };

    protected:
        InternalPayloadType* m_payload;
        Windows::Foundation::IPropertyValue m_property;
        winrt::com_array<uint8_t> m_propContainer;
    };
}