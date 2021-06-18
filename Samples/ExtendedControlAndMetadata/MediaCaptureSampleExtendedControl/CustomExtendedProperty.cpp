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
// CustomExtendedProperty.cpp
//

#include "pch.h"
#include "CustomExtendedProperty.h"

using namespace CustomExtendedProperties;

template<typename T>
CustomExtendedProperty<T>::~CustomExtendedProperty()

{
	m_pExtPropHeader = nullptr;
	m_pExtPropValue = nullptr;
	m_cbData = 0;
	if (m_pbData)
	{
		free(m_pbData);
		m_pbData = nullptr;
	}
}

template<typename T>
CustomExtendedProperty<T>::CustomExtendedProperty(Platform::String^ strPropId, BYTE* pbData, DWORD cbData) :
	m_pbData(nullptr)
	, m_cbData(0)
	, m_pExtPropHeader(nullptr)
	, m_pExtPropValue(nullptr)
{
	m_pbData = (BYTE*)malloc(cbData);
	if (m_pbData == nullptr)
	{
		return;
	}
	CopyMemory(m_pbData, pbData, cbData);

	m_strPropId = strPropId;
	m_cbData = cbData;
	m_pExtPropHeader = (KSCAMERA_EXTENDEDPROP_HEADER*)m_pbData;
	m_pExtPropHeader->Version = KSCAMERA_EXTENDEDPROP_VERSION;
	m_pExtPropHeader->Size = sizeof(KSCAMERA_EXTENDEDPROP_HEADER) + sizeof(T);
	m_pExtPropValue = (T*)(m_pbData + sizeof(KSCAMERA_EXTENDEDPROP_HEADER));
}


template<typename T>
Platform::Array<unsigned char>^ CustomExtendedProperty<T>::GetData()
{
	Platform::Array<unsigned char>^ spArr = ref new Platform::Array<unsigned char>(m_pbData, m_cbData);
	return spArr;
}

HRESULT ExtendedPropertyValueHelper::SetDeviceProperty(Windows::Media::Devices::VideoDeviceController^ vdc, Platform::String^ strPropId, CustomExtendedProperty<KSCAMERA_EXTENDEDPROP_VALUE>* pControl)
{
	return CExtendedPropertyHelperBase<KSCAMERA_EXTENDEDPROP_VALUE>::SetDeviceProperty(vdc, strPropId, pControl);
}

HRESULT ExtendedPropertyValueHelper::GetDeviceProperty(Windows::Media::Devices::VideoDeviceController^ vdc, Platform::String^ strPropId, CustomExtendedProperty<KSCAMERA_EXTENDEDPROP_VALUE>** ppControl)
{
	return CExtendedPropertyHelperBase<KSCAMERA_EXTENDEDPROP_VALUE>::GetDeviceProperty(vdc, strPropId, ppControl);
}

HRESULT ExtendedPropertyVideoProcSettingHelper::SetDeviceProperty(Windows::Media::Devices::VideoDeviceController^ vdc, Platform::String^ strPropId, CustomExtendedProperty<KSCAMERA_EXTENDEDPROP_VIDEOPROCSETTING>* pControl)
{
	return CExtendedPropertyHelperBase<KSCAMERA_EXTENDEDPROP_VIDEOPROCSETTING>::SetDeviceProperty(vdc, strPropId, pControl);
}

HRESULT ExtendedPropertyVideoProcSettingHelper::GetDeviceProperty(Windows::Media::Devices::VideoDeviceController^ vdc, Platform::String^ strPropId, CustomExtendedProperty<KSCAMERA_EXTENDEDPROP_VIDEOPROCSETTING>** ppControl)
{
	return CExtendedPropertyHelperBase<KSCAMERA_EXTENDEDPROP_VIDEOPROCSETTING>::GetDeviceProperty(vdc, strPropId, ppControl);
}

template<typename T>
HRESULT CExtendedPropertyHelperBase<T>::SetDeviceProperty(Windows::Media::Devices::VideoDeviceController^ vdc, Platform::String^ strPropId, CustomExtendedProperty<T>* pControl)
{
	HRESULT hr = S_OK;
	if (nullptr == vdc || nullptr == pControl)
	{
		return E_INVALIDARG;
	}

	try
	{
		Platform::Array<unsigned char>^ spArr = pControl->GetData();
		Platform::Object^ spObj = Windows::Foundation::PropertyValue::CreateUInt8Array(spArr);
		vdc->SetDeviceProperty(strPropId, spObj);
	}
	catch (Platform::Exception ^ e)
	{
		hr = e->HResult;
	}

	return hr;
}

template<typename T>
HRESULT CExtendedPropertyHelperBase<T>::GetDeviceProperty(Windows::Media::Devices::VideoDeviceController^ vdc, Platform::String^ strPropId, CustomExtendedProperty<T>** ppControl)
{
	HRESULT hr = S_OK;
	if (nullptr == vdc || nullptr == ppControl)
	{
		return E_INVALIDARG;
	}

	try
	{
		Platform::Array<unsigned char>^ spVal;
		Windows::Foundation::IPropertyValue^ propertyValue = static_cast<Windows::Foundation::IPropertyValue^>(vdc->GetDeviceProperty(strPropId));
		propertyValue->GetUInt8Array(&spVal);

		hr = CustomExtendedProperty<T>::CreateCustomExtendedProperty(strPropId, spVal->Data, spVal->Length, ppControl);
	}
	catch (Platform::Exception ^ e)
	{
		hr = e->HResult;
	}

	return hr;
}