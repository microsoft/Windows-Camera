//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#pragma once

#ifndef MEDIASOURCEUT_COMMON_H
#define MEDIASOURCEUT_COMMON_H

class MediaSourceUT_Common
{
public:
    static void LogMediaType(IMFMediaType* pMediaType);

protected:
     HRESULT TestMediaSourceRegistration(GUID clsid, IMFAttributes* pAttributes);
     HRESULT ValidateStreaming(IMFSourceReader* pSourceReader, DWORD streamIdx, IMFMediaType* pMediaType);
     HRESULT ValidateVirtualCamera(_In_ IMFVirtualCamera* spVirtualCamera);

     HRESULT _CoCreateAndActivateMediaSource(GUID clsid, IMFAttributes* pAttributes, IMFMediaSource** ppMediaSource);

     HRESULT _CreateCameraDeviceSource(winrt::hstring const& devSymLink, IMFMediaSource** ppMediaSource);

private: 
    static HRESULT ValidateInterfaces(IMFMediaSource* pMediaSource);
};

#endif
