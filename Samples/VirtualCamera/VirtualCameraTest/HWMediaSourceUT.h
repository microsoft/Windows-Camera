//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#pragma once

#ifndef HWMEDIASOURCEUT_H
#define HWMEDIASOURCEUT_H

#include "MediaSourceUT_Common.h"
namespace VirtualCameraTest::impl
{
    class HWMediaSourceUT : MediaSourceUT_Common
    {
    public:
        HWMediaSourceUT(winrt::hstring const& devSymlink)
            : m_devSymlink(devSymlink)
        {};

        HRESULT TestMediaSource();
        HRESULT TestMediaSourceStream();
        HRESULT TestKsControl();

        HRESULT TestVirtualCamera();
        
        HRESULT CreateVirtualCamera(winrt::hstring const& postfix, IMFVirtualCamera** ppVirtualCamera);

    protected:
        HWMediaSourceUT() {};
        winrt::hstring m_devSymlink;
    };
}

#endif
