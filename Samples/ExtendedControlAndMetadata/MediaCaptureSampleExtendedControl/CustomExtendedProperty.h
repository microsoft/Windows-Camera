//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

//
// CustomExtendedProperty.h
//

#pragma once
#include "KsStructures.h"

#define KSCAMERA_EXTENDEDPROP_VERSION 1

//
// Helper class for using and accessing KSPROPERTYSETID_ExtendedCameraControl and their payloads
// 
namespace CustomExtendedProperties
{
    //
    // 
    //
    template<class T>
    class CustomExtendedProperty
    {
    public:
        static HRESULT CreateCustomExtendedProperty(Platform::String^ strPropId, BYTE* pbData, DWORD cbData, CustomExtendedProperty<T>** ppControl)
        {
            HRESULT hr = S_OK;

            if (nullptr == ppControl || nullptr == pbData)
            {
                return E_POINTER;
            }
            *ppControl = nullptr;

            if (cbData < (sizeof(KSCAMERA_EXTENDEDPROP_HEADER) + sizeof(T)))
            {
                return E_INVALIDARG;
            }

            *ppControl = new CustomExtendedProperty<T>(strPropId, pbData, cbData);
            if (*ppControl == nullptr)
            {
                hr = E_OUTOFMEMORY;
            }

            return hr;
        }

        virtual ~CustomExtendedProperty();

        Platform::Array<unsigned char>^ GetData();

        // Property functions
        __declspec(property(get = get_Id))UINT32 Id;
        UINT32 get_Id()
        {
            return m_uiPropId;
        };

        __declspec(property(get = get_Header, put = set_Header))KSCAMERA_EXTENDEDPROP_HEADER* Header;
        KSCAMERA_EXTENDEDPROP_HEADER* get_Header()
        {
            return m_pExtPropHeader;
        };
        void set_Header(KSCAMERA_EXTENDEDPROP_HEADER* pHeader)
        {
            m_pExtPropHeader = pHeader;
        };

        __declspec(property(get = get_Value, put = set_Value))T* Value;
        T* get_Value()
        {
            return m_pExtPropValue;
        };
        void set_Value(T* pValue)
        {
            m_pExtPropValue = pValue;
        };

        __declspec(property(get = get_DataSize))DWORD DataSize;
        DWORD get_DataSize()
        {
            return m_cbData;
        };

    protected:
        CustomExtendedProperty(Platform::String^ strPropId, BYTE* pbData, DWORD cbData);

    private:
        KSCAMERA_EXTENDEDPROP_HEADER* m_pExtPropHeader;
        T* m_pExtPropValue;
        BYTE* m_pbData;
        DWORD m_cbData;
        Platform::String^ m_strPropId;
    };

    template<class T>
    class CExtendedPropertyHelperBase
    {
    public:
        static HRESULT GetDeviceProperty(Windows::Media::Devices::VideoDeviceController^ vdc, Platform::String^ strPropId, CustomExtendedProperty<T>** ppControl);
        static HRESULT SetDeviceProperty(Windows::Media::Devices::VideoDeviceController^ vdc, Platform::String^ strPropId, CustomExtendedProperty<T>* pControl);
    };

    class ExtendedPropertyValueHelper
    {
    public:
        static HRESULT GetDeviceProperty(Windows::Media::Devices::VideoDeviceController^ vdc, Platform::String^ strPropId, CustomExtendedProperty<KSCAMERA_EXTENDEDPROP_VALUE>** ppControl);
        static HRESULT SetDeviceProperty(Windows::Media::Devices::VideoDeviceController^ vdc, Platform::String^ strPropId, CustomExtendedProperty<KSCAMERA_EXTENDEDPROP_VALUE>* pControl);
    };
    
    class ExtendedPropertyVideoProcSettingHelper
    {
    public:
        static HRESULT GetDeviceProperty(Windows::Media::Devices::VideoDeviceController^ vdc, Platform::String^ strPropId, CustomExtendedProperty<KSCAMERA_EXTENDEDPROP_VIDEOPROCSETTING>** ppControl);
        static HRESULT SetDeviceProperty(Windows::Media::Devices::VideoDeviceController^ vdc, Platform::String^ strPropId, CustomExtendedProperty<KSCAMERA_EXTENDEDPROP_VIDEOPROCSETTING>* pControl);
    };
}