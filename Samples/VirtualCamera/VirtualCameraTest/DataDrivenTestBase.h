//
// Copyright (C) Microsoft Corporation. All rights reserved.
//
#pragma once
#ifndef DATADRIVENTEST_BASE_H
#define DATADRIVENTEST_BASE_H

#include <map>
#include <string>
#include "gtest/gtest.h"

// Custom data driver test
// This is a xml data file driven test
// XML data table format
// <Data>
// <Table  Id="REQUIRE:table name>
//     <Row Name=Optional: row name>
//        <Parameter Name=REQUIRE:param name>parameter value </Parameter>
//          ...
//        <Parameter Name=REQUIRE:param name>parameter value </Parameter>
//     </Row>
// </Table>
//  ...
// <Table  Id="REQUIRE:table name>
//     <Row Name=Optional: row name>
//        <Parameter Name=REQUIRE:param name>parameter value </Parameter>
//          ...
//        <Parameter Name=REQUIRE:param name>parameter value </Parameter>
//     </Row>
// </Table>
//</Data>
//
// Each Test can reference a table, and testcases will be generated for each row.
// To define test use the follow:
//
//    class TestClass : public DataDrivenTestBase { ... };
//    TEST_P(TestClass, TestName) { ...};
//    INSTANTIATE_DATADRIVENTEST_CASE_P(TestClass, Table.xml, TableName);
//
//  The test cases generate will have the follow naming convention
//  Note: row name is optional, if no row name is provided, the default name
//        is the row index
// 
//    TestClass.TestName/RowName
// 
struct ci_compare
{
    struct nocase_compare
    {
        bool operator() (const wchar_t& c1, const wchar_t& c2) const
        {
            return towlower(c1) < towlower(c2);
        }
    };

    bool operator() (const std::wstring& s1, const std::wstring& s2) const
    {
        return (std::lexicographical_compare(s1.begin(), s1.end(), s2.begin(), s2.end(), nocase_compare()));
    }
};

typedef std::map < std::wstring, std::wstring, ci_compare > testrowData_t;

class DataDrivenTestBase :
    public ::testing::TestWithParam<std::pair<const std::wstring, testrowData_t>>
{
public:

    static std::map<std::wstring, testrowData_t> LoadDataFromTable(LPCWSTR pwszXmlFile, LPCWSTR pwszTableName)
    {
        std::map<std::wstring, testrowData_t> data;
        THROW_IF_FAILED(LoadDataFromXml(pwszXmlFile, pwszTableName, data));
        return data;
    };

protected:
    const testrowData_t& TestData() const { return GetParam().second; }

private:
    static HRESULT LoadDataFromXml(LPCWSTR pwszXmlFile, LPCWSTR pwszTable, std::map<std::wstring, testrowData_t>& data);
    static HRESULT LoadXml(LPCWSTR pwszXMLFile, IXmlReader** ppXMLReader);
    static HRESULT FindTable(LPCWSTR pwszTableId, IXmlReader* pXmlReader, bool& bTablefound);
    static HRESULT ReadRows(IXmlReader* pXmlReader, std::map<std::wstring, testrowData_t>& rowdata);
    static HRESULT ReadParameters(IXmlReader* pXmlReader, testrowData_t& parameters);
    static LPCWSTR GetAttributeValueByName(LPCWSTR pwszName, IXmlReader* pXmlReader);
    static LPCWSTR ReadContent(IXmlReader* pXmlReader);

    static std::wstring GetModuleDirectory();
};

# define INSTANTIATE_DATADRIVENTEST_CASE_P(test_class_name, dataFile, dataTable)         \
INSTANTIATE_TEST_CASE_P(                                                                \
    , /*Prefix*/                                                                        \
    test_class_name, /*TestClass*/                                                       \
    testing::ValuesIn(DataDrivenTestBase::LoadDataFromTable(dataFile, dataTable)),      \
    [](const testing::TestParamInfo<DataDrivenTestBase::ParamType>& info) {           \
        std::string name(info.param.first.begin(), info.param.first.end());             \
        return name;                                                                    \
    }                                                                                   \
);                                                                                      \

#endif
