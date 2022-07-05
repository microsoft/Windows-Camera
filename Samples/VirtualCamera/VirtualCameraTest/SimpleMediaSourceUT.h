//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#pragma once

#ifndef SIMPLEMEDIASOURCEUT_H
#define SIMPLEMEDIASOURCEUT_H

#include "MediaSourceUT_Common.h"
#include "VirtualCameraMediaSource.h"
namespace VirtualCameraTest::impl
{
    class SimpleMediaSourceUT : public MediaSourceUT_Common
    {
    public:
        SimpleMediaSourceUT()
        {
            m_clsid = CLSID_VirtualCameraMediaSource;
        };

        // General simplemediasource test
        HRESULT TestMediaSourceStream();
        HRESULT TestKsControl();

        // virtualcamera with simplemediasource test
        HRESULT TestCreateVirtualCamera();

        // Helper functions for SimpleMediaSource
        HRESULT CreateVirtualCamera(MFVirtualCameraLifetime vcamLifetime, MFVirtualCameraAccess vcamAccess, IMFVirtualCamera** ppVirtualCamera);
        static HRESULT GetColorMode(IMFMediaSource* pMediaSource, uint32_t* pColorMode);
        static HRESULT SetColorMode(IMFMediaSource* pMediaSource, uint32_t colorMode);

    protected:
        virtual HRESULT CreateSourceAttributes(_Outptr_ IMFAttributes** ppAttributes);
    };
}

#endif
