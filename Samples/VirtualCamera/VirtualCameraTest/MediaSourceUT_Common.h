//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#pragma once

#ifndef MEDIASOURCEUT_COMMON_H
#define MEDIASOURCEUT_COMMON_H

namespace VirtualCameraTest::impl
{
    class MediaSourceUT_Common
    {
    public:
        static void LogMediaType(IMFMediaType* pMediaType);
        static HRESULT ValidateStreaming(IMFSourceReader* pSourceReader, DWORD streamIdx, IMFMediaType* pMediaType);

    protected:
        HRESULT TestMediaSourceRegistration(GUID clsid, IMFAttributes* pAttributes);
        HRESULT TestMediaSourceStream(IMFMediaSource* pMediaSource);
        HRESULT ValidateKsControl_UnSupportedControl(IKsControl* pKsControl);
        HRESULT ValidateVirtualCamera(_In_ IMFVirtualCamera* spVirtualCamera);
        HRESULT CoCreateAndActivateMediaSource(GUID clsid, IMFAttributes* pAttributes, IMFMediaSource** ppMediaSource);
        HRESULT CreateCameraDeviceSource(winrt::hstring const& devSymLink, IMFMediaSource** ppMediaSource);

    private:
        static HRESULT ValidateInterfaces(IMFMediaSource* pMediaSource);
    };
}

#endif
