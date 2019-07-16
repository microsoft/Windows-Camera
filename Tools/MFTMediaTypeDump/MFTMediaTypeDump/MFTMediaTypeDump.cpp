// MFTMediaTypeDump.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <map>
#include <Windows.h>
#include <windows.foundation.h>
#include <intsafe.h>
#include <strsafe.h>
#include <mfapi.h>
#include <mftransform.h>
#include <wrl\client.h>
#include <Unknwn.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <dxgi1_3.h>
#include <mfidl.h>

using namespace Microsoft::WRL;

//#include <winrt\base.h>
#define CHECKHR_GOTO(hrop, label) \
{\
    hr=hrop;\
    if (FAILED(hr))\
    {\
        std::cout << "Failed Hresult:" << std::hex << hr << std::endl;\
        goto label;\
    }\
}
#define SAFE_COTASKMEMFREE(p)       CoTaskMemFree( p ); p = NULL;


#define MAKE_GUID_NAME_PAIR(val) {val, L#val}

struct GUIDComparer
{
    bool operator()(const GUID & Left, const GUID & Right) const
    {
        // comparison logic goes here
        return memcmp(&Left, &Right, sizeof(Right)) < 0;
    }
};

std::map<GUID, const wchar_t *, GUIDComparer> GUIDName =
{
    MAKE_GUID_NAME_PAIR(MF_MT_MAJOR_TYPE),
    MAKE_GUID_NAME_PAIR(MF_MT_MAJOR_TYPE),
    MAKE_GUID_NAME_PAIR(MF_MT_SUBTYPE),
    MAKE_GUID_NAME_PAIR(MF_MT_ALL_SAMPLES_INDEPENDENT),
    MAKE_GUID_NAME_PAIR(MF_MT_FIXED_SIZE_SAMPLES),
    MAKE_GUID_NAME_PAIR(MF_MT_COMPRESSED),
    MAKE_GUID_NAME_PAIR(MF_MT_SAMPLE_SIZE),
    MAKE_GUID_NAME_PAIR(MF_MT_WRAPPED_TYPE),
    MAKE_GUID_NAME_PAIR(MF_MT_AUDIO_NUM_CHANNELS),
    MAKE_GUID_NAME_PAIR(MF_MT_AUDIO_SAMPLES_PER_SECOND),
    MAKE_GUID_NAME_PAIR(MF_MT_AUDIO_FLOAT_SAMPLES_PER_SECOND),
    MAKE_GUID_NAME_PAIR(MF_MT_AUDIO_AVG_BYTES_PER_SECOND),
    MAKE_GUID_NAME_PAIR(MF_MT_AUDIO_BLOCK_ALIGNMENT),
    MAKE_GUID_NAME_PAIR(MF_MT_AUDIO_BITS_PER_SAMPLE),
    MAKE_GUID_NAME_PAIR(MF_MT_AUDIO_VALID_BITS_PER_SAMPLE),
    MAKE_GUID_NAME_PAIR(MF_MT_AUDIO_SAMPLES_PER_BLOCK),
    MAKE_GUID_NAME_PAIR(MF_MT_AUDIO_CHANNEL_MASK),
    MAKE_GUID_NAME_PAIR(MF_MT_AUDIO_FOLDDOWN_MATRIX),
    MAKE_GUID_NAME_PAIR(MF_MT_AUDIO_WMADRC_PEAKREF),
    MAKE_GUID_NAME_PAIR(MF_MT_AUDIO_WMADRC_PEAKTARGET),
    MAKE_GUID_NAME_PAIR(MF_MT_AUDIO_WMADRC_AVGREF),
    MAKE_GUID_NAME_PAIR(MF_MT_AUDIO_WMADRC_AVGTARGET),
    MAKE_GUID_NAME_PAIR(MF_MT_AUDIO_PREFER_WAVEFORMATEX),
    MAKE_GUID_NAME_PAIR(MF_MT_AAC_PAYLOAD_TYPE),
    MAKE_GUID_NAME_PAIR(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION),
    MAKE_GUID_NAME_PAIR(MF_MT_FRAME_SIZE),
    MAKE_GUID_NAME_PAIR(MF_MT_FRAME_RATE),
    MAKE_GUID_NAME_PAIR(MF_MT_FRAME_RATE_RANGE_MAX),
    MAKE_GUID_NAME_PAIR(MF_MT_FRAME_RATE_RANGE_MIN),
    MAKE_GUID_NAME_PAIR(MF_MT_PIXEL_ASPECT_RATIO),
    MAKE_GUID_NAME_PAIR(MF_MT_DRM_FLAGS),
    MAKE_GUID_NAME_PAIR(MF_MT_PAD_CONTROL_FLAGS),
    MAKE_GUID_NAME_PAIR(MF_MT_SOURCE_CONTENT_HINT),
    MAKE_GUID_NAME_PAIR(MF_MT_VIDEO_CHROMA_SITING),
    MAKE_GUID_NAME_PAIR(MF_MT_INTERLACE_MODE),
    MAKE_GUID_NAME_PAIR(MF_MT_TRANSFER_FUNCTION),
    MAKE_GUID_NAME_PAIR(MF_MT_VIDEO_PRIMARIES),
    MAKE_GUID_NAME_PAIR(MF_MT_CUSTOM_VIDEO_PRIMARIES),
    MAKE_GUID_NAME_PAIR(MF_MT_YUV_MATRIX),
    MAKE_GUID_NAME_PAIR(MF_MT_VIDEO_LIGHTING),
    MAKE_GUID_NAME_PAIR(MF_MT_VIDEO_NOMINAL_RANGE),
    MAKE_GUID_NAME_PAIR(MF_MT_GEOMETRIC_APERTURE),
    MAKE_GUID_NAME_PAIR(MF_MT_MINIMUM_DISPLAY_APERTURE),
    MAKE_GUID_NAME_PAIR(MF_MT_PAN_SCAN_APERTURE),
    MAKE_GUID_NAME_PAIR(MF_MT_PAN_SCAN_ENABLED),
    MAKE_GUID_NAME_PAIR(MF_MT_AVG_BITRATE),
    MAKE_GUID_NAME_PAIR(MF_MT_AVG_BIT_ERROR_RATE),
    MAKE_GUID_NAME_PAIR(MF_MT_MAX_KEYFRAME_SPACING),
    MAKE_GUID_NAME_PAIR(MF_MT_DEFAULT_STRIDE),
    MAKE_GUID_NAME_PAIR(MF_MT_PALETTE),
    MAKE_GUID_NAME_PAIR(MF_MT_USER_DATA),
    MAKE_GUID_NAME_PAIR(MF_MT_AM_FORMAT_TYPE),
    MAKE_GUID_NAME_PAIR(MF_MT_MPEG_START_TIME_CODE),
    MAKE_GUID_NAME_PAIR(MF_MT_MPEG2_PROFILE),
    MAKE_GUID_NAME_PAIR(MF_MT_MPEG2_LEVEL),
    MAKE_GUID_NAME_PAIR(MF_MT_MPEG2_FLAGS),
    MAKE_GUID_NAME_PAIR(MF_MT_MPEG_SEQUENCE_HEADER),
    MAKE_GUID_NAME_PAIR(MF_MT_DV_AAUX_SRC_PACK_0),
    MAKE_GUID_NAME_PAIR(MF_MT_DV_AAUX_CTRL_PACK_0),
    MAKE_GUID_NAME_PAIR(MF_MT_DV_AAUX_SRC_PACK_1),
    MAKE_GUID_NAME_PAIR(MF_MT_DV_AAUX_CTRL_PACK_1),
    MAKE_GUID_NAME_PAIR(MF_MT_DV_VAUX_SRC_PACK),
    MAKE_GUID_NAME_PAIR(MF_MT_DV_VAUX_CTRL_PACK),
    MAKE_GUID_NAME_PAIR(MF_MT_ARBITRARY_HEADER),
    MAKE_GUID_NAME_PAIR(MF_MT_ARBITRARY_FORMAT),
    MAKE_GUID_NAME_PAIR(MF_MT_IMAGE_LOSS_TOLERANT),
    MAKE_GUID_NAME_PAIR(MF_MT_MPEG4_SAMPLE_DESCRIPTION),
    MAKE_GUID_NAME_PAIR(MF_MT_MPEG4_CURRENT_SAMPLE_ENTRY),
    MAKE_GUID_NAME_PAIR(MF_MT_ORIGINAL_4CC),
    MAKE_GUID_NAME_PAIR(MF_MT_ORIGINAL_WAVE_FORMAT_TAG),

    // Media types

    MAKE_GUID_NAME_PAIR(MFMediaType_Audio),
    MAKE_GUID_NAME_PAIR(MFMediaType_Video),
    MAKE_GUID_NAME_PAIR(MFMediaType_Protected),
    MAKE_GUID_NAME_PAIR(MFMediaType_SAMI),
    MAKE_GUID_NAME_PAIR(MFMediaType_Script),
    MAKE_GUID_NAME_PAIR(MFMediaType_Image),
    MAKE_GUID_NAME_PAIR(MFMediaType_HTML),
    MAKE_GUID_NAME_PAIR(MFMediaType_Binary),
    MAKE_GUID_NAME_PAIR(MFMediaType_FileTransfer),

    MAKE_GUID_NAME_PAIR(MFVideoFormat_AI44), //     FCC('AI44')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_ARGB32), //   D3DFMT_A8R8G8B8
    MAKE_GUID_NAME_PAIR(MFVideoFormat_AYUV), //     FCC('AYUV')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_DV25), //     FCC('dv25')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_DV50), //     FCC('dv50')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_DVH1), //     FCC('dvh1')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_DVSD), //     FCC('dvsd')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_DVSL), //     FCC('dvsl')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_H264), //     FCC('H264')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_I420), //     FCC('I420')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_IYUV), //     FCC('IYUV')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_M4S2), //     FCC('M4S2')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_MJPG),
    MAKE_GUID_NAME_PAIR(MFVideoFormat_MP43), //     FCC('MP43')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_MP4S), //     FCC('MP4S')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_MP4V), //     FCC('MP4V')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_MPG1), //     FCC('MPG1')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_MSS1), //     FCC('MSS1')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_MSS2), //     FCC('MSS2')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_NV11), //     FCC('NV11')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_NV12), //     FCC('NV12')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_P010), //     FCC('P010')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_P016), //     FCC('P016')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_P210), //     FCC('P210')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_P216), //     FCC('P216')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_RGB24), //    D3DFMT_R8G8B8
    MAKE_GUID_NAME_PAIR(MFVideoFormat_RGB32), //    D3DFMT_X8R8G8B8
    MAKE_GUID_NAME_PAIR(MFVideoFormat_RGB555), //   D3DFMT_X1R5G5B5
    MAKE_GUID_NAME_PAIR(MFVideoFormat_RGB565), //   D3DFMT_R5G6B5
    MAKE_GUID_NAME_PAIR(MFVideoFormat_RGB8),
    MAKE_GUID_NAME_PAIR(MFVideoFormat_UYVY), //     FCC('UYVY')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_v210), //     FCC('v210')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_v410), //     FCC('v410')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_WMV1), //     FCC('WMV1')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_WMV2), //     FCC('WMV2')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_WMV3), //     FCC('WMV3')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_WVC1), //     FCC('WVC1')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_Y210), //     FCC('Y210')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_Y216), //     FCC('Y216')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_Y410), //     FCC('Y410')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_Y416), //     FCC('Y416')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_Y41P),
    MAKE_GUID_NAME_PAIR(MFVideoFormat_Y41T),
    MAKE_GUID_NAME_PAIR(MFVideoFormat_YUY2), //     FCC('YUY2')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_YV12), //     FCC('YV12')
    MAKE_GUID_NAME_PAIR(MFVideoFormat_YVYU),

    MAKE_GUID_NAME_PAIR(MFAudioFormat_PCM), //              WAVE_FORMAT_PCM
    MAKE_GUID_NAME_PAIR(MFAudioFormat_Float), //            WAVE_FORMAT_IEEE_FLOAT
    MAKE_GUID_NAME_PAIR(MFAudioFormat_DTS), //              WAVE_FORMAT_DTS
    MAKE_GUID_NAME_PAIR(MFAudioFormat_Dolby_AC3_SPDIF), //  WAVE_FORMAT_DOLBY_AC3_SPDIF
    MAKE_GUID_NAME_PAIR(MFAudioFormat_DRM), //              WAVE_FORMAT_DRM
    MAKE_GUID_NAME_PAIR(MFAudioFormat_WMAudioV8), //        WAVE_FORMAT_WMAUDIO2
    MAKE_GUID_NAME_PAIR(MFAudioFormat_WMAudioV9), //        WAVE_FORMAT_WMAUDIO3
    MAKE_GUID_NAME_PAIR(MFAudioFormat_WMAudio_Lossless), // WAVE_FORMAT_WMAUDIO_LOSSLESS
    MAKE_GUID_NAME_PAIR(MFAudioFormat_WMASPDIF), //         WAVE_FORMAT_WMASPDIF
    MAKE_GUID_NAME_PAIR(MFAudioFormat_MSP1), //             WAVE_FORMAT_WMAVOICE9
    MAKE_GUID_NAME_PAIR(MFAudioFormat_MP3), //              WAVE_FORMAT_MPEGLAYER3
    MAKE_GUID_NAME_PAIR(MFAudioFormat_MPEG), //             WAVE_FORMAT_MPEG
    MAKE_GUID_NAME_PAIR(MFAudioFormat_AAC), //              WAVE_FORMAT_MPEG_HEAAC
    MAKE_GUID_NAME_PAIR(MFAudioFormat_ADTS) //             WAVE_FORMAT_MPEG_ADTS_AAC

};

