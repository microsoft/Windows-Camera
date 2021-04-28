
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#include "pch.h"
#include "SimpleMediaSourceUT.h"
#include "HWMediaSourceUT.h"
#include "VCamUtils.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;

//
// Define SimpleMediaSource test case
//
TEST(SimpleMediaSourceTest, TestMediaSource) {

    VirtualCameraTest::impl::SimpleMediaSourceUT test;
    EXPECT_HRESULT_SUCCEEDED(test.TestMediaSource());
}

TEST(SimpleMediaSourceTest, TestMediaSourceStream)
{
    VirtualCameraTest::impl::SimpleMediaSourceUT test;
    EXPECT_HRESULT_SUCCEEDED(test.TestMediaSourceStream());
}

TEST(SimpleMediaSourceTest, TestKsControl)
{
    VirtualCameraTest::impl::SimpleMediaSourceUT test;
    EXPECT_HRESULT_SUCCEEDED(test.TestKsControl());
}

//
// Define VirtualCamera_SimpleMediaSource test case
//
TEST(VirtualCameraSimpleMediaSourceTest, TestVirtualCamera)
{
    VirtualCameraTest::impl::SimpleMediaSourceUT test;
    EXPECT_HRESULT_SUCCEEDED(test.TestVirtualCamera());
}

//
// Define HWMediaSource test case
//
class HWMediaSourceTest : public DataDrivenTestBase, public VirtualCameraTest::impl::HWMediaSourceUT
{
public:
    HRESULT TestSetup()
    {
        auto value = TestData().find(L"VidDeviceSymLink");
        if (TestData().end() != value)
        {
            m_devSymlink = value->second;
        }
        else if (TestData().end() != (value = TestData().find(L"VidDeviceIndex")))
        {
            int index = _wtoi((value->second).c_str());
            std::vector< DeviceInformation> cameralist;
            RETURN_IF_FAILED(VCamUtils::GetPhysicalCameras(cameralist));
            if (index >= 0 && index < cameralist.size())
            {
                m_devSymlink = cameralist[index].Id();
            }
            else
            {
                LOG_ERROR_RETURN(E_TEST_FAILED, L"Test setup failed, invalid device index: %d", index);
            }
        }
        else
        {
            LOG_ERROR_RETURN(E_TEST_FAILED, L"Test setup failed, missing data");
        }
        LOG_COMMENT(L"Using physical camera: %s", m_devSymlink.data());
        return S_OK;
    }

    virtual void SetUp()
    {
        ASSERT_TRUE(SUCCEEDED(TestSetup()));
    };
};

TEST_P(HWMediaSourceTest, TestMediaSource)
{
    EXPECT_HRESULT_SUCCEEDED(TestMediaSource());
};

TEST_P(HWMediaSourceTest, TestMediaSourceStream)
{
    EXPECT_HRESULT_SUCCEEDED(TestMediaSourceStream());
};

TEST_P(HWMediaSourceTest, TestKsControl)
{
    EXPECT_HRESULT_SUCCEEDED(TestKsControl());
};

INSTANTIATE_DATADRIVENTEST_CASE_P(HWMediaSourceTest, L"VirtualCameraTestData.xml", L"HWMediaSourceTest");

//
// Define VirtualCameraHWMediaSource test case
//
using VirtualCameraHWMediaSourceTest = HWMediaSourceTest;
TEST_P(VirtualCameraHWMediaSourceTest, TestVirtualCamera)
{
    EXPECT_HRESULT_SUCCEEDED(TestVirtualCamera());
};

INSTANTIATE_DATADRIVENTEST_CASE_P(VirtualCameraHWMediaSourceTest, L"VirtualCameraTestData.xml", L"VirtualCameraHWMediaSourceTest");



void __stdcall WilFailureLog(_In_ const wil::FailureInfo& failure) WI_NOEXCEPT
{
    LOG_WARNING(L"%S(%d):%S, hr=0x%08X, msg=%S",
        failure.pszFile, failure.uLineNumber, failure.pszFunction,
        failure.hr, (failure.pszMessage) ? failure.pszMessage : L"");
}

int wmain(int argc, wchar_t* argv[])
{
    init_apartment();
    EnableVTMode();
    wil::SetResultLoggingCallback(WilFailureLog);
    bool mfShutdown = false;

    auto exit = wil::scope_exit([&] {
        if (mfShutdown)
        {
            MFShutdown();
        }
    });

    RETURN_IF_FAILED(MFStartup(MF_VERSION));
    mfShutdown = true;

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
