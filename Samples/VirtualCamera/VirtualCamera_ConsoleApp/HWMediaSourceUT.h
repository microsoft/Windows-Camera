#pragma once

#ifndef HWMEDIASOURCEUT_H
#define HWMEDIASOURCEUT_H

#include "MediaSourceUT_Common.h"

class HWMediaSourceUT : MediaSourceUT_Common
{
public:
    HWMediaSourceUT(winrt::hstring const& devSymlink)
        : m_devSymlink(devSymlink)
    {};

    HRESULT TestMediaSource();
    HRESULT TestMediaSourceStream();
    HRESULT TestCreateVirtualCamera();
    HRESULT TestKSPropertyImp();

    HRESULT CreateVirutalCamera(winrt::hstring const& postfix, IMFVirtualCamera** ppVirtualCamera);

private: 
    winrt::hstring m_devSymlink;
};

#endif
