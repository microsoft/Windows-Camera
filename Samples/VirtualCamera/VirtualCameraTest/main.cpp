#include "pch.h"
#include "SimpleMediaSourceUT.h"
#include "HWMediaSourceUT.h"
#include "VCamUtils.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;

TEST(SimpleMediaSource, TestMediaSource) {

    SimpleMediaSourceUT test;
    EXPECT_HRESULT_SUCCEEDED(test.TestMediaSource());
}

TEST(SimpleMediaSource, TestMediaSourceStream)
{
    SimpleMediaSourceUT test;
    EXPECT_HRESULT_SUCCEEDED(test.TestMediaSourceStream());
}

TEST(SimpleMediaSource, TestKSProperty)
{
    SimpleMediaSourceUT test;
    EXPECT_HRESULT_SUCCEEDED(test.TestKSProperty());
}

TEST(VirtuaCamera_SimpleMediaSource, TestVirtualCamera)
{
    SimpleMediaSourceUT test;
    EXPECT_HRESULT_SUCCEEDED(test.TestVirtualCamera());
}


DeviceInformation SelectRealCamera()
{
    winrt::hstring strSymLink;
    DeviceInformation devInfo{ nullptr };

    std::vector<DeviceInformation> camList;
    if (FAILED(VCamUtils::GetRealCameras(camList)) || (camList.size() == 0))
    {
        LOG_COMMENT(L"No real Camera found");
        return devInfo;
    }

    for (uint32_t i = 0; i < camList.size(); i++)
    {
        auto dev = camList[i];
        LOG_COMMENT(L"[%d] %s \n", i + 1, dev.Id().data());
    }
    LOG_COMMENT(L"select device");
    int devIdx = 0;
    std::wcin >> devIdx;

    if (devIdx <= 0 && devIdx > camList.size())
    {
        LOG_COMMENT(L"Invalid device selection");
        return devInfo;
    }

    return camList[devIdx - 1];
}

TEST(HWMediaSource, TestMediaSource)
{
    auto devInfo = SelectRealCamera();
    if (devInfo)
    {
        HWMediaSourceUT test(devInfo.Id());
        EXPECT_HRESULT_SUCCEEDED(test.TestMediaSource());
    }
    else
    {
        LOG_ERROR(L"No device selected");
    }
}

TEST(HWMediaSource, TestMediaSourceStream)
{
    auto devInfo = SelectRealCamera();
    if (devInfo)
    {
        HWMediaSourceUT test(devInfo.Id());
        EXPECT_HRESULT_SUCCEEDED(test.TestMediaSourceStream());
    }
    else
    {
        LOG_ERROR(L"No device selected");
    }
}

int wmain(int argc, wchar_t* argv[])
{
    init_apartment();
    EnableVTMode();

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
