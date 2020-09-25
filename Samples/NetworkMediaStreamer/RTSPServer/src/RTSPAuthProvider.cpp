// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for more information

#include <pch.h>

using namespace winrt::Windows::Security;
using namespace winrt::Windows::Security::Credentials;
using namespace Cryptography::Core;
using namespace Cryptography;

class CAuthProvider : public winrt::implements<CAuthProvider, IRTSPAuthProvider, IRTSPAuthProviderCredStore>
{
public:

    STDMETHODIMP GetNewAuthSessionMessage(HSTRING* pAuthSessionMessage) override try
    {
        winrt::check_pointer(pAuthSessionMessage);
        std::string authString;
        if ((m_authType == AuthType::Both) || (m_authType == AuthType::Basic))
        {
            authString = "WWW-Authenticate: Basic realm=\"BeyondTheWall\", charset=\"UTF-8\"\r\n";
        }
        auto buf = Cryptography::CryptographicBuffer::GenerateRandom(24);
        *((uint64_t*)(buf.data() + 16)) = MFGetSystemTime();
        auto nonce = Cryptography::CryptographicBuffer::EncodeToHexString(buf);
        if ((m_authType == AuthType::Both) || (m_authType == AuthType::Digest))
        {
            authString += "WWW-Authenticate: Digest realm=\"BeyondTheWall\", nonce=\"" + winrt::to_string(nonce) + "\", algorithm=SHA-256, charset=\"UTF-8\", stale=FALSE\r\n";
            authString += "WWW-Authenticate: Digest realm=\"BeyondTheWall\", nonce=\"" + winrt::to_string(nonce) + "\", algorithm=MD5, charset=\"UTF-8\", stale=FALSE\r\n";
        }

        *pAuthSessionMessage = reinterpret_cast<HSTRING> (winrt::detach_abi(winrt::to_hstring(authString)));
        return S_OK;
    }HRESULT_EXCEPTION_BOUNDARY_FUNC

    STDMETHODIMP Authorize(LPCWSTR authResp, LPCWSTR authSesMsg, LPCWSTR mthd) override try
    {
        winrt::check_pointer(authResp);
        winrt::check_pointer(authSesMsg);
        winrt::check_pointer(mthd);
        bool result = true;
        auto authResponse = winrt::to_string(authResp);
        auto authSessionMessage = winrt::to_string(authSesMsg);
        auto method = winrt::to_string(mthd);
        std::vector<std::string> paramsResponce = { "Authorization: ","realm=","nonce=", "username=", "response=", "algorithm=", "uri=" };
        std::vector<std::string> paramsSent = { "realm=","nonce=" };
        std::map<std::string, std::string> paramMapResponse, paramMap;
        for (auto param : paramsResponce)
        {
            paramMapResponse[param] = GetValueForParam(authResponse, param);
        }
        for (auto param : paramsSent)
        {
            paramMap[param] = GetValueForParam(authSessionMessage, param);
            if (!((paramMapResponse[param].empty()) || (paramMap[param].empty())))
            {
                result &= (paramMapResponse[param] == paramMap[param]);
            }
        }

        if (paramMapResponse["Authorization: "] == "Digest")
        {
            CryptographicHash hash(nullptr);
            if ((paramMapResponse["algorithm="] == "MD5") || (paramMapResponse["response="].size() < 64))
            {
                hash = m_hashMD5;
            }
            else
            {
                hash = m_hashSHA256;
            }
            auto username = paramMapResponse["username="];
            std::string password;
            auto vault = PasswordVault();

            PasswordCredential cred = nullptr;
            if (!username.empty())
            {
                try
                {
                    cred = vault.Retrieve(m_resourceName, winrt::to_hstring(username));
                }
                catch (winrt::hresult_error const& ex) // not-so-elegant hack- vault.Retrieve throws if user is not found.
                {
                    (ex);
                    cred = nullptr;
                }
            }
            if (cred)
            {
                cred.RetrievePassword();
                password = winrt::to_string(cred.Password());
                auto A1 = username + ":" + paramMap["realm="] + ":" + password;
                auto bufA1 = CryptographicBuffer::ConvertStringToBinary(winrt::to_hstring(A1), BinaryStringEncoding::Utf8);
                hash.Append(bufA1);
                auto HA1 = hash.GetValueAndReset();
                auto A2 = method + ":" + paramMapResponse["uri="];
                auto bufA2 = CryptographicBuffer::ConvertStringToBinary(winrt::to_hstring(A2), BinaryStringEncoding::Utf8);
                hash.Append(bufA2);
                auto HA2 = hash.GetValueAndReset();
                auto nonce = paramMap["nonce="];
                auto strKD = CryptographicBuffer::EncodeToHexString(HA1) + L":" + winrt::to_hstring(nonce) + L":" + CryptographicBuffer::EncodeToHexString(HA2);
                auto bufKD = CryptographicBuffer::ConvertStringToBinary(strKD, BinaryStringEncoding::Utf8);
                hash.Append(bufKD);

                auto response = hash.GetValueAndReset();
                paramMap["response="] = winrt::to_string(CryptographicBuffer::EncodeToHexString(response));
                result &= (paramMapResponse["response="] == paramMap["response="]);
            }
            else
            {
                result = false;
            }
        }
        else //Basic
        {
            auto idx = authResponse.find("Basic");
            if (idx != std::string::npos)
            {
                idx += 6;
                paramMapResponse["response="] = authResponse.substr(idx, authResponse.find_first_of(" \n\r\t,", idx) - idx);
            }
            auto received = CryptographicBuffer::DecodeFromBase64String(winrt::to_hstring(paramMapResponse["response="]));
            auto strReceveid = CryptographicBuffer::ConvertBinaryToString(BinaryStringEncoding::Utf8, received);
            auto strR = winrt::to_string(strReceveid);
            idx = strR.rfind(":");
            std::string username, password;
            if (idx != std::string::npos)
            {
                idx++;
                password = strR.substr(idx);
            }
            idx = strR.find(":");
            if (idx != std::string::npos)
            {
                username = strR.substr(0, idx);
            }
            auto vault = PasswordVault();
            PasswordCredential cred = nullptr;
            if (!username.empty())
            {
                try
                {
                    cred = vault.Retrieve(m_resourceName, winrt::to_hstring(username));
                }
                catch (winrt::hresult_error const& ex) // not-so-elegant hack- vault.Retrieve throws if user is not found.
                {
                    (ex);
                    cred = nullptr;
                }
            }
            bool match = false;
            if (cred)
            {
                cred.RetrievePassword();
                match = (cred.Password() == winrt::to_hstring(password));
            }
            result &= match;
        }

        if (!result)
        {
            winrt::check_win32(ERROR_INVALID_PASSWORD);
        }
        return S_OK;
    }HRESULT_EXCEPTION_BOUNDARY_FUNC

