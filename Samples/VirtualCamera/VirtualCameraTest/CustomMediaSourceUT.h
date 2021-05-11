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
        
        HRESULT TestMediaSource();
        HRESULT TestMediaSourceStream();
        HRESULT TestKsControl();

        HRESULT TestCreateVirtualCamera();

    protected:
        CustomMediaSourceUT() = default;

        winrt::hstring m_strClsid;
        GUID m_clsid = GUID_NULL;
        winrt::hstring m_vcamFriendlyName = L"CustomMediaSource_VCam";
        uint32_t m_streamCount = 1;
    };
}

#endif