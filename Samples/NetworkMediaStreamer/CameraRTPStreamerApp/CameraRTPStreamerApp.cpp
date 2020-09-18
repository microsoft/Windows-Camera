// Copyright (C) Microsoft Corporation. All rights reserved.


#include <iostream>
#include <iomanip>
#include <fstream>
#include <Mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mfapi.h>
#include <windows.media.h>
#include <windows.media.core.interop.h>
#include <windows.foundation.h>
#include <windows.Storage.streams.h>
#include <winrt\base.h>
#include <winrt\Windows.Media.Capture.h>
#include <winrt\Windows.Media.Capture.Frames.h>
#include <winrt\Windows.Graphics.Imaging.h>
#include <winrt\Windows.Foundation.h>
#include <winrt\Windows.Devices.Enumeration.h>
#include <winrt\Windows.Media.Devices.h>
#include <winrt\Windows.Media.MediaProperties.h>
#include <winrt\Windows.Networking.Connectivity.h>
#include <winrt\Windows.storage.streams.h>

#include "..\Common\inc\NetworkMediaStreamer.h"
#include "..\Common\inc\RTPMediaStreamer.h"
#include "..\Common\inc\RTSPServerControl.h"
using namespace winrt;
using namespace Windows;
using namespace Media::Capture;
using namespace Frames;
using namespace Windows::Graphics::Imaging;
using namespace Windows::Foundation;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Media;
using namespace Windows::Media::Devices;
using namespace Windows::Media::MediaProperties;
constexpr uint16_t ServerPort = 8554;
constexpr uint16_t SecureServerPort = 6554;

// Uncomment the following if you want to use FrameReader API instead of the Record-to-sink APIs
//#define USE_FR 

// sample test code to get localhost test certificate
std::vector<PCCERT_CONTEXT> getServerCertificate()
{
    std::vector<PCCERT_CONTEXT> aCertContext;
    PCCERT_CONTEXT pCertContext = NULL;
    //-------------------------------------------------------
    // Open the My store, also called the personal store.
    // This call to CertOpenStore opens the Local_Machine My 
    // store as opposed to the Current_User's My store.

    auto hMyCertStore = CertOpenStore(CERT_STORE_PROV_SYSTEM,
        X509_ASN_ENCODING,
        0,
        CERT_SYSTEM_STORE_LOCAL_MACHINE,
        L"MY");

    if (hMyCertStore == NULL)
    {
        printf("Error opening MY store for server.\n");
        goto cleanup;
    }
    //-------------------------------------------------------
    // Search for a certificate with some specified
    // string in it. This example attempts to find
    // a certificate with the string "example server" in
    // its subject string. Substitute an appropriate string
    // to find a certificate for a specific user.
    do
    {
        pCertContext = CertFindCertificateInStore(hMyCertStore,
            X509_ASN_ENCODING,
            0,
            CERT_FIND_SUBJECT_STR_W,
            L"localhost", // use appropriate subject name
            pCertContext
        );
        if (pCertContext)
        {
            aCertContext.push_back(CertDuplicateCertificateContext(pCertContext));
        }
    } while (pCertContext);
    if (aCertContext.empty())
    {
        printf("Error retrieving server certificate.");
    }
cleanup:
    if (hMyCertStore)
    {
        CertCloseStore(hMyCertStore, 0);
    }
    return aCertContext;
}

std::map<winrt::hstring, std::vector<GUID>> streamMap =
{
    {L"/h264", {MFVideoFormat_H264}},
};

#ifdef USE_FR
IMFSinkWriter* InitSinkWriter(IMFMediaSink *pMediaSink, MediaFrameFormat format)
{
    com_ptr<IMFSinkWriter> spSinkWriter;
    com_ptr<IMFAttributes> spSWAttributes;
    winrt::com_ptr<IMFMediaType> spInType;
    std::map<hstring, GUID> subtypeMap =
    {
        {L"NV12", MFVideoFormat_NV12},
        {L"YUY2", MFVideoFormat_YUY2},
        {L"IYUV", MFVideoFormat_IYUV},
        {L"ARGB32", MFVideoFormat_ARGB32}
    };
    auto subType = subtypeMap[format.Subtype()];
    auto width = format.VideoFormat().Width();
    auto height = format.VideoFormat().Height();
    check_hresult(MFCreateMediaType(spInType.put()));
    check_hresult(MFSetAttributeSize(spInType.get(), MF_MT_FRAME_SIZE, width, height));
    check_hresult(MFSetAttributeRatio(spInType.get(), MF_MT_FRAME_RATE, format.FrameRate().Numerator(), format.FrameRate().Denominator()));

    check_hresult(spInType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
    check_hresult(spInType->SetGUID(MF_MT_SUBTYPE, subType ));

    check_hresult(MFCreateAttributes(spSWAttributes.put(), 2));
    check_hresult(spSWAttributes->SetUINT32(MF_LOW_LATENCY, TRUE));
    HRESULT hr = S_OK;
    BOOL bEnableHWTransforms = FALSE;
    do
    {
        spSinkWriter = nullptr;
        check_hresult(spSWAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, bEnableHWTransforms));
        check_hresult(MFCreateSinkWriterFromMediaSink(pMediaSink, spSWAttributes.get(), spSinkWriter.put()));
        hr = spSinkWriter->SetInputMediaType(0, spInType.get(), nullptr);
        bEnableHWTransforms = !bEnableHWTransforms;
    } while (hr == MF_E_TOPO_CODEC_NOT_FOUND);
    check_hresult(hr);
    check_hresult(spSinkWriter->BeginWriting());

    return spSinkWriter.detach();
}
#endif

