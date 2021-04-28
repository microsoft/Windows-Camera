//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#pragma once

#ifndef SIMPLEMEDIASOURCEUT_H
#define SIMPLEMEDIASOURCEUT_H

#include "MediaSourceUT_Common.h"
namespace VirtualCameraTest::impl
{
    class SimpleMediaSourceUT : MediaSourceUT_Common
    {
    public:
        // General simplemediasource test
        HRESULT TestMediaSource();
        HRESULT TestMediaSourceStream();
        HRESULT TestKsControl();

        // virtualcamera with simplemediasource test
        HRESULT TestCreateVirtualCamera();

        // Helper functions for SimpleMediaSource
        HRESULT CreateVirtualCamera(IMFVirtualCamera** ppVirtualCamera);
        static HRESULT GetColorMode(IMFMediaSource* pMediaSource, uint32_t* pColorMode);
        static HRESULT SetColorMode(IMFMediaSource* pMediaSource, uint32_t colorMode);
    };
}

#endif
