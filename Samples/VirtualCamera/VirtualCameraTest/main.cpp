
// Copyright (C) Microsoft Corporation. All rights reserved.
//

#include "pch.h"
#include "SimpleMediaSourceUT.h"
#include "HWMediaSourceUT.h"
#include "CustomMediaSourceUT.h"
#include "AugmentedMediaSourceUT.h"
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
TEST(VirtualCameraSimpleMediaSourceTest, TestCreateVirtualCamera)
{
    VirtualCameraTest::impl::SimpleMediaSourceUT test;
    EXPECT_HRESULT_SUCCEEDED(test.TestCreateVirtualCamera());
}

//
// Define HWMediaSource test case
//
class HWMediaSourceTest : 
    public DataDrivenTestBase, 
    public VirtualCameraTest::impl::HWMediaSourceUT
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
TEST_P(VirtualCameraHWMediaSourceTest, TestCreateVirtualCamera)
{
    EXPECT_HRESULT_SUCCEEDED(TestCreateVirtualCamera());
};

INSTANTIATE_DATADRIVENTEST_CASE_P(VirtualCameraHWMediaSourceTest, L"VirtualCameraTestData.xml", L"VirtualCameraHWMediaSourceTest");

//
// Define CustomMediaSource test case
//
class CustomMediaSourceTest : 
     public DataDrivenTestBase,
     public VirtualCameraTest::impl::CustomMediaSourceUT
{
public:
    HRESULT TestSetup()
    {
        auto value = TestData().find(L"Clsid");
        if (TestData().end() != value)
        {
            m_strClsid = value->second;
            RETURN_IF_FAILED(CLSIDFromString(m_strClsid.data(), &m_clsid));
        }
        else
        {
            LOG_ERROR_RETURN(E_TEST_FAILED, L"Test setup failed, missing data");
        }
        LOG_COMMENT(L"Testing media source with clsid: %s", m_strClsid.data());

        value = TestData().find(L"Friendlyname");
        if (TestData().end() != value)
        {
            m_vcamFriendlyName = value->second;
        }

        value = TestData().find(L"StreamCount");
        if(TestData().end() != value)
        {
            m_streamCount = (uint32_t)(_wtoi((value->second).c_str()));
        }

        return S_OK;
    }

    virtual void SetUp()
    {
        ASSERT_TRUE(SUCCEEDED(TestSetup()));
    };
};

TEST_P(CustomMediaSourceTest, TestMediaSource)
{
    EXPECT_HRESULT_SUCCEEDED(TestMediaSource());
};

TEST_P(CustomMediaSourceTest, TestMediaSourceStream)
{
    EXPECT_HRESULT_SUCCEEDED(TestMediaSourceStream());
};

TEST_P(CustomMediaSourceTest, TestKsControl)
{
    EXPECT_HRESULT_SUCCEEDED(TestKsControl());
};

INSTANTIATE_DATADRIVENTEST_CASE_P(CustomMediaSourceTest, L"VirtualCameraTestData.xml", L"CustomMediaSourceTest");

//
// Define VirtualCameraCustomMediaSource test case
//
using VirtualCameraCustomMediaSourceTest = CustomMediaSourceTest;
TEST_P(VirtualCameraCustomMediaSourceTest, TestCreateVirtualCamera)
{
    EXPECT_HRESULT_SUCCEEDED(TestCreateVirtualCamera());
};

INSTANTIATE_DATADRIVENTEST_CASE_P(VirtualCameraCustomMediaSourceTest, L"VirtualCameraTestData.xml", L"VirtualCameraCustomMediaSourceTest");


void __stdcall WilFailureLog(_In_ const wil::FailureInfo& failure) WI_NOEXCEPT
{
    LOG_WARNING(L"%S(%d):%S, hr=0x%08X, msg=%S",
        failure.pszFile, failure.uLineNumber, failure.pszFunction,
        failure.hr, (failure.pszMessage) ? failure.pszMessage : L"");
}

//
// Define AugmentedMediaSource test case
//
class AugmentedMediaSourceTest :
    public DataDrivenTestBase,
    public VirtualCameraTest::impl::AugmentedMediaSourceUT
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
            std::vector<DeviceInformation> cameralist;
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

TEST_P(AugmentedMediaSourceTest, TestMediaSource)
{
    EXPECT_HRESULT_SUCCEEDED(TestMediaSource());
};

TEST_P(AugmentedMediaSourceTest, TestMediaSourceStream)
{
    EXPECT_HRESULT_SUCCEEDED(TestMediaSourceStream());
};

TEST_P(AugmentedMediaSourceTest, TestKsControl)
{
    EXPECT_HRESULT_SUCCEEDED(TestKsControl());
};

INSTANTIATE_DATADRIVENTEST_CASE_P(AugmentedMediaSourceTest, L"VirtualCameraTestData.xml", L"AugmentedMediaSourceTest");

//
// Define VirtualCameraAugmentedMediaSource test case
//
using VirtualCameraAugmentedMediaSourceTest = AugmentedMediaSourceTest;
TEST_P(VirtualCameraAugmentedMediaSourceTest, TestCreateVirtualCamera)
{
    EXPECT_HRESULT_SUCCEEDED(TestCreateVirtualCamera());
};

INSTANTIATE_DATADRIVENTEST_CASE_P(VirtualCameraAugmentedMediaSourceTest, L"VirtualCameraTestData.xml", L"VirtualCameraAugmentedMediaSourceTest");

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
