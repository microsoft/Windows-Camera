// Copyright (c) Microsoft. All rights reserved.

#pragma once
#include "PropertyInquiry.g.h"


namespace winrt::CameraKsPropertyHelper::implementation
{
    struct PropertyInquiry
    {
        PropertyInquiry() = default;

        static winrt::CameraKsPropertyHelper::IVidCapVideoProcAmpPropetyPayload PropertyInquiry::GetVidCapVideoProcAmp(
            winrt::Windows::Media::Devices::VideoDeviceController const& controller,
            winrt::CameraKsPropertyHelper::VidCapVideoProcAmpKind const& vidCapVideoProcAmpKind);
        static void PropertyInquiry::SetVidCapVideoProcAmpValue(
            winrt::Windows::Media::Devices::VideoDeviceController const& controller,
            winrt::CameraKsPropertyHelper::VidCapVideoProcAmpKind const& vidCapVideoProcAmpKind,
            double value);
        
        static winrt::CameraKsPropertyHelper::IExtendedPropertyHeader GetExtendedControl(
            winrt::Windows::Media::Devices::VideoDeviceController const& controller,
            winrt::CameraKsPropertyHelper::ExtendedControlKind const& extendedControlKind);
        static void SetExtendedControlFlags(
            winrt::Windows::Media::Devices::VideoDeviceController const& controller,
            winrt::CameraKsPropertyHelper::ExtendedControlKind const& extendedControlKind,
            uint64_t flags);
        static void SetExtendedControlFlagsAndValue(
            winrt::Windows::Media::Devices::VideoDeviceController const& controller,
            winrt::CameraKsPropertyHelper::ExtendedControlKind const& extendedControlKind,
            uint64_t flags,
            uint64_t value);

        static winrt::CameraKsPropertyHelper::IMetadataPayload ExtractFrameMetadata(
            winrt::Windows::Media::Capture::Frames::MediaFrameReference const& frame,
            winrt::CameraKsPropertyHelper::FrameMetadataKind const& metadataKind);
    };
}
namespace winrt::CameraKsPropertyHelper::factory_implementation
{
    struct PropertyInquiry : PropertyInquiryT<PropertyInquiry, implementation::PropertyInquiry>
    {
    };
}
