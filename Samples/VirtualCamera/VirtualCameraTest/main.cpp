//
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


//
// Define HWMediaSource TestCase test case
//
class GTestHWMediaSourceUT : public DataDrivenTestBase, public HWMediaSourceUT
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
        return S_OK;
    }

    virtual void SetUp()
    {
        ASSERT_TRUE(TestSetup());
    };
};

TEST_P(GTestHWMediaSourceUT, TestMediaSource)
{
    EXPECT_HRESULT_SUCCEEDED(TestMediaSource());
};

TEST_P(GTestHWMediaSourceUT, TestMediaSourceStream)
{
    EXPECT_HRESULT_SUCCEEDED(TestMediaSourceStream());
};

INSTANTIATE_DATADRIVENTEST_CASE_P(GTestHWMediaSourceUT, L"VirtualCameraTestData.xml", L"HWMediaSourceUTSetup");



//
//class SampleDataDrivenTest : public DataDrivenTestBase
//{
//public:
//    SampleDataDrivenTest() {};
//    bool InitMyTestData() { return false; }
//
//    virtual void SetUp() {
//        ASSERT_TRUE(InitMyTestData());
//    }
//    void Test1()
//    {
//        auto value = TestData().find(L"VidDeviceSymLink");
//        if (TestData().end() != value)
//        {
//            LOG_COMMENT(L"use device symlink");
//        }
//        else if(TestData().end() != (value = TestData().find(L"VidDeviceIndex")))
//        {
//            LOG_COMMENT(L"use device INDEX");
//        }
//        else
//        {
//            LOG_COMMENT(L"missing data");
//        }
//    }
//};
//
//
//TEST_P(SampleDataDrivenTest, Test1)
//{
//    Test1();
//};
//
//INSTANTIATE_DATADRIVENTEST_CASE_P(SampleDataDrivenTest, L"TestXml.xml", L"Test");

void __stdcall WilFailureLog(_In_ const wil::FailureInfo& failure) WI_NOEXCEPT
{
    LOG_ERROR(L"%S(%d):%S, hr=0x%08X, msg=%s",
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
