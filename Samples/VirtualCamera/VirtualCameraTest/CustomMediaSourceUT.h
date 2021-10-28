//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#pragma once

#ifndef CUSTOMMEDIASOURCEUT_H
#define CUSTOMMEDIASOURCEUT_H

#include "MediaSourceUT_Common.h"
namespace VirtualCameraTest::impl
{
    class CustomMediaSourceUT : public MediaSourceUT_Common
    {
    public:
        
        HRESULT TestMediaSourceStream();
        HRESULT TestKsControl();
        HRESULT TestCreateVirtualCamera();

    protected:
        CustomMediaSourceUT() = default;
        virtual HRESULT CreateSourceAttributes(_Outptr_ IMFAttributes** ppAttributes);

        winrt::hstring m_strClsid;
        winrt::hstring m_vcamFriendlyName = L"CustomMediaSource_VCam";
        uint32_t m_streamCount = 1;
    };
}

#endif