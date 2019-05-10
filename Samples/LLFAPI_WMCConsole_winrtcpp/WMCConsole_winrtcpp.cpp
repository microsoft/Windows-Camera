// LLFAPI_WMCConsole_winrtcpp.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <Windows.h>
#include <iostream>
#include <conio.h>
#include <winrt\base.h>
#include <winrt\Windows.Foundation.h>
#include <winrt\Windows.Foundation.Collections.h>
#include <winrt\Windows.Media.Capture.h>
#include <winrt\Windows.Media.Core.h>
#include <winrt\Windows.Media.Devices.h>
#include <winrt\Windows.Media.MediaProperties.h>
#include <winrt\Windows.Media.Capture.Frames.h>

#include <winrt\Windows.Devices.Enumeration.h>
#include <winrt\Windows.Graphics.Imaging.h>
#include <winrt\Windows.Storage.h>
#include <winrt\Windows.Storage.Streams.h>

using namespace winrt;
using namespace Windows::Media::Capture;
using namespace Windows::Media::MediaProperties;
using namespace Windows::Media::Capture::Frames;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Devices::Enumeration;
using namespace Windows::Storage;
using namespace Windows::Graphics::Imaging;
using namespace Windows::Media::Core;

// Iterates through source groups and filter-out the frame sources which have IR,depth and other sources which we cannot consume in this app
IVector<MediaFrameSourceInfo> GetFilteredSourceGroupList()
{
    auto sourcegroups = MediaFrameSourceGroup::FindAllAsync().get();
    auto filteredGroup = single_threaded_vector<MediaFrameSourceInfo>();

    auto iter = sourcegroups.First();
    while (iter.HasCurrent())
    {
        auto infos = iter.Current().SourceInfos().First();
        // iterate through sources and filter-out the IR,depth and other sources which we cannot consume in this app
        while (infos.HasCurrent())
        {
            if ((infos.Current().MediaStreamType() == MediaStreamType::Photo
                || infos.Current().MediaStreamType() == MediaStreamType::VideoPreview
                || infos.Current().MediaStreamType() == MediaStreamType::VideoRecord)
                && infos.Current().SourceKind() == MediaFrameSourceKind::Color)
            {
                filteredGroup.Append(infos.Current());
            }
            infos.MoveNext();
        }
        iter.MoveNext();
    }
    return filteredGroup;
}

int GetSGSelection(IVector<MediaFrameSourceInfo> filteredGroup)
{
    unsigned int idx = 0;
    auto grp = filteredGroup.First();
    while (grp.HasCurrent())
    {
        idx++;
        auto curr = grp.Current();
        std::wstring streamtypes[] = { L"VideoPreview", L"VideoRecord", L"Audio", L"Photo" };
        std::wstring Paneltypes[] = {
            L"Unknown",
            L"Front",
            L"Back",
            L"Top",
            L"Bottom",
            L"Left",
            L"Right"
        };
        auto loc = curr.DeviceInformation().EnclosureLocation();
        std::wstring panelloc;
        if (loc)
        {
            panelloc = Paneltypes[(int)loc.Panel()];
        }
        
        std::wcout << idx << L":" << curr.DeviceInformation().Name().c_str() << ":" << streamtypes[(int)curr.MediaStreamType()].c_str() << panelloc.c_str() << std::endl;
        grp.MoveNext();
    }
    do
    {
        std::wcout << L"Enter selection:";
        std::cin >> idx;
    } while ((idx > filteredGroup.Size()) || (idx < 1));
    return idx;
}

