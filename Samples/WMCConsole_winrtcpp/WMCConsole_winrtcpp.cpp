// 
// WMCConsole_winrtcpp.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <Windows.h>
#include <iostream>
#include <conio.h>
#include <Mferror.h>
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
IVectorView<MediaFrameSourceGroup> sourceGroups;
// Iterates through source groups and filter-out the frame sources which have IR,depth and other sources which we cannot consume in this app
IVector<MediaFrameSourceInfo> GetFilteredSourceGroupList()
{
    auto filteredSourceInfos = single_threaded_vector<MediaFrameSourceInfo>();
    sourceGroups = MediaFrameSourceGroup::FindAllAsync().get();
    auto sourceGroupIter = sourceGroups.First();
    while (sourceGroupIter.HasCurrent())
    {
        std::wcout << sourceGroupIter.Current().DisplayName().c_str();
        auto sourceInfos = sourceGroupIter.Current().SourceInfos();
        auto sourceInfoIter = sourceInfos.First();
        // iterate through sources and filter-out the IR,depth and other sources which we cannot consume in this app
        while (sourceInfoIter.HasCurrent())
        {
            if ((sourceInfoIter.Current().MediaStreamType() == MediaStreamType::Photo
                || sourceInfoIter.Current().MediaStreamType() == MediaStreamType::VideoPreview
                || sourceInfoIter.Current().MediaStreamType() == MediaStreamType::VideoRecord)
                && sourceInfoIter.Current().SourceKind() == MediaFrameSourceKind::Color)
            {
                filteredSourceInfos.Append(sourceInfoIter.Current());
            }
            sourceInfoIter.MoveNext();
        }
        sourceGroupIter.MoveNext();
    }
    return filteredSourceInfos;
}

int GetSGSelection(IVector<MediaFrameSourceInfo> filteredGroup)
{
    unsigned int idx = 0;
    auto group = filteredGroup.First();
    while (group.HasCurrent())
    {
        idx++;
        auto currGroup = group.Current();
        if (currGroup.SourceGroup() == nullptr)
        {
            std::cout << "nullptr";
        }
        // These are in the same order as the enum MediaStreamType
        // TODO: better solution is to create a Pair type lookup which is future proof
        std::wstring streamTypes[] = 
        { 
            L"VideoPreview", 
            L"VideoRecord", 
            L"Audio", 
            L"Photo" 
        };

        // These are in the same order as the enum Panel
        std::wstring panelTypes[] = 
        {
            L"Unknown",
            L"Front",
            L"Back",
            L"Top",
            L"Bottom",
            L"Left",
            L"Right"
        };
        auto enclosureLocation = currGroup.DeviceInformation().EnclosureLocation();
        std::wstring panelLocation;
        if (enclosureLocation)
        {
            panelLocation = panelTypes[(int)enclosureLocation.Panel()];
        }
        
        std::wcout << idx << L":" << currGroup.DeviceInformation().Name().c_str() << ":" << streamTypes[(int)currGroup.MediaStreamType()].c_str() << panelLocation.c_str() << std::endl;
        group.MoveNext();
    }
    do
    {
        std::wcout << L"Enter selection:";
        std::cin >> idx;
    } while ((idx > filteredGroup.Size()) || (idx < 1));
    return idx-1;
}

int GetMediaTypeSelection(MediaFrameSourceInfo selectedSource)
{
    auto mediaDescriptionIter = selectedSource.VideoProfileMediaDescription().First();
    int idx = 0;
    while (mediaDescriptionIter.HasCurrent())
    {

        auto format = mediaDescriptionIter.Current();
        int frameRateInt = (int)format.FrameRate();
        int frameRateDec = ((int)(format.FrameRate() * 100) % 100);
        idx++;
        auto formatStr = std::to_wstring(idx) + L". " + std::wstring(format.Subtype().c_str()) + L": " +
            std::to_wstring(format.Width()) + L"x" + std::to_wstring(format.Height()) + L" @ " + std::to_wstring(frameRateInt) + L"." + std::to_wstring(frameRateDec) + L" fps";
        std::wcout << formatStr << std::endl;
        mediaDescriptionIter.MoveNext();
    }
    int selection;
    do
    {
        std::wcout << L"Enter selection:";
        std::cin >> selection;
    } while ((selection > idx) || (selection < 1));
    return selection-1; // because selection idx is from 1 to N and indexes for from 0 to N-1
}

