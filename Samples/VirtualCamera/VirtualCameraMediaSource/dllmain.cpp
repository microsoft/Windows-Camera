//
// Copyright (C) Microsoft Corporation. All rights reserved.
//
// 
// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "SimpleMediaSourceActivate.h"

HINSTANCE   g_hInst;

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        g_hInst = hModule;
    }
    return TRUE;
}

bool __stdcall winrt_can_unload_now() noexcept
{
    if (winrt::get_module_lock())
    {
        return false;
    }

    winrt::clear_factory_cache();
    return true;
}

int32_t __stdcall WINRT_CanUnloadNow() noexcept
{
#ifdef _WRL_MODULE_H_
    if (!::Microsoft::WRL::Module<::Microsoft::WRL::InProc>::GetModule().Terminate())
    {
        return 1;
    }
#endif

    return winrt_can_unload_now() ? 0 : 1;
}


HRESULT __stdcall DllGetClassObject(GUID const& clsid, GUID const& iid, void** result)
{
    try
    {
        *result = nullptr;

        if (clsid == __uuidof(winrt::WindowsSample::implementation::SimpleMediaSourceActivate))
        {
            return winrt::make<SimpleMediaSourceActivateFactory>()->QueryInterface(iid, result);
        }

#ifdef _WRL_MODULE_H_
        return ::Microsoft::WRL::Module<::Microsoft::WRL::InProc>::GetModule().GetClassObject(clsid, iid, result);
#else
        return winrt::hresult_class_not_available().to_abi();
#endif
    }
    catch (...)
    {
        return winrt::to_hresult();
    }
}

HRESULT CreateObjectKeyName(
    _In_ REFGUID guid,
    _Out_writes_to_(cch, *pcch) PWSTR pwz,
    _In_ size_t cch,
    _Out_ size_t* pcch
)
{
    wil::unique_cotaskmem_string    wzClsid;
    size_t                          cchRemaining = 0;

    RETURN_HR_IF_NULL(E_INVALIDARG, pwz);
    RETURN_HR_IF(E_INVALIDARG, (cch <= 0));
    RETURN_HR_IF_NULL(E_POINTER, pcch);
    *pcch = 0;

    RETURN_IF_FAILED(StringFromCLSID(guid, &wzClsid));
    RETURN_IF_FAILED(StringCchPrintfEx(pwz,
        cch,
        nullptr,
        &cchRemaining,
        STRSAFE_NO_TRUNCATION,
        L"Software\\Classes\\CLSID\\%ls",
        wzClsid.get()));
    *pcch = (cch - cchRemaining);

    return S_OK;
}

HRESULT CreateRegKey(
    _In_ HKEY hKey,
    _In_z_ PCWSTR pszSubKeyName,
    _Out_ PHKEY phkResult
)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, hKey);
    RETURN_HR_IF_NULL(E_INVALIDARG, pszSubKeyName);
    RETURN_HR_IF_NULL(E_POINTER, phkResult);
    *phkResult = nullptr;

    wil::unique_hkey hSubkey;
    RETURN_IF_WIN32_ERROR(RegCreateKeyEx(hKey,
        pszSubKeyName,
        0,
        nullptr,
        REG_OPTION_NON_VOLATILE,
        KEY_ALL_ACCESS,
        nullptr,
        &hSubkey,
        nullptr));

    *phkResult = hSubkey.release();

    return S_OK;
}

/* 
    Add and set subkey string value 
*/
HRESULT SetRegKeyStringValue(
    _In_ HKEY hKey,
    _In_z_ PCWSTR pszSubKeyName,
    _In_z_ PCWSTR pwzData)
{
    size_t cch = 0;

    RETURN_HR_IF_NULL(E_INVALIDARG, pwzData);
    RETURN_IF_FAILED(StringCchLength(pwzData, STRSAFE_MAX_CCH, &cch));
    cch += 1;

    RETURN_IF_WIN32_ERROR(RegSetValueEx(
        hKey,
        pszSubKeyName,
        0,
        REG_SZ,
        (const BYTE*)pwzData,
        cch * sizeof(WCHAR)));

    return S_OK;
}

std::wstring get_module_path()
{
    std::wstring path(100, L'?');
    uint32_t path_size{};
    DWORD actual_size{};

    HINSTANCE hMod = GetModuleHandle(L"VirtualCameraWin32.dll");

    do
    {
        path_size = static_cast<uint32_t>(path.size());
        actual_size = ::GetModuleFileName(hMod, path.data(), path_size);

        if (actual_size + 1 > path_size)
        {
            path.resize(path_size * 2, L'?');
        }
    } while (actual_size + 1 > path_size);

    path.resize(actual_size);

    return path;
}


STDAPI DllRegisterServer(void)
{
    wil::unique_hkey hKey = NULL;
    
    WCHAR wzBuffer[MAX_PATH] = {};
    size_t cch = 0;

    // Create the name of the key from the object's CLSID
    RETURN_IF_FAILED(CreateObjectKeyName(CLSID_VirtualCameraMediaSource, wzBuffer, MAX_PATH, &cch));

    // Create HKEY_LOCAL_MACHINE\SOFTWARE\Classes\CLSID\{???}
    RETURN_IF_FAILED(CreateRegKey(HKEY_LOCAL_MACHINE, wzBuffer, &hKey));
    RETURN_IF_FAILED(SetRegKeyStringValue(hKey.get(), nullptr, L"VirtualCameraWin32"));

    // Create InprocServer32 key
    wil::unique_hkey hInProcSever32 = NULL;
    RETURN_IF_FAILED(CreateRegKey(hKey.get(), L"InProcServer32", &hInProcSever32));

    // set InprocServer32\default = <dll path>
    std::wstring path{ get_module_path() };
    RETURN_IF_FAILED(SetRegKeyStringValue(hInProcSever32.get(), nullptr, path.c_str()));

    // Add and set InprocServer32\ThreadingModel = <threading model>
    RETURN_IF_FAILED(SetRegKeyStringValue(hInProcSever32.get(), L"ThreadingModel", L"Both"));

    return S_OK;
}

STDAPI DllUnregisterServer(void)
{
    WCHAR wzBuffer[MAX_PATH] = {};
    size_t cch = 0;

    RETURN_IF_FAILED(CreateObjectKeyName(CLSID_VirtualCameraMediaSource, wzBuffer, MAX_PATH, &cch));

    // Delete the key recursively.
    RETURN_IF_WIN32_ERROR(RegDeleteTree(HKEY_LOCAL_MACHINE, wzBuffer));

    return S_OK;
}

