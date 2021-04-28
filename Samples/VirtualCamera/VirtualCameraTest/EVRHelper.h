//
// Copyright (C) Microsoft Corporation. All rights reserved.
//
#pragma once
#ifndef EVRHELPER_H
#define EVRHELPER_H

#include "mfobjects.h"
#include <initguid.h> // this needs to go before mfapi.h
#include "mfapi.h"
#include "mfidl.h"
#include "mfreadwrite.h"
#include "D3D11.h"
#include "evr.h"

#include"vector"

class EVRHelper
{
public:
    HRESULT Initialize(IMFMediaType* pMediaType);
    HRESULT WriteSample(IMFSample* pSample);

    ~EVRHelper();

private:

    HRESULT _CreateSVRSink(IMFMediaType* pMediaType);
    HRESULT _CreateEVRSink(IMFMediaType* pMediaType);
    HRESULT _CreateRenderWindow();

    HRESULT _OffsetTimestamp(IMFSample* pSample);

    wil::unique_hwnd m_PreviewHwnd;

    wil::com_ptr_nothrow<IMFSinkWriter> m_spEVRSinkWriter;
    wil::com_ptr_nothrow<IMFSourceReader> m_spSourceReader;

    wil::com_ptr_nothrow<IMFTransform> m_spCopyMFT;
    wil::com_ptr_nothrow<IMFVideoSampleAllocator> m_spSampleAllocator;
    wil::com_ptr_nothrow<IMFMediaType> m_spOutputMediaType;

    LONGLONG m_llBaseTimestamp;
};

#endif