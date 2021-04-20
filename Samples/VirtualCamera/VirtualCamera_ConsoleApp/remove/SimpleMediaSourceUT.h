#pragma once

#ifndef SIMPLEMEDIASOURCEUT_H
#define SIMPLEMEDIASOURCEUT_H

#include "MediaSourceUT_Common.h"

class SimpleMediaSourceUT : MediaSourceUT_Common
{
public:
    // General simplemediasource test
    HRESULT TestMediaSource();
    HRESULT TestMediaSourceStream();
    HRESULT TestKSProperty();

     // virtualcamera with simplemediasource test
    HRESULT TestVirtualCamera();
    //HRESULT TestCustomControl();

    // Helper functions for SimpleMediaSource
    HRESULT CreateVirtualCamera(IMFVirtualCamera** ppVirtualCamera);
    static HRESULT GetColorMode(IMFMediaSource* pMediaSource, uint32_t* pColorMode);
    static HRESULT SetColorMode(IMFMediaSource* pMediaSource, uint32_t colorMode);
};

#endif
