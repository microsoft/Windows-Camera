//
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#include "pch.h"
#include "MediaCaptureUtils.h"

CMediaCaptureUtils::CMediaCaptureUtils() {
}

HRESULT CMediaCaptureUtils::GetDeviceList(winrt::hstring const& strDeviceSelector, DeviceInformationCollection& devList)
{
    winrt::IAsyncOperation<DeviceInformationCollection> enumerateTask;

    LOG_COMMENT(L"Device Selector string: %s \n", strDeviceSelector.data());
    TRY_RETURN_CAUGHT_EXCEPTION_MSG(enumerateTask = DeviceInformation::FindAllAsync(strDeviceSelector),
        "Fail to enumerate devices!");
    TRY_RETURN_CAUGHT_EXCEPTION_MSG(devList = MediaCaptureTaskHelper::RunTaskWithTimeout(enumerateTask).get(), "Fail to enumerate devices! (async)");

    return S_OK;
}

HRESULT CMediaCaptureUtils::GetDeviceInfo(winrt::hstring const& strDevSymLink, DeviceInformation& devInfo)
{
    devInfo = nullptr;
    TRY_RETURN_CAUGHT_EXCEPTION_MSG(
        devInfo = MediaCaptureTaskHelper::RunTaskWithTimeout(DeviceInformation::CreateFromIdAsync(strDevSymLink)).get(),
        "Error: DeviceInformation::FromIdAsync() failed!");

    return S_OK;
}
