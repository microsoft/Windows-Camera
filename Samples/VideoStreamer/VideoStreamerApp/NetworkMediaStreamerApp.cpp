// VideoStreamerApp.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include "..\VideoStreamerLib\inc\NetworkMediaStreamer.h"
#include <iostream>
#include <Mferror.h>
#include <windows.media.h>
#include<windows.media.core.interop.h>
#include<winrt\base.h>
#include <winrt\Windows.Media.Capture.h>
#include <winrt\Windows.Media.Capture.Frames.h>
#include <winrt\Windows.Graphics.Imaging.h>
#include <winrt\Windows.Foundation.h>
#include <winrt\Windows.Devices.Enumeration.h>
#include <winrt\Windows.Media.Devices.h>
#include <winrt\Windows.Media.MediaProperties.h>
#include <winrt\Windows.Networking.Connectivity.h>
#include <mfreadwrite.h>

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
        //goto cleanup;
    }
cleanup:
    if (hMyCertStore)
    {
        CertCloseStore(hMyCertStore, 0);
    }
    return aCertContext;
}

std::map<std::string, std::vector<GUID>> streamMap =
{
    {"/h264", {MFVideoFormat_H264}},
    //{"/hevc",MFVideoFormat_HEVC},
    //{"/mpeg2",MFVideoFormat_MPEG2}
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
int main()
{
    std::cout << "VideoSteamer App \n";
    try
    {
        GUID inFormat = MFVideoFormat_NV12;
        auto filteredDevices = DeviceInformation::FindAllAsync(DeviceClass::VideoCapture).get();
        if (!filteredDevices.Size())
        {
            throw_hresult(MF_E_NO_CAPTURE_DEVICES_AVAILABLE);
        }
        auto deviceIter = filteredDevices.First();
        while (deviceIter.HasCurrent())
        {
            auto device = deviceIter.Current();
            std::wcout << std::endl << device.Name().c_str() << L":" << device.Id().c_str();
            deviceIter.MoveNext();
        }
        int selection;
        std::cout << "\nEnter Selection:";
        std::cin >> selection;
        auto mc = MediaCapture();
        auto s = MediaCaptureInitializationSettings();
        s.VideoDeviceId(filteredDevices.GetAt(selection).Id());
        s.MemoryPreference(MediaCaptureMemoryPreference::Cpu);
        s.StreamingCaptureMode(StreamingCaptureMode::Video);
        mc.InitializeAsync(s).get();
        std::map<std::string, winrt::com_ptr<IMFMediaSink>> streamers;

        auto sz = BitmapSize();
        std::cout << "Enter Resolution Width: ";
        std::cin >> sz.Width;
        std::cout << "Enter Resolution Height: ";
        std::cin >> sz.Height;

        for (auto strm : streamMap)
        {

            std::vector<IMFMediaType*> mediaTypes;
            winrt::com_ptr<IMFMediaType> spOutType;
            winrt::check_hresult(MFCreateMediaType(spOutType.put()));
            winrt::check_hresult(spOutType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video));
            winrt::check_hresult(spOutType->SetGUID(MF_MT_SUBTYPE, strm.second[0]));
            winrt::check_hresult(spOutType->SetUINT32(MF_MT_AVG_BITRATE, sz.Width * 1000));

            winrt::check_hresult(spOutType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive));
            winrt::check_hresult(MFSetAttributeSize(spOutType.get(), MF_MT_FRAME_SIZE, sz.Width, sz.Height));
            winrt::check_hresult(MFSetAttributeRatio(spOutType.get(), MF_MT_FRAME_RATE, (int)(30 * 100), 100));
            mediaTypes.push_back(spOutType.get());
            streamers[strm.first].attach(CreateRTPMediaSink(mediaTypes));
            mediaTypes.clear();
            spOutType = nullptr;
        }

        com_ptr<IRTSPServerControl> serverHandle, serverHandle1;
        serverHandle.attach(CreateRTSPServer(streamers, ServerPort, false));
#if 0
        auto ServerCerts = getServerCertificate();
        serverHandle1.attach(CreateRTSPServer(streamers, SecureServerPort, true, ServerCerts));
#else
        serverHandle1.attach(CreateRTSPServer(streamers, SecureServerPort, false));
#endif
        auto loggerdelegate = [](auto er, auto msg)
        {
            std::wcout << msg.c_str();
            if (er) std::wcout << L" ErrCode:" << std::hex << er;
        };
        for (int i = 0; i < (int)LoggerType::LOGGER_MAX; i++)
        {
            serverHandle->AddLogHandler((LoggerType)i, loggerdelegate);
            serverHandle1->AddLogHandler((LoggerType)i, loggerdelegate);
        }
        serverHandle->StartServer();
        serverHandle1->StartServer();
#ifdef USE_FR
        auto fsources = mc.FrameSources();
        MediaFrameSource selectedFs(nullptr);
        std::vector<hstring> preferredsubTypes = { L"NV12", L"YUY2", L"IYUV", L"ARGB32" };
        for (auto fs : fsources)
        {
            if (fs.Value().Controller().VideoDeviceController().Id() != s.VideoDeviceId()) continue;
            auto prefSubType = preferredsubTypes.begin();
            do
            {
                if ((fs.Value().Info().MediaStreamType() == MediaStreamType::VideoPreview)
                    || (fs.Value().Info().MediaStreamType() == MediaStreamType::VideoRecord)
                    )
                {
                    auto formats = fs.Value().SupportedFormats();
                    //std::cout << fs.Value().Info().MediaStreamType();
                    for (auto format : formats)
                    {
                        std::wcout << L"\n" << format.VideoFormat().Width() << L"x" << format.VideoFormat().Height() << L":" << format.Subtype().c_str();
                        if ((format.VideoFormat().Height() == sz.Height)
                            && (format.VideoFormat().Width() == sz.Width)
                            && (((float)format.FrameRate().Numerator() / (float)format.FrameRate().Denominator()) > 24)
                            && (format.Subtype() == *prefSubType)
                            )
                        {
                            fs.Value().SetFormatAsync(format).get();
                            selectedFs = fs.Value();
                            break;
                        }
                    }
                    if (selectedFs)
                        break;
                }
                if (++prefSubType == preferredsubTypes.end()) break;
            } while (!selectedFs);
        }
        auto fr = mc.CreateFrameReaderAsync(selectedFs, selectedFs.CurrentFormat().Subtype(), sz).get();
        fr.AcquisitionMode(MediaFrameReaderAcquisitionMode::Realtime);
        slim_mutex m;
        com_ptr<IMFSinkWriter> spSinkWriter; 
        spSinkWriter.attach(InitSinkWriter(streamers.begin()->second.get(), selectedFs.CurrentFormat()));

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
                for (DWORD i = 0; i < streamers.size(); i++)
                {
                    spSinkWriter->WriteSample(i, spSample.get());
                }
                
            });
        fr.StartAsync();
