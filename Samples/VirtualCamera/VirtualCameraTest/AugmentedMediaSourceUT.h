//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#pragma once

#ifndef AUGMENTEDMEDIASOURCEUT_H
#define AUGMENTEDMEDIASOURCEUT_H

#include "MediaSourceUT_Common.h"
#include "VirtualCameraMediaSource.h"
namespace VirtualCameraTest::impl
{
    class AugmentedMediaSourceUT : public MediaSourceUT_Common
    {
    public:
        AugmentedMediaSourceUT(winrt::hstring const& devSymlink)
            : m_devSymlink(devSymlink)
        {
            m_clsid = CLSID_VirtualCameraMediaSource;
        };

        HRESULT TestMediaSourceStream();
        HRESULT TestKsControl();

        HRESULT TestCreateVirtualCamera();
        
        HRESULT CreateVirtualCamera(winrt::hstring const& postfix, MFVirtualCameraLifetime vcamLifetime, MFVirtualCameraAccess vcamAccess, IMFVirtualCamera** ppVirtualCamera);

    protected:
        AugmentedMediaSourceUT() { m_clsid = CLSID_VirtualCameraMediaSource; };
        winrt::hstring m_devSymlink;
        virtual HRESULT CreateSourceAttributes(_Outptr_ IMFAttributes** ppAttributes);
    };
}

#endif
