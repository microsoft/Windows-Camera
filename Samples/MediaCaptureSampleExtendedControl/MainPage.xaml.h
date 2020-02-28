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
// MainPage.xaml.h
// Declaration of the MainPage class.
//

#pragma once

#include "pch.h"
#include "MainPage.g.h"


using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::Graphics::Display;
using namespace Windows::UI::ViewManagement;
using namespace Windows::Devices::Enumeration;
#define VIDEO_FILE_NAME "video.mp4"
#define PHOTO_FILE_NAME "photo.jpg"

namespace MediaCaptureSampleExtendedControls
{

	enum NotifyType
	{
		StatusMessage,
		ErrorMessage
	};

	/// <summary>
	/// An empty page that can be used on its own or navigated to within a Frame.
	/// </summary>
	public ref class MainPage sealed
	{
	public:
		MainPage();

	protected:
		virtual void OnNavigatedTo(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;
		virtual void OnNavigatedFrom(Windows::UI::Xaml::Navigation::NavigationEventArgs^ e) override;

	private:
		void sldBrightness_ValueChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs^ e);
		void sldContrast_ValueChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs^ e);
		void sldExposure_ValueChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs^ e);

		void btnStartDevice_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void btnStartPreview_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void btnStartStopRecord_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void btnTakePhoto_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);
		void btnToggleOIS(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e);

		void ScenarioInit();
		void ScenarioClose();

		void Suspending(Object^ sender, Windows::ApplicationModel::SuspendingEventArgs^ e);
		void Resuming(Object^ sender, Object^ e);
		void SystemMediaControlsPropertyChanged(Windows::Media::SystemMediaTransportControls^ sender, Windows::Media::SystemMediaTransportControlsPropertyChangedEventArgs^ e);
		void RecordLimitationExceeded(Windows::Media::Capture::MediaCapture ^ mediaCapture);
		void Failed(Windows::Media::Capture::MediaCapture ^ mediaCapture, Windows::Media::Capture::MediaCaptureFailedEventArgs ^ args);
		void SetupVideoDeviceControl(Windows::Media::Devices::MediaDeviceControl^ videoDeviceControl, Slider^ slider);
		void SetupExtendedCustomControl(Windows::Media::Devices::VideoDeviceController^ videoDeviceController, Platform::String^  stringID, Slider^ slider);
		void SetupExtendedCustomControl(Windows::Media::Devices::VideoDeviceController^ videoDeviceController, Platform::String^  stringID, Button^ button);
		void ShowStatusMessage(Platform::String^ text);
		void ShowExceptionMessage(Platform::Exception^ ex);
		void NotifyUser(Platform::String^ strMessage, NotifyType type);

		void StartRecord();
		void FindIRCameraDeviceSensorGroup();

		Platform::Agile<Windows::Media::Capture::MediaCapture> m_mediaCaptureMgr;
		Platform::Agile <Windows::Media::Capture::MediaCaptureInitializationSettings> m_settings;
		Windows::Storage::StorageFile^ m_photoStorageFile;
		Windows::Storage::StorageFile^ m_recordStorageFile;
		bool m_bRecording;
		bool m_bEffectAdded;
		bool m_bSuspended;
		bool m_bPreviewing;
		bool m_bOIS;
		bool m_bInit;
		
		Windows::Foundation::EventRegistrationToken m_eventRegistrationToken;
	};
}