    STDMETHODIMP AddUser(LPCWSTR userName, LPCWSTR password) override try
    {
        winrt::check_pointer(userName);
        winrt::check_pointer(password);
        auto vault = PasswordVault();
        auto cr = PasswordCredential(m_resourceName, userName, winrt::hstring(password));
        vault.Add(cr);
        return S_OK;
    }HRESULT_EXCEPTION_BOUNDARY_FUNC

    STDMETHODIMP RemoveUser(LPCWSTR userName) override try
    {
        winrt::check_pointer(userName);
        auto vault = PasswordVault();
        auto creds = vault.FindAllByUserName(userName);
        for (auto cred : creds)
        {
            if (cred.Resource() == m_resourceName)
            {
                vault.Remove(cred);
                break;
            }
        }
        return S_OK;
    }HRESULT_EXCEPTION_BOUNDARY_FUNC

    static IRTSPAuthProvider* CreateInstance(AuthType authtype, winrt::hstring resourceName)
    {
        return new CAuthProvider(authtype, resourceName);
    }

private:

    CAuthProvider(AuthType authType, winrt::hstring resourceName)
        : m_authType(authType)
        , m_hashMD5(nullptr)
        , m_hashSHA256(nullptr)
        , m_resourceName(resourceName)
    {
        auto hashProvider = HashAlgorithmProvider::OpenAlgorithm(HashAlgorithmNames::Md5());
        m_hashMD5 = hashProvider.CreateHash();
        hashProvider = HashAlgorithmProvider::OpenAlgorithm(HashAlgorithmNames::Sha256());
        m_hashSHA256 = hashProvider.CreateHash();
    }

    virtual  ~CAuthProvider() = default;

    std::string GetValueForParam(std::string message, std::string param)
    {
        auto idx = message.find(param);
        std::string val;
        if (idx != std::string::npos)
        {
            idx += param.size();
            if (message[idx] == '\"') idx++;
            auto endIdx = message.find_first_of("\" \r\n,", idx);
            val = message.substr(idx, endIdx - idx);
        }
        return val;

    }

    winrt::hstring m_resourceName;
    AuthType m_authType;
    CryptographicHash m_hashMD5, m_hashSHA256;
};

RTSPSERVER_API STDMETHODIMP GetAuthProviderInstance(AuthType authType, LPCWSTR resourceName, IRTSPAuthProvider** ppRTSPAuthProvider) try
{
    winrt::check_pointer(ppRTSPAuthProvider);
    *ppRTSPAuthProvider = CAuthProvider::CreateInstance(authType, resourceName);
    return S_OK;
}HRESULT_EXCEPTION_BOUNDARY_FUNC