#else
        std::vector<hstring> preferredsubTypes = { L"NV12", L"YUY2", L"IYUV", L"ARGB32" };
        auto formats = mc.VideoDeviceController().GetAvailableMediaStreamProperties(MediaStreamType::VideoRecord);
        IMediaEncodingProperties selectedProp(nullptr);
        auto prefSubType = preferredsubTypes.begin();
        do
        {
            for (auto f : formats)
            {
                auto format = f.try_as<VideoEncodingProperties>();
                std::wcout << L"\n" << format.Width() << L"x" << format.Height() << L":" << format.Subtype().c_str();
                if ((format.Height() == sz.Height)
                    && (format.Width() == sz.Width)
                    && (((float)format.FrameRate().Numerator() / (float)format.FrameRate().Denominator()) > 24)
                    && (format.Subtype() == *prefSubType)
                    )
                {
                    selectedProp = f;
                    break;
                }
            }
            if (selectedProp) break;
        } while(++prefSubType != preferredsubTypes.end());
        mc.VideoDeviceController().SetMediaStreamPropertiesAsync(MediaStreamType::VideoRecord, selectedProp).get();
        auto me = MediaEncodingProfile::CreateMp4(VideoEncodingQuality::Auto);
        auto lowLagRec = mc.PrepareLowLagRecordToCustomSinkAsync(me, streamers.begin()->second.as<IMediaExtension>()).get();
        lowLagRec.StartAsync().get();
#endif

        std::cout << "\nStarted Capture and RTSP Server.\n";
        auto hostnames = winrt::Windows::Networking::Connectivity::NetworkInformation::GetHostNames();
        std::cout << "\nAvailable links:\n";
        for (auto hname : hostnames)
        {
            for (auto s : streamers)
            {
                std::wcout << L"rtsp://" << hname.DisplayName().c_str() << L":" << ServerPort << s.first.c_str() << std::endl;
                std::wcout << L"rtsps://" << hname.DisplayName().c_str() << L":" << SecureServerPort << s.first.c_str() << std::endl;
            }
        }
        std::cin.seekg(std::cin._Seekend);
        std::cout << "Press any key to stop...";
        std::cin.get();

#ifdef USE_FR
        fr.StopAsync();
#else
        lowLagRec.StopAsync().get();
        lowLagRec.FinishAsync().get();
#endif
    }
    catch (hresult_error const& ex)
    {
        std::wcout << L"Error: " << ex.code() << L":" << ex.message().c_str();
    }
}