int GetMediaTypeSelection(MediaFrameSourceInfo selectedSource)
{
    auto Iter = selectedSource.VideoProfileMediaDescription().First();
    int idx = 0;
    while (Iter.HasCurrent())
    {

        auto fmt = Iter.Current();
        int frameRateInt = (int)fmt.FrameRate();
        int frameRateDec = ((int)(fmt.FrameRate() * 100) % 100);
        idx++;
        auto fmtstr = std::to_wstring(idx) + L". " + std::wstring(fmt.Subtype().c_str()) + L": " +
            std::to_wstring(fmt.Width()) + L"x" + std::to_wstring(fmt.Height()) + L" @ " + std::to_wstring(frameRateInt) + L"." + std::to_wstring(frameRateDec) + L" fps";
        std::wcout << fmtstr << std::endl;
        Iter.MoveNext();
    }
    int selection;
    do
    {
        std::wcout << L"Enter selection:";
        std::cin >> selection;
    } while ((selection > idx) || (selection < 1));
    return selection;
}
int wmain(int argc, wchar_t *args[])
{

    std::cout << "Windows Media Capture in Win32 desktop Console app!";
    MediaCapture _mediacapture;
    auto settings = MediaCaptureInitializationSettings();
    auto filteredGroup = GetFilteredSourceGroupList();

    auto idx = GetSGSelection(filteredGroup);

    auto selectedSrc = filteredGroup.GetAt(idx - 1);
    if (selectedSrc == nullptr)
    {
        std::cout << "Error";
        return -1;
    }
    //std::wcout << L"Selected:" << selectedSrc.DisplayName().c_str();
    settings.SourceGroup(selectedSrc.SourceGroup());

    if ((selectedSrc.MediaStreamType() == MediaStreamType::VideoPreview) || (selectedSrc.MediaStreamType() == MediaStreamType::VideoRecord))
    {
        // if user explicitly selected a non-photo pin to take the photo
        settings.PhotoCaptureSource(PhotoCaptureSource::VideoPreview);
    }
    else
    {
        // we hope that auto will select the best photo pin options
        settings.PhotoCaptureSource(PhotoCaptureSource::Auto);
    }

    settings.StreamingCaptureMode(StreamingCaptureMode::Video);
    _mediacapture.InitializeAsync(settings).get();
    
    //Set format on the medicapture frame source
    auto fmtidx = GetMediaTypeSelection(selectedSrc);
    auto fsIter = _mediacapture.FrameSources().First();
    while (fsIter.HasCurrent())
    {

        auto fs = fsIter.Current().Value();
        if (fs.Info().Id() == selectedSrc.Id())
        {
            fs.SetFormatAsync(fs.SupportedFormats().GetAt(fmtidx)).get();
        }
        fsIter.MoveNext();
    }

    std::cout << "Press p take photos and q to quit";
    char key=0;
    uint32_t photoCounter = 0;
    // Main processing part of photos
    while (key != 'q')
    {
        
        key = _getch();
        std::cout << key;
        switch (key)
        {
            case 'p':
                std::cout << "\nTaking photos: " << ++photoCounter;
                break;

            default:
                continue;
        }

        auto folderRoot = StorageFolder::GetFolderFromPathAsync(L"C:\\").get();
        auto folder = folderRoot.CreateFolderAsync(L"test\\", CreationCollisionOption::OpenIfExists).get();
        auto file1 = folder.CreateFileAsync(to_hstring(photoCounter) + L"_1.png", CreationCollisionOption::ReplaceExisting).get();
        auto file2 = folder.CreateFileAsync(to_hstring(photoCounter) + L"_2.png", CreationCollisionOption::ReplaceExisting).get();

        //Capture and save two photos
        _mediacapture.CapturePhotoToStorageFileAsync(ImageEncodingProperties::CreatePng(), file1).get();
        _mediacapture.CapturePhotoToStorageFileAsync(ImageEncodingProperties::CreatePng(), file2).get();
        
        auto sbList = single_threaded_vector<SoftwareBitmap>();
        // open photos and decode them into NV12 format for processing
        // photo1
        { 
            auto strm = file1.OpenReadAsync().get();
            auto decoder = BitmapDecoder::CreateAsync(strm).get();
            auto sb1 = decoder.GetSoftwareBitmapAsync().get();
            sbList.Append(SoftwareBitmap::Convert(sb1, BitmapPixelFormat::Nv12));
        }
        // photo2
        {
            auto strm = file2.OpenReadAsync().get();
            auto decoder = BitmapDecoder::CreateAsync(strm).get();
            auto sb1 = decoder.GetSoftwareBitmapAsync().get();
            sbList.Append(SoftwareBitmap::Convert(sb1, BitmapPixelFormat::Nv12));
        }

        // Do some processing like low light fusion in this example
        auto op = LowLightFusion::FuseAsync(sbList).get();
        
        // Save the output
        auto fname = to_hstring(photoCounter) + L"_LLF.png";
        auto fileOp = folder.CreateFileAsync(fname, CreationCollisionOption::ReplaceExisting).get();
        {
            auto strm = fileOp.OpenAsync(FileAccessMode::ReadWrite).get();
            auto encoder = BitmapEncoder::CreateAsync(BitmapEncoder::PngEncoderId(), strm).get();
            encoder.SetSoftwareBitmap(SoftwareBitmap::Convert(op.Frame(), BitmapPixelFormat::Bgra8));
            encoder.FlushAsync().get();
        }
    }
}