void TakePhotosAndProcess(MediaCapture mediaCapture, uint32_t photoIndex)
{
    auto folderRoot = StorageFolder::GetFolderFromPathAsync(L"C:\\").get();
    auto folder = folderRoot.CreateFolderAsync(L"test\\", CreationCollisionOption::OpenIfExists).get();
    auto file1 = folder.CreateFileAsync(to_hstring(photoIndex) + L"_1.png", CreationCollisionOption::ReplaceExisting).get();
    auto file2 = folder.CreateFileAsync(to_hstring(photoIndex) + L"_2.png", CreationCollisionOption::ReplaceExisting).get();

    //Capture and save two photos
    mediaCapture.CapturePhotoToStorageFileAsync(ImageEncodingProperties::CreatePng(), file1).get();
    mediaCapture.CapturePhotoToStorageFileAsync(ImageEncodingProperties::CreatePng(), file2).get();

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
    auto fname = to_hstring(photoIndex) + L"_LLF.png";
    auto fileOp = folder.CreateFileAsync(fname, CreationCollisionOption::ReplaceExisting).get();
    {
        auto strm = fileOp.OpenAsync(FileAccessMode::ReadWrite).get();
        auto encoder = BitmapEncoder::CreateAsync(BitmapEncoder::PngEncoderId(), strm).get();
        encoder.SetSoftwareBitmap(SoftwareBitmap::Convert(op.Frame(), BitmapPixelFormat::Bgra8));
        encoder.FlushAsync().get();
    }
}

MediaCapture InitCamera()
{
    MediaCapture mediaCapture;
    auto settings = MediaCaptureInitializationSettings();
    auto filteredGroups = GetFilteredSourceGroupList();

    if (filteredGroups.Size() < 1)
    {
        std::cout << "Error No suitable capture sources found";
        throw_hresult(MF_E_NO_CAPTURE_DEVICES_AVAILABLE);
    }

    auto selectedSGidx = GetSGSelection(filteredGroups);

    auto selectedSrc = filteredGroups.GetAt(selectedSGidx);

    if (selectedSrc == nullptr)
    {
        throw_hresult(MF_E_OUT_OF_RANGE);
    }

    settings.SourceGroup(selectedSrc.SourceGroup());

    // if user explicitly selected a non-photo pin to take the photo
    if (selectedSrc.MediaStreamType() != MediaStreamType::Photo)
    {
        settings.PhotoCaptureSource(PhotoCaptureSource::VideoPreview);
    }
    else
    {
        // we hope that auto will select the best photo pin options
        settings.PhotoCaptureSource(PhotoCaptureSource::Auto);
    }

    settings.StreamingCaptureMode(StreamingCaptureMode::Video);
    mediaCapture.InitializeAsync(settings).get();

    //Set format on the medicapture frame source
    auto formatIdx = GetMediaTypeSelection(selectedSrc);
    auto frameSourceIter = mediaCapture.FrameSources().First();
    while (frameSourceIter.HasCurrent())
    {

        auto frameSource = frameSourceIter.Current().Value();
        if (frameSource.Info().Id() == selectedSrc.Id())
        {
            frameSource.SetFormatAsync(frameSource.SupportedFormats().GetAt(formatIdx)).get();
        }
        frameSourceIter.MoveNext();
    }
    return mediaCapture;
}

int wmain()
{
    std::cout << "Windows Media Capture in Win32 desktop Console app!";
    auto mediaCapture = InitCamera();

    // Main processing part of photos
    std::cout << "Press 'p' take photos and 'q' to quit";
    char key = 0;
    uint32_t photoCounter = 0;
    while(key != 'q')
    {
        key = _getch();
        switch (key)
        {
            case 'p':
                std::cout << "\nTaking photos and processing: " << ++photoCounter;
                TakePhotosAndProcess(mediaCapture, photoCounter);
                break;

            default:
                continue;
        }
    }
}