void
FormatMFAttribute(
    _In_ REFGUID guidKey,
    _In_ PROPVARIANT& prop,
    _In_ LPWSTR pwz,
    _In_ ULONG cch
)
{
    HRESULT     hr = S_OK;
    WCHAR       wzValue[MAX_PATH] = { 0 };
    WCHAR       wzGuidName[MAX_PATH] = { 0 };
    const wchar_t *pGuidName;
    const WCHAR* name;
    switch (prop.vt)
    {
    case MF_ATTRIBUTE_UINT32:
        CHECKHR_GOTO(StringCchPrintf(wzValue, ARRAYSIZE(wzValue), L"%u(0x%08X)", prop.ulVal, prop.ulVal), done);
        break;
    case MF_ATTRIBUTE_UINT64:
        CHECKHR_GOTO(StringCchPrintf(wzValue, ARRAYSIZE(wzValue), L"%I64u(0x%I64X)(%u,%u)", prop.uhVal.QuadPart, prop.uhVal.QuadPart, prop.uhVal.LowPart, prop.uhVal.HighPart), done);
        break;
    case MF_ATTRIBUTE_DOUBLE:
        CHECKHR_GOTO(StringCchPrintf(wzValue, ARRAYSIZE(wzValue), L"%f", prop.dblVal), done);
        break;
    case MF_ATTRIBUTE_GUID:
        name = GUIDName[(GUID)*(prop.puuid)];
        if (name != nullptr)
        {
            CHECKHR_GOTO(StringCchPrintf(wzValue, ARRAYSIZE(wzValue), L"%s", name), done);
        }
        else
        {
            CHECKHR_GOTO((StringCchPrintf(wzValue, ARRAYSIZE(wzValue), L"{%08X-%04X-%04X-%02X%02X%02X%02X%02X%02X%02X%02X}",
            prop.puuid->Data1,
                prop.puuid->Data2,
                prop.puuid->Data3,
                prop.puuid->Data4[0],
                prop.puuid->Data4[1],
                prop.puuid->Data4[2],
                prop.puuid->Data4[3],
                prop.puuid->Data4[4],
                prop.puuid->Data4[5],
                prop.puuid->Data4[6],
                prop.puuid->Data4[7])), done);
        }
        break;
    case MF_ATTRIBUTE_STRING:
        CHECKHR_GOTO(StringCchPrintf(wzValue, ARRAYSIZE(wzValue), L"%s", prop.pwszVal), done);
        break;
    case MF_ATTRIBUTE_BLOB:
        CHECKHR_GOTO(StringCchPrintf(wzValue, ARRAYSIZE(wzValue), L"<blob>"), done);
        break;
    default:
        CHECKHR_GOTO(StringCchPrintf(wzValue, ARRAYSIZE(wzValue), L"<unknown>"), done);
        break;
    }

    pGuidName = GUIDName[guidKey];
    if (pGuidName == nullptr)
    {
        StringCchPrintf(pwz, cch, L"{%08X-%04X-%04X-%02X%02X%02X%02X%02X%02X%02X%02X} : value=[%s]", 
            guidKey.Data1,
            guidKey.Data2,
            guidKey.Data3,
            guidKey.Data4[0],
            guidKey.Data4[1],
            guidKey.Data4[2],
            guidKey.Data4[3],
            guidKey.Data4[4],
            guidKey.Data4[5],
            guidKey.Data4[6],
            guidKey.Data4[7], wzValue);
    }
    else
    {
        StringCchPrintf(pwz, cch, L"%s : value=[%s]", pGuidName, wzValue);
    }

done:
    return;
}
void PrintMediaTypeAttributes(ComPtr<IMFMediaType> spOutputMediaType)
{
    HRESULT hrlocal = S_OK;
    ComPtr<IMFAttributes> spOutpuMediaTypeAttrs;
    spOutputMediaType.As(&spOutpuMediaTypeAttrs);
    for (int k = 0; ; k++)
    {
        GUID key;
        PROPVARIANT Value;

        hrlocal = spOutpuMediaTypeAttrs->GetItemByIndex(k, &key, &Value);
        if (FAILED(hrlocal))
        {
            break;
        }
        //std::wstring strVal;
        wchar_t wcsVal[MAX_PATH];
        FormatMFAttribute(key, Value, wcsVal, ARRAYSIZE(wcsVal));
        std::wcout << wcsVal << std::endl;
    }
}
HRESULT EnumerateMediaTypes(UINT32 cMFTActivate, IMFActivate **ppActivates, ComPtr<IMFDXGIDeviceManager> spDXGIDeviceManager)
{
    HRESULT hr = S_OK;
    ComPtr<IMFTransform> spMJPGDecoder;
    HRESULT hrlocal = S_OK;
    for (UINT i = 0; i < cMFTActivate; i++)
    {
        //LPWSTR pszMFTVendorId = nullptr;
        ComPtr<IMFAttributes> spMFTAttr;
        HRESULT hr = ppActivates[i]->ActivateObject(IID_PPV_ARGS(spMJPGDecoder.GetAddressOf()));
        if (SUCCEEDED(hr))
        {
            ComPtr<IMFMediaType> spInputMediaType;
            UINT32 len = 0;
            std::wstring name= L"";
            hrlocal = spMJPGDecoder->GetAttributes(spMFTAttr.ReleaseAndGetAddressOf());
            if (SUCCEEDED(hrlocal))
            {
                spMFTAttr->GetStringLength(MFT_FRIENDLY_NAME_Attribute, &len);
                name.resize(len + 1);
                hrlocal = spMFTAttr->GetString(MFT_FRIENDLY_NAME_Attribute, &name[0], name.size(), &len);
                hrlocal = spMFTAttr->SetUINT32(MF_TRANSFORM_ASYNC_UNLOCK, TRUE);
                if ((spDXGIDeviceManager)
                    && ((MFGetAttributeUINT32(spMFTAttr.Get(), MF_SA_D3D11_AWARE, FALSE) != 0)
                        || (MFGetAttributeUINT32(spMFTAttr.Get(), MF_SA_D3D_AWARE, FALSE) != 0))
                    )
                {
                    //ComPtr<IUnknown> spUnkDXGIManager;
                    //spDXGIDeviceManager.As(&spUnkDXGIManager);
                    CHECKHR_GOTO(spMJPGDecoder->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, (ULONG_PTR)(spDXGIDeviceManager.Get())), localdone);
                }
                //spMFTAttr->Release();
            }
            std::wcout << L"\n-----------------Decoder:" << name.c_str() << L"-----------------" << std::endl;
            DWORD strmInid = 0, strmOutid = 0;

            (void)spMJPGDecoder->GetStreamIDs(1, &strmInid, 1, &strmOutid);

            std::cout << "Setting input mediatype" << std::endl;
            int idx = 0;
            //do
            {
                CHECKHR_GOTO(spMJPGDecoder->GetInputAvailableType(strmInid, idx, spInputMediaType.ReleaseAndGetAddressOf()), localdone);
                
                idx++;
                if (spInputMediaType)
                {
                    CHECKHR_GOTO(MFSetAttributeSize(spInputMediaType.Get(), MF_MT_FRAME_SIZE, 1280, 720), localdone);
                    CHECKHR_GOTO(MFSetAttributeRatio(spInputMediaType.Get(), MF_MT_FRAME_RATE, 30, 1), localdone);
                    std::cout << "\nInput MediaType:\n";
                    PrintMediaTypeAttributes(spInputMediaType);
                }
            } //while(FAILED(spMJPGDecoder->SetInputType(strmInid, spInputMediaType.Get(), 0)));
            CHECKHR_GOTO(spMJPGDecoder->SetInputType(strmInid, spInputMediaType.Get(), 0); , localdone);
            
            for (int j = 0; ; j++)
            {
                ComPtr<IMFMediaType> spOutputMediaType;
                
                hrlocal = spMJPGDecoder->GetOutputAvailableType(strmOutid, j, spOutputMediaType.ReleaseAndGetAddressOf());
                if (FAILED(hrlocal))
                {
                    std::cout << "\nNo more mediatypes\n";
                    break;
                }
                /*hrlocal = spMJPGDecoder->SetOutputType(strmOutid, spOutputMediaType.Get(), 0);
                if (FAILED(hrlocal))
                {
                    std::cout << "\nSetOutputType failed \n";
                    continue;
                }
                hrlocal = spMJPGDecoder->GetOutputCurrentType(strmOutid, spOutputMediaType.ReleaseAndGetAddressOf());
                if (FAILED(hrlocal))
                {
                    std::cout << "\nGetOutputCurrentType failed \n";
                    continue;
                }*/
                std::cout << "\nOutput MediaType:" << j << std::endl;
                PrintMediaTypeAttributes(spOutputMediaType);
            }
        }
        else
        {

        }
    localdone:
        continue;
    }