PROCESS_INFORMATION StartLoggerConsole(std::wstring filename)
{
        STARTUPINFO si,si1;
        PROCESS_INFORMATION pi;
        GetStartupInfo(&si1);
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        ZeroMemory(&pi, sizeof(pi));
        std::wstring cmdline = L"powershell Get-Content " + filename + L" -Wait ";
        std::wstring title = L"RTSPServer Log Console";
        si.lpTitle = (LPWSTR)title.c_str();
        
        si.dwX = si1.dwX + 500;
        si.dwY = si1.dwY;
        si.dwXCountChars = 40;
        si.dwYCountChars = 10;
        si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USEPOSITION | STARTF_PREVENTPINNING | STARTF_USECOUNTCHARS;
        si.wShowWindow = SW_SHOWNOACTIVATE;
        // Start the child process. 
        if (!CreateProcess(NULL,   // No module name (use command line)
            (LPWSTR)cmdline.c_str(),        // Command line
            NULL,           // Process handle not inheritable
            NULL,           // Thread handle not inheritable
            FALSE,          // Set handle inheritance to FALSE
            CREATE_NEW_CONSOLE,              // No creation flags
            NULL,           // Use parent's environment block
            NULL,           // Use parent's starting directory 
            &si,            // Pointer to STARTUPINFO structure
            &pi)            // Pointer to PROCESS_INFORMATION structure
            )
        {
            check_win32(GetLastError());
        }
        return pi;
    
}

void StopLoggerConsole(PROCESS_INFORMATION pi)
{
    TerminateProcess(pi.hProcess, 0);
    // Close process and thread handles. 
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
}

winrt::hstring GetDeviceSelectionFromUser()
{
    auto filteredDevices = DeviceInformation::FindAllAsync(DeviceClass::VideoCapture).get();
    if (!filteredDevices.Size())
    {
        throw_hresult(MF_E_NO_CAPTURE_DEVICES_AVAILABLE);
    }
    int selection = 1;
    auto deviceIter = filteredDevices.First();
    while (deviceIter.HasCurrent())
    {
        auto device = deviceIter.Current();
        std::wcout << std::endl << selection++ << L". " << device.Name().c_str() << L":" << device.Id().c_str();
        deviceIter.MoveNext();
    }
    std::cout << "\nEnter Selection:";
    std::cin >> selection; selection--;
    return filteredDevices.GetAt(selection).Id();
}
void GetUserSelectionAndSetMediaFormat(MediaCapture &mc)
{
    auto formats = mc.VideoDeviceController().GetAvailableMediaStreamProperties(MediaStreamType::VideoRecord);
    IMediaEncodingProperties selectedProp(nullptr);
    uint32_t idx = 1;
    for (auto f : formats)
    {
        auto format = f.try_as<VideoEncodingProperties>();
        float fr = (float)format.FrameRate().Numerator() / (float)format.FrameRate().Denominator();
        std::wcout << L"\n" << idx++ << L". " << format.Width() << L"x" << format.Height() << L"@" << /*std::setprecision(2) <<*/ fr << L":" << format.Subtype().c_str();
    }
    std::cout << "Enter format choice: ";
    std::cin >> idx; idx--;
    selectedProp = formats.GetAt(idx);
    mc.VideoDeviceController().SetMediaStreamPropertiesAsync(MediaStreamType::VideoRecord, selectedProp).get();
}

