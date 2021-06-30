//
// Copyright (C) Microsoft Corporation. All rights reserved.
//
#include "pch.h"
#include "DataDrivenTestBase.h"

LPCWSTR TAG_TABLE = L"Table";
LPCWSTR TAG_ROW = L"Row";
LPCWSTR TAG_PARAMETER = L"Parameter";

HRESULT DataDrivenTestBase::LoadDataFromXml(LPCWSTR pwszXmlFile, LPCWSTR pwszTable, std::map<std::wstring, testrowData_t>& data)
{
    std::wstring strCurrDir = GetModuleDirectory();
    strCurrDir += L"\\";
    strCurrDir += pwszXmlFile;

    wil::com_ptr_nothrow<IXmlReader> spXmlReader;

    RETURN_IF_FAILED(LoadXml(strCurrDir.c_str(), &spXmlReader));

    bool bTablefound = false;
    RETURN_IF_FAILED(FindTable(pwszTable, spXmlReader.get(), bTablefound));

    if (bTablefound)
    {
        RETURN_IF_FAILED(ReadRows(spXmlReader.get(), data));
    }
    return S_OK;
}

HRESULT DataDrivenTestBase::LoadXml(LPCWSTR pwszXMLFile, IXmlReader** ppXMLReader)
{
    RETURN_HR_IF_NULL(E_POINTER, ppXMLReader);
    *ppXMLReader = nullptr;

    wil::com_ptr_nothrow<IStream> spStream;
    RETURN_IF_FAILED_MSG(SHCreateStreamOnFileW(pwszXMLFile, STGM_READ, &spStream), "Fail to open %s", pwszXMLFile);

    wil::com_ptr_nothrow<IXmlReader> spXmlReader;
    RETURN_IF_FAILED_MSG(CreateXmlReader(IID_PPV_ARGS(&spXmlReader), NULL), "Fail to create xml reader");

    RETURN_IF_FAILED(spXmlReader->SetInput(spStream.get()));

    *ppXMLReader = spXmlReader.detach();
    return S_OK;
}

HRESULT DataDrivenTestBase::FindTable(LPCWSTR pwszTableId, IXmlReader* pXmlReader, bool& bTablefound)
{
    bTablefound = false;
    RETURN_HR_IF_NULL(E_INVALIDARG, pwszTableId);
    RETURN_HR_IF_NULL(E_INVALIDARG, pXmlReader);

    while (true)
    {
        XmlNodeType  nodeType;
        RETURN_IF_FAILED(pXmlReader->Read(&nodeType));

        if (nodeType == XmlNodeType_Element)
        {
            LPCWSTR pwszLocalName = NULL;
            RETURN_IF_FAILED(pXmlReader->GetLocalName(&pwszLocalName, NULL));

            // define element
            if (_wcsicmp(pwszLocalName, TAG_TABLE) == 0)
            {
                LPCWSTR pwszValue = GetAttributeValueByName(L"Id", pXmlReader);
                if (_wcsicmp(pwszValue, pwszTableId) == 0)
                {
                    bTablefound = true;
                    return S_OK;
                }
            }
        }
    }

    return S_OK;
}


HRESULT DataDrivenTestBase::ReadRows(IXmlReader* pXmlReader, std::map<std::wstring, testrowData_t>& rowdata)
{
    uint32_t rowIdx = 0;
    RETURN_HR_IF_NULL(E_INVALIDARG, pXmlReader);

    bool bContinue = true;
    while (bContinue)
    {
        XmlNodeType  nodeType;
        RETURN_IF_FAILED(pXmlReader->Read(&nodeType));

        switch (nodeType)
        {
        case XmlNodeType_Element:
        {
            LPCWSTR pwszLocalName = NULL;
            RETURN_IF_FAILED(pXmlReader->GetLocalName(&pwszLocalName, NULL));

            // Read Row element
            if (_wcsicmp(pwszLocalName, L"Row") == 0)
            {
                LPCWSTR pwszRowName = GetAttributeValueByName(L"Name", pXmlReader);

                testrowData_t parameters;
                RETURN_IF_FAILED(ReadParameters(pXmlReader, parameters));
                if (pwszRowName)
                {
                    rowdata.insert(std::pair<std::wstring, testrowData_t>(std::wstring(pwszRowName), parameters));
                }
                else
                {
                    WCHAR defaultRowName[MAX_PATH] = { 0 };
                    wsprintf(defaultRowName, L"%d", rowIdx);
                    rowdata.insert(std::pair<std::wstring, testrowData_t>(std::wstring(defaultRowName), parameters));
                }
                rowIdx++;
            }
            break;
        }

        case XmlNodeType_EndElement:
        {
            LPCWSTR pwszLocalName = NULL;
            RETURN_IF_FAILED(pXmlReader->GetLocalName(&pwszLocalName, NULL));

            if ((_wcsicmp(pwszLocalName, TAG_TABLE) == 0))
            {
                bContinue = false;
            }
            break;
        }
        }
    }
    return S_OK;
}