done:
    return hr;
}

int main()
{
    HRESULT hr = S_OK;
    MFT_REGISTER_TYPE_INFO inputType = { 0 };
    MFT_REGISTER_TYPE_INFO outputType = { 0 };
    IMFActivate **ppActivates = NULL;   // Pointer to an array of CLISDs. 
    UINT32 cMFTActivate = NULL;   // Size of the array.
    
    RoInitialize(RO_INIT_TYPE::RO_INIT_MULTITHREADED);
    inputType.guidMajorType = MFMediaType_Video;
    inputType.guidSubtype = MFVideoFormat_MJPG;
    outputType.guidMajorType = MFMediaType_Video;
    outputType.guidSubtype = MFVideoFormat_NV12;
    UINT resetToken;
    ComPtr<IMFDXGIDeviceManager> spDXGIDeviceManager;
    ComPtr<IDXGIFactory2> spDXGIFactory2;
    ComPtr<IDXGIAdapter> spAdapter;
    DXGI_ADAPTER_DESC AdapterDesc;
    UINT32 uIndex = 0;
    const D3D_FEATURE_LEVEL FeatureLevels[] = {
        /*D3D_FEATURE_LEVEL_12_1
       ,D3D_FEATURE_LEVEL_12_0
       ,*/D3D_FEATURE_LEVEL_11_1
       , D3D_FEATURE_LEVEL_11_0
       , D3D_FEATURE_LEVEL_10_1
       , D3D_FEATURE_LEVEL_10_0
       , D3D_FEATURE_LEVEL_9_3
       , D3D_FEATURE_LEVEL_9_2
       , D3D_FEATURE_LEVEL_9_1
    };
    ComPtr<ID3D10Multithread> spMultithread;
    ComPtr<ID3D11Device> spDevice;
    ComPtr<IMFAttributes> spAttribs;
    D3D_FEATURE_LEVEL FeatureLevel;
    UINT flags;
    D3D_DRIVER_TYPE DriverTypes[] =
    {
        D3D_DRIVER_TYPE_HARDWARE,
        D3D_DRIVER_TYPE_WARP,
        D3D_DRIVER_TYPE_REFERENCE,
        D3D_DRIVER_TYPE_SOFTWARE,
        D3D_DRIVER_TYPE_NULL,
        D3D_DRIVER_TYPE_UNKNOWN
    };
    HRESULT hrlocal = S_OK;
    CHECKHR_GOTO(CreateDXGIFactory2(0, IID_PPV_ARGS(&spDXGIFactory2)), done);
    while (SUCCEEDED(spDXGIFactory2->EnumAdapters(uIndex, spAdapter.ReleaseAndGetAddressOf())))
    {
        uIndex++;
        CHECKHR_GOTO(spAdapter->GetDesc(&AdapterDesc), done);
        std::wcout << L"Adapter:" << AdapterDesc.Description;
        //for (int a = 0; a < _countof(DriverTypes); a++)
        {
            if (SUCCEEDED(D3D11CreateDevice(spAdapter.Get(),
                (spAdapter == nullptr ? (D3D_DRIVER_TYPE) D3D_DRIVER_TYPE_HARDWARE : D3D_DRIVER_TYPE_UNKNOWN),
                nullptr,
                /*D3D11_CREATE_DEVICE_VIDEO_SUPPORT,*/0,
                FeatureLevels, _countof(FeatureLevels),
                D3D11_SDK_VERSION,
                spDevice.ReleaseAndGetAddressOf(),
                &FeatureLevel,
                nullptr)))
            {
                std::wcout << L"\tFeatureLevel:" << std::hex << FeatureLevel;

                CHECKHR_GOTO(spDevice->QueryInterface(IID_PPV_ARGS(spMultithread.ReleaseAndGetAddressOf())), done);
                (void)spMultithread->SetMultithreadProtected(TRUE);
                break;
            }
        }
        if (!spDevice)
        {
            std::wcout << L"\tNo Devices";
            continue;
        }
        if (spDXGIDeviceManager)
        {
            spDXGIDeviceManager->Release();
        }
        
        MFCreateDXGIDeviceManager(&resetToken, spDXGIDeviceManager.ReleaseAndGetAddressOf());
        spDXGIDeviceManager->ResetDevice(spDevice.Get(), resetToken);

        CHECKHR_GOTO(MFCreateAttributes(spAttribs.ReleaseAndGetAddressOf(), 1), done);
        CHECKHR_GOTO(spAttribs->SetBlob(MFT_ENUM_ADAPTER_LUID, (byte*)&(AdapterDesc.AdapterLuid), sizeof(AdapterDesc.AdapterLuid)), done);
        flags = MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_ASYNCMFT | MFT_ENUM_FLAG_SORTANDFILTER | MFT_ENUM_FLAG_HARDWARE;

        CHECKHR_GOTO(MFTEnum2(MFT_CATEGORY_VIDEO_DECODER,
            flags,
            &inputType,
            &outputType,
            spAttribs.Get(),
            &ppActivates, // Receives a pointer to an array of CLSIDs.
            &cMFTActivate  // Receives the size of the array.
        ), done);
        CHECKHR_GOTO(EnumerateMediaTypes(cMFTActivate, ppActivates, spDXGIDeviceManager), done);
    }
    for (UINT32 i = 0; i < cMFTActivate; i++)
    {
        if (nullptr != ppActivates[i])
        {
            ppActivates[i]->Release();
        }
    }

    flags = 0;
    CHECKHR_GOTO(MFTEnum2(MFT_CATEGORY_VIDEO_DECODER,
        flags,
        &inputType,
        &outputType,
        NULL,//spAttribs.Get(),
        &ppActivates, // Receives a pointer to an array of CLSIDs.
        &cMFTActivate  // Receives the size of the array.
    ), done);
    CHECKHR_GOTO(EnumerateMediaTypes(cMFTActivate, ppActivates, nullptr), done);
done:
    for (UINT32 i = 0; i < cMFTActivate; i++)
    {
        if (nullptr != ppActivates[i])
        {
            ppActivates[i]->Release();
        }
    }
    SAFE_COTASKMEMFREE(ppActivates);

}