int main()
{
    std::cout << "VideoSteamer App \n";
    PROCESS_INFORMATION loggerConsole;
    try
    {
       
        std::wofstream fileLogger("RTSPServer.log");
        fileLogger << L"StartLogging..\n";
        loggerConsole = StartLoggerConsole(L"RTSPServer.log");
       
        auto mc = MediaCapture();
        auto s = MediaCaptureInitializationSettings();
        s.VideoDeviceId(GetDeviceSelectionFromUser());
#ifdef USE_FR
        s.MemoryPreference(MediaCaptureMemoryPreference::Cpu);
#else
        s.MemoryPreference(MediaCaptureMemoryPreference::Auto);
#endif // USE_FR

        s.StreamingCaptureMode(StreamingCaptureMode::Video);
        mc.InitializeAsync(s).get();
        GetUserSelectionAndSetMediaFormat(mc);

        auto streamers = winrt::RTSPSuffixSinkMap();
        BitmapSize sz;
        auto selectedFormat = mc.VideoDeviceController().GetMediaStreamProperties(MediaStreamType::VideoRecord).as<VideoEncodingProperties>(); //selectedProp.as<VideoEncodingProperties>();
        sz.Height = selectedFormat.Height();
        sz.Width = selectedFormat.Width();
        MediaRatio frameRate = selectedFormat.FrameRate();
        for (auto&& strm : streamMap)
        {
            std::vector<IMFMediaType*> mediaTypes;
            winrt::com_ptr<IMFMediaType> spOutType;
            winrt::check_hresult(MFCreateMediaType(spOutType.put()));
            winrt::check_hresult(spOutType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
            winrt::check_hresult(spOutType->SetGUID(MF_MT_SUBTYPE, strm.second[0]));
            winrt::check_hresult(spOutType->SetUINT32(MF_MT_AVG_BITRATE, sz.Width * 1000));

            winrt::check_hresult(spOutType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));
            winrt::check_hresult(MFSetAttributeSize(spOutType.get(), MF_MT_FRAME_SIZE, sz.Width, sz.Height));
            winrt::check_hresult(MFSetAttributeRatio(spOutType.get(), MF_MT_FRAME_RATE, frameRate.Numerator() * 100, frameRate.Denominator() * 100));
            mediaTypes.push_back(spOutType.get());
            IMediaExtension mediaExtSink;
            winrt::check_hresult(CreateRTPMediaSink(mediaTypes.data(), (DWORD)mediaTypes.size(), (IMFMediaSink**)put_abi(mediaExtSink)));
            streamers.Insert(winrt::hstring(strm.first), mediaExtSink);
            mediaTypes.clear();
            spOutType = nullptr;
        }

        com_ptr<IRTSPServerControl> serverHandle, serverHandleSecure;
        com_ptr<IRTSPAuthProvider> m_spAuthProvider;
        check_hresult(GetAuthProviderInstance(AuthType::Digest, L"RTSPServer", m_spAuthProvider.put()));
        // add a default user for testing
        m_spAuthProvider.as<IRTSPAuthProviderCredStore>()->AddUser(L"user", L"pass");
        check_hresult(CreateRTSPServer(streamers.as<ABI::RTSPSuffixSinkMap>().get(), ServerPort, false, m_spAuthProvider.get(),{},0, serverHandle.put() ));
#ifndef  DISABLE_SECURE_RTSP
        try
        {
            com_ptr<IRTSPAuthProvider> spBasicAuthProvider;
            check_hresult(GetAuthProviderInstance(AuthType::Basic, L"RTSPServer", spBasicAuthProvider.put()));

            auto serverCerts = getServerCertificate();
            check_hresult(CreateRTSPServer(streamers.as<ABI::RTSPSuffixSinkMap>().get(), SecureServerPort, true, spBasicAuthProvider.get(), serverCerts.data(), serverCerts.size(), serverHandleSecure.put()));
        }
        catch (hresult_error const& ex)
        {
            std::wcout << L"\nError starting secure server: " << std::hex << ex.code() << std::dec << L":" << ex.message().c_str();
            serverHandleSecure = nullptr;
        }
#endif
        auto loggerdelegate = winrt::LogHandler([&fileLogger](auto er, auto msg)
        {
            fileLogger << msg.c_str() << "\n";
            if (er)
            {
                fileLogger << L" ErrCode:" << std::hex << er <<"\n" << std::dec;
                std::wcout << msg.c_str();
                std::wcout << L" ErrCode:" << std::hex << er << "\n" << std::dec;
            }
            fileLogger.flush();

        });
        for (int i = 0; i < (int)LoggerType::LOGGER_MAX; i++)
        {
            EventRegistrationToken t1, t2;
            winrt::check_hresult(serverHandle->AddLogHandler((LoggerType)i, loggerdelegate.as<ABI::LogHandler>().get(), &t1));
            serverHandleSecure ? winrt::check_hresult(serverHandleSecure->AddLogHandler((LoggerType)i, loggerdelegate.as<ABI::LogHandler>().get(), &t2)) : void(0);
        }
        winrt::check_hresult(serverHandle->StartServer());
        serverHandleSecure ? winrt::check_hresult(serverHandleSecure->StartServer()) : void(0);
#ifdef USE_FR
        auto fsources = mc.FrameSources();
        MediaFrameSource selectedFs(nullptr);
        std::vector<hstring> preferredsubTypes = { L"NV12", L"YUY2", L"IYUV", L"ARGB32" };
        for (auto&& fs : fsources)
        {
            if (fs.Value().Controller().VideoDeviceController().Id() == s.VideoDeviceId())
            {
                selectedFs = fs.Value();
            }
        }
        auto fr = mc.CreateFrameReaderAsync(selectedFs, selectedFs.CurrentFormat().Subtype(), sz).get();
        fr.AcquisitionMode(MediaFrameReaderAcquisitionMode::Realtime);
        slim_mutex m;
        std::vector<com_ptr<IMFSinkWriter>> spSinkWriters;
        auto strmIter = streamers.First();
        for (uint32_t i = 0; i < streamers.Size(); i++)
        {
            winrt::com_ptr<IMFSinkWriter> spSW;
            spSW.attach(InitSinkWriter(strmIter.Current().Value().as<IMFMediaSink>().get(), selectedFs.CurrentFormat()));
            spSinkWriters.push_back(spSW);
            strmIter.MoveNext();
        }
        fr.FrameArrived([&](MediaFrameReader mfr, MediaFrameArrivedEventArgs args)
            {
                slim_lock_guard g(m);
                auto mf = mfr.TryAcquireLatestFrame();
                if (!mf) return;
                auto vmf = mf.VideoMediaFrame();
                if (!vmf) return;
                auto vf = vmf.GetVideoFrame();
                if (!vf) return;
                winrt::com_ptr<IMFSample> spSample;
                vf.as<IVideoFrameNative>()->GetData(__uuidof(IMFSample), spSample.put_void());
                for(auto&& sinkWriter : spSinkWriters)
                {
                    sinkWriter->WriteSample(0, spSample.get());
                }
            });
        fr.StartAsync();
#else

        auto me = MediaEncodingProfile::CreateMp4(VideoEncodingQuality::Auto);
        auto lowLagRec = mc.PrepareLowLagRecordToCustomSinkAsync(me, streamers.First().Current().Value().as<IMediaExtension>()).get();
        lowLagRec.StartAsync().get();
#endif

        std::cout << "\nStarted Capture and RTSP Server.\n";
        auto hostnames = winrt::Windows::Networking::Connectivity::NetworkInformation::GetHostNames();
        std::cout << "\nAvailable links:\n";
        for (auto&& hname : hostnames)
        {
            for (auto&& s : streamers)
            {
                std::wcout << L"rtsp://" << hname.DisplayName().c_str() << L":" << ServerPort << s.Key().c_str() << std::endl;
                if (serverHandleSecure)
                {
                    std::wcout << L"rtsps://" << hname.DisplayName().c_str() << L":" << SecureServerPort << s.Key().c_str() << std::endl;
                }
            }
        }

        char c = 1;
        while (c != 0)
        {
            std::cout << "\n 1. Add User\n 2. Remove User \n 0. Quit\n Your choice:" ;

            std::cin >> c;
            switch (c)
            {
            case '1':
            {
                std::string username, password;
                std::cout << "\nEnter username :-  ";
                std::cin >> username;
                std::cout << "\nEnter new password :- ";
                std::cin >> password;
                check_hresult( m_spAuthProvider.as<IRTSPAuthProviderCredStore>()->AddUser(winrt::to_hstring(username).c_str(), winrt::to_hstring(password).c_str()));
            }
                break;
            case '2':
            {
                std::string username;
                std::cout << "\nEnter username :-  ";
                std::cin >> username;
                check_hresult(m_spAuthProvider.as<IRTSPAuthProviderCredStore>()->RemoveUser(winrt::to_hstring(username).c_str()));
            }
                break;
            case '0':
                c = 0;
                break;
            default:
                break;
            }

        } 
#ifdef USE_FR
        fr.StopAsync();
#else
        lowLagRec.StopAsync().get();
        lowLagRec.FinishAsync().get();
#endif
        check_hresult(m_spAuthProvider.as<IRTSPAuthProviderCredStore>()->RemoveUser(L"user"));
        
    }
    catch (hresult_error const& ex)
    {
        std::wcout << L"Error: " << ex.code() << L":" << ex.message().c_str();
    }

    StopLoggerConsole(loggerConsole);

}