HRESULT DataDrivenTestBase::ReadParameters(IXmlReader* pXmlReader, testrowData_t& parameters)
{
    RETURN_HR_IF_NULL(E_INVALIDARG, pXmlReader);
    bool bContinue = true;
    while (bContinue)
    {
        XmlNodeType  nodeType;
        RETURN_IF_FAILED(pXmlReader->Read(&nodeType));

        switch (nodeType)
        {
        case XmlNodeType_Element:
        {
            LPCWSTR pwszLocalName = NULL;
            RETURN_IF_FAILED(pXmlReader->GetLocalName(&pwszLocalName, NULL));

            if (_wcsicmp(pwszLocalName, TAG_PARAMETER) == 0)
            {
                LPCWSTR pwszName = GetAttributeValueByName(L"Name", pXmlReader);

                // read content:  <Parmeter Name="name"> paramValue </Parameter>
                if (!pXmlReader->IsEmptyElement())
                {
                    LPCWSTR pwszValue = ReadContent(pXmlReader);

                    if (pwszValue && pwszName)
                    {
                        parameters.insert(std::pair<std::wstring, std::wstring>(std::wstring(pwszName), std::wstring(pwszValue)));
                    }
                }
            }
            else if ((_wcsicmp(pwszLocalName, TAG_PARAMETER) != 0))
            {
                RETURN_HR(E_FAIL); // malform data;
            }
            break;
        }

        case XmlNodeType_EndElement:
        {
            LPCWSTR pwszLocalName = NULL;
            RETURN_IF_FAILED(pXmlReader->GetLocalName(&pwszLocalName, NULL));

            if (pwszLocalName && (_wcsicmp(pwszLocalName, TAG_ROW) == 0))
            {
                bContinue = false;
                break;
            }
            break;
        }
        }
    }
    return S_OK;
}

LPCWSTR DataDrivenTestBase::GetAttributeValueByName(LPCWSTR pwszName, IXmlReader* pXmlReader)
{
    // push all attributes into map
    if (FAILED(pXmlReader->MoveToFirstAttribute()))
    {
        return nullptr;
    }
    for (;;)
    {
        if (!pXmlReader->IsDefault())
        {
            LPCWSTR pwszLocalName = nullptr;
            LPCWSTR pwszValue = nullptr;

            if (SUCCEEDED(pXmlReader->GetLocalName(&pwszLocalName, NULL)) &&
                (_wcsicmp(pwszName, pwszLocalName) == 0))
            {
                if (SUCCEEDED(pXmlReader->GetValue(&pwszValue, NULL)))
                {
                    return pwszValue;
                }
            }
        }
        if (S_OK != pXmlReader->MoveToNextAttribute())
            break;
    }
    return nullptr;
}

LPCWSTR DataDrivenTestBase::ReadContent(IXmlReader* pXmlReader)
{
    // read content:  <Parmeter Name="name"> paramValue </Parameter>
    XmlNodeType  nodeType;
    LPCWSTR pwszValue = NULL;
    if (SUCCEEDED(pXmlReader->Read(&nodeType)) &&
        (nodeType == XmlNodeType_Text))
    {
        if (SUCCEEDED(pXmlReader->GetValue(&pwszValue, NULL)))
        {
            return pwszValue;
        }
    }

    return nullptr;
}


std::wstring DataDrivenTestBase::GetModuleDirectory()
{
    WCHAR wszPath[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, wszPath, MAX_PATH);
    WCHAR* pwc = wcsrchr(wszPath, L'\\');

    if (!pwc)
    {
        return std::wstring();
    }
    *pwc = '\0';

    return std::wstring(wszPath);
}


