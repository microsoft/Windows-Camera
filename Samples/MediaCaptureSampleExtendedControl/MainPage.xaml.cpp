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
// MainPage.xaml.cpp
// Implementation of the MainPage class.
//

#include "pch.h"
#include "MainPage.xaml.h"
#include "ppl.h"
#include "CustomExtendedProperty.h"

using namespace MediaCaptureSampleExtendedControls;

using namespace Windows::System;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Platform;
using namespace Windows::UI;
using namespace Windows::UI::Core;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Navigation;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Media::Imaging;
using namespace Windows::Storage;
using namespace Windows::Media;
using namespace Windows::Media::MediaProperties;
using namespace Windows::Media::Capture;
using namespace Windows::Media::Capture::Frames;
using namespace Windows::Storage::Streams;

using namespace concurrency;
using namespace CustomExtendedProperties;

MainPage::MainPage()
{
	InitializeComponent();
	ScenarioInit();
}

void MainPage::sldBrightness_ValueChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs^ e)
{
	try
	{
		bool succeeded = m_mediaCaptureMgr->VideoDeviceController->Brightness->TrySetAuto(false);
		if (!succeeded)
		{
			ShowStatusMessage("Set Brightness manual failed");
		}
		succeeded = m_mediaCaptureMgr->VideoDeviceController->Brightness->TrySetValue(sldBrightness->Value);
		if (!succeeded)
		{
			ShowStatusMessage("Set Brightness failed");
		}
	}
	catch (Platform::Exception^ e)
	{
		ShowExceptionMessage(e);
	}
}

void MainPage::sldContrast_ValueChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs^ e)
{
	try
	{
		bool succeeded = m_mediaCaptureMgr->VideoDeviceController->Contrast->TrySetAuto(false);
		if (!succeeded)
		{
			ShowStatusMessage("Set Contrast manual failed");
		}
		succeeded = m_mediaCaptureMgr->VideoDeviceController->Contrast->TrySetValue(sldContrast->Value);
		if (!succeeded)
		{
			ShowStatusMessage("Set Contrast failed");
		}
	}
	catch (Platform::Exception^ e)
	{
		ShowExceptionMessage(e);
	}
}

//
// The preferred method for accessing Exposure control is from the VideoDeviceController->Exposure,
//  but method below shows how to access a KSProperty Extended Property Control with a VideoProcSetting payload as an example
//
void MainPage::sldExposure_ValueChanged(Platform::Object^ sender, Windows::UI::Xaml::Controls::Primitives::RangeBaseValueChangedEventArgs^ e)
{
	try
	{
		Platform::String^ stringID = L"{1CB79112-C0D2-4213-9CA6-CD4FDB927972},12"; //KSCameraExtendedControlPropSet and ExposurePropSetID
		CustomExtendedProperty<KSCAMERA_EXTENDEDPROP_VIDEOPROCSETTING>* pControl = nullptr;
		HRESULT hr = ExtendedPropertyVideoProcSettingHelper::GetDeviceProperty(m_mediaCaptureMgr->VideoDeviceController, stringID, &pControl);
		if (hr == S_OK)
		{
			
			pControl->Header->Flags = KSCAMERA_EXTENDEDPROP_VIDEOPROCFLAG_MANUAL;
			pControl->Value->VideoProc.Value.ull = (ULONGLONG)sldExposure->Value;

			hr = ExtendedPropertyVideoProcSettingHelper::SetDeviceProperty(m_mediaCaptureMgr->VideoDeviceController, stringID, pControl);
		}

		if (hr != S_OK)
		{
			ShowStatusMessage("Exposure failed");
		}
	}
	catch (Platform::Exception^ e)
	{
		ShowExceptionMessage(e);
	}
}

void MainPage::btnStartDevice_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	try
	{
		btnStartDevice1->IsEnabled = false;
		ShowStatusMessage("Starting device");

		auto mediaCapture = ref new Windows::Media::Capture::MediaCapture();
		m_mediaCaptureMgr = mediaCapture;

		auto settings = ref new Capture::MediaCaptureInitializationSettings();
		m_settings = settings;

		FindIRCameraDeviceSensorGroup();

		create_task(m_mediaCaptureMgr->InitializeAsync(settings)).then([this](task<void> initTask)
		{
			try
			{
				initTask.get();

				auto mediaCapture = m_mediaCaptureMgr.Get();

				if (mediaCapture->MediaCaptureSettings->VideoDeviceId != nullptr && mediaCapture->MediaCaptureSettings->AudioDeviceId != nullptr)
				{
					btnStartPreview1->IsEnabled = true;
					btnStartStopRecord1->IsEnabled = true;
					btnTakePhoto1->IsEnabled = true;

					ShowStatusMessage("Device initialized successfully");

					mediaCapture->RecordLimitationExceeded += ref new Windows::Media::Capture::RecordLimitationExceededEventHandler(this, &MainPage::RecordLimitationExceeded);
					mediaCapture->Failed += ref new Windows::Media::Capture::MediaCaptureFailedEventHandler(this, &MainPage::Failed);

				}
				else
				{
					btnStartDevice1->IsEnabled = true;
					ShowStatusMessage("No VideoDevice/AudioDevice Found");
				}

			}
			catch (Exception ^ e)
			{
				ShowExceptionMessage(e);
			}
		}
		);
	}
	catch (Exception ^ e)
	{
		ShowExceptionMessage(e);
	}
}

void MainPage::btnStartPreview_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	try
	{
		ShowStatusMessage("Starting preview");
		btnStartPreview1->IsEnabled = false;
		auto mediaCapture = m_mediaCaptureMgr.Get();

		previewCanvas1->Visibility = Windows::UI::Xaml::Visibility::Visible;
		previewElement1->Source = mediaCapture;
		create_task(mediaCapture->StartPreviewAsync()).then([this](task<void> previewTask)
		{
			try
			{
				previewTask.get();
				auto mediaCapture = m_mediaCaptureMgr.Get();
				m_bPreviewing = true;
				ShowStatusMessage("Start preview successful");
				if (mediaCapture->VideoDeviceController->Brightness)
				{
					SetupVideoDeviceControl(mediaCapture->VideoDeviceController->Brightness, sldBrightness);
				}
				if (mediaCapture->VideoDeviceController->Contrast)
				{
					SetupVideoDeviceControl(mediaCapture->VideoDeviceController->Contrast, sldContrast);
				}

				Platform::String^ stringExposureID = L"{1CB79112-C0D2-4213-9CA6-CD4FDB927972},12"; //KSCameraExtendedControlPropSet and ExposurePropSetID
				Platform::String^ stringOISID = "{1CB79112-C0D2-4213-9CA6-CD4FDB927972},29"; //KSCameraExtendedControlPropSet and OISPropSetID
				SetupExtendedCustomControl(mediaCapture->VideoDeviceController, stringExposureID, sldExposure);
				SetupExtendedCustomControl(mediaCapture->VideoDeviceController, stringOISID, btnEnableOIS);

			}
			catch (Exception ^e)
			{
				m_bPreviewing = false;
				previewElement1->Source = nullptr;
				btnStartPreview1->IsEnabled = true;
				ShowExceptionMessage(e);
			}
		});
	}
	catch (Exception ^e)
	{
		m_bPreviewing = false;
		previewElement1->Source = nullptr;
		btnStartPreview1->IsEnabled = true;
		ShowExceptionMessage(e);
	}
}

void MainPage::btnStartStopRecord_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	try
	{
		btnStartStopRecord1->IsEnabled = false;
		playbackElement1->Source = nullptr;
		if (safe_cast<String^>(btnStartStopRecord1->Content) == "StartRecord")
		{
			if (!m_mediaCaptureMgr->MediaCaptureSettings->ConcurrentRecordAndPhotoSupported)
			{
				//if camera does not support record and Takephoto at the same time
				//disable TakePhoto button when recording
				btnTakePhoto1->IsEnabled = false;
			}
			StartRecord();
		}
		else //(safe_cast<String^>(btnStartStopRecord1->Content) == "StopRecord")
		{

			ShowStatusMessage("Stopping Record");
			create_task(m_mediaCaptureMgr->StopRecordAsync())
				.then([this](task<void> recordTask)
			{
				try
				{
					recordTask.get();
					m_bRecording = false;
					btnStartStopRecord1->IsEnabled = true;
					btnStartStopRecord1->Content = "StartRecord";

					if (!m_mediaCaptureMgr->MediaCaptureSettings->ConcurrentRecordAndPhotoSupported)
					{
						//if camera does not support lowlag record and lowlag photo at the same time
						//enable TakePhoto button after recording
						btnTakePhoto1->IsEnabled = true;
					}

					ShowStatusMessage("Stop record successful");
					if (!m_bSuspended)
					{
						create_task(this->m_recordStorageFile->OpenAsync(FileAccessMode::Read))
							.then([this](task<IRandomAccessStream^> streamTask)
						{
							try
							{
								auto stream = streamTask.get();
								ShowStatusMessage("Record file opened");
								ShowStatusMessage(this->m_recordStorageFile->Path);
								playbackElement1->AutoPlay = true;
								playbackElement1->SetSource(stream, this->m_recordStorageFile->FileType);
								playbackElement1->Play();
							}
							catch (Exception ^e)
							{
								ShowExceptionMessage(e);
								m_bRecording = false;
								btnStartStopRecord1->IsEnabled = true;
								btnStartStopRecord1->Content = "StartRecord";
							}
						});
					}
				}
				catch (Exception ^e)
				{
					ShowExceptionMessage(e);
					m_bRecording = false;
					btnStartStopRecord1->IsEnabled = true;
					btnStartStopRecord1->Content = "StartRecord";
				}
			});
		}
	}
	catch (Exception ^e)
	{
		ShowExceptionMessage(e);
		m_bRecording = false;
		btnStartStopRecord1->IsEnabled = true;
		btnStartStopRecord1->Content = "StartRecord";
	}
}

void MainPage::btnTakePhoto_Click(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	try
	{
		ShowStatusMessage("Taking photo");
		btnTakePhoto1->IsEnabled = false;

		if (!m_mediaCaptureMgr->MediaCaptureSettings->ConcurrentRecordAndPhotoSupported)
		{
			//if camera does not support record and Takephoto at the same time
			//disable Record button when taking photo
			btnStartStopRecord1->IsEnabled = false;
		}

		create_task(KnownFolders::PicturesLibrary->CreateFileAsync(PHOTO_FILE_NAME, Windows::Storage::CreationCollisionOption::GenerateUniqueName))
			.then([this](task<StorageFile^> getFileTask)
		{

			this->m_photoStorageFile = getFileTask.get();
			ShowStatusMessage("Create photo file successful");
			ImageEncodingProperties^ imageProperties = ImageEncodingProperties::CreateJpeg();

			return m_mediaCaptureMgr->CapturePhotoToStorageFileAsync(imageProperties, this->m_photoStorageFile);
		}).then([this](task<void> photoTask)
		{
			photoTask.get();
			btnTakePhoto1->IsEnabled = true;
			ShowStatusMessage("Photo taken");

			if (!m_mediaCaptureMgr->MediaCaptureSettings->ConcurrentRecordAndPhotoSupported)
			{
				//if camera does not support lowlag record and lowlag photo at the same time
				//enable Record button after taking photo
				btnStartStopRecord1->IsEnabled = true;
			}

			return this->m_photoStorageFile->OpenAsync(FileAccessMode::Read);
		}).then([this](task<IRandomAccessStream^> getStreamTask)
		{
			try
			{
				auto photoStream = getStreamTask.get();
				ShowStatusMessage("File open successful");
				auto bmpimg = ref new BitmapImage();

				bmpimg->SetSource(photoStream);
				imageElement1->Source = bmpimg;
			}
			catch (Exception^ e)
			{
				ShowExceptionMessage(e);
				btnTakePhoto1->IsEnabled = true;
			}
		});
	}
	catch (Exception^ e)
	{
		ShowExceptionMessage(e);
		btnTakePhoto1->IsEnabled = true;
	}
}

void MainPage::btnToggleOIS(Platform::Object^ sender, Windows::UI::Xaml::RoutedEventArgs^ e)
{
	try
	{
		btnEnableOIS->IsEnabled = false;

		Platform::String^ stringID = "{1CB79112-C0D2-4213-9CA6-CD4FDB927972},29"; //KSCameraExtendedControlPropSet and OISSetID
		CustomExtendedProperty<KSCAMERA_EXTENDEDPROP_VALUE>* pControl = nullptr;
		HRESULT hr = ExtendedPropertyValueHelper::GetDeviceProperty(m_mediaCaptureMgr->VideoDeviceController, stringID, &pControl);

		if (pControl->Header->Flags == KSCAMERA_EXTENDEDPROP_OIS_OFF)
		{
			m_bOIS = true;
			pControl->Header->Flags = KSCAMERA_EXTENDEDPROP_OIS_ON;
		}
		else
		{
			m_bOIS = false;
			pControl->Header->Flags = KSCAMERA_EXTENDEDPROP_OIS_OFF;
		}
		if (hr == S_OK)
		{
			hr = ExtendedPropertyValueHelper::SetDeviceProperty(m_mediaCaptureMgr->VideoDeviceController, stringID, pControl);
		}
		if (hr != S_OK)
		{
			ShowStatusMessage("OIS Failed");
		}
		else
		{
			ShowStatusMessage("OIS Succeeded");
		}

		btnEnableOIS->IsEnabled = true;
		if (m_bOIS)
		{
			btnEnableOIS->Content = "OIS Enabled";
		}
		else
		{
			btnEnableOIS->Content = "OIS Disabled";
		}
	}
	catch (Exception ^e)
	{
		ShowExceptionMessage(e);
	}
}

/// <summary>
/// Invoked when this page is about to be displayed in a Frame.
/// </summary>
/// <param name="e">Event data that describes how this page was reached.  The Parameter
/// property is typically used to configure the page.</param>
void MainPage::OnNavigatedTo(NavigationEventArgs^ e)
{
	// A pointer back to the main page.  This is needed if you want to call methods in MainPage such
	// as NotifyUser()
	SystemMediaTransportControls^ systemMediaControls = SystemMediaTransportControls::GetForCurrentView();
	m_eventRegistrationToken = systemMediaControls->PropertyChanged += ref new TypedEventHandler<SystemMediaTransportControls^, SystemMediaTransportControlsPropertyChangedEventArgs^>(this, &MainPage::SystemMediaControlsPropertyChanged);
}

void MainPage::OnNavigatedFrom(NavigationEventArgs^ e)
{
	SystemMediaTransportControls^ systemMediaControls = SystemMediaTransportControls::GetForCurrentView();
	systemMediaControls->PropertyChanged -= m_eventRegistrationToken;

	ScenarioClose();

}

void  MainPage::ScenarioInit()
{
	btnStartDevice1->IsEnabled = true;
	btnStartPreview1->IsEnabled = false;
	btnStartStopRecord1->IsEnabled = false;
	btnStartStopRecord1->Content = "StartRecord";
	btnTakePhoto1->IsEnabled = false;
	btnTakePhoto1->Content = "TakePhoto";

	m_bRecording = false;
	m_bPreviewing = false;
	m_bSuspended = false;

	previewElement1->Source = nullptr;
	playbackElement1->Source = nullptr;
	imageElement1->Source = nullptr;
	sldBrightness->IsEnabled = false;
	sldContrast->IsEnabled = false;
	sldExposure->IsEnabled = false;
}

void MainPage::ScenarioClose()
{
	if (m_bRecording)
	{
		ShowStatusMessage("Stopping Record, Preview and Camera");
		m_bRecording = false;
		create_task(m_mediaCaptureMgr->StopRecordAsync()).then([this](task<void> recordTask)
		{
			try
			{
				recordTask.get();
				previewElement1->Source = nullptr;
				delete(m_mediaCaptureMgr.Get());
				delete(m_settings.Get());
				m_bPreviewing = false;
			}
			catch (Exception ^e)
			{
				ShowExceptionMessage(e);
			}
		});
	}
	else //!(m_bRecording)
	{
		previewElement1->Source = nullptr;
		try
		{
			if (m_mediaCaptureMgr.Get() != nullptr)
			{
				delete(m_mediaCaptureMgr.Get());
				m_bPreviewing = false;
			}
		}
		catch (Exception ^e)
		{
			ShowExceptionMessage(e);
		}
	}

	m_bInit = false;
}

void MainPage::SystemMediaControlsPropertyChanged(SystemMediaTransportControls^ sender, SystemMediaTransportControlsPropertyChangedEventArgs^ e)
{
	switch (e->Property)
	{
	case SystemMediaTransportControlsProperty::SoundLevel:
		create_task(Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::High, ref new Windows::UI::Core::DispatchedHandler([this, sender]()
		{
			if (sender->SoundLevel != SoundLevel::Muted)
			{
				ScenarioInit();
			}
			else
			{
				ScenarioClose();
			}
		})));
		break;

	default:
		break;
	}
}

void MainPage::RecordLimitationExceeded(Windows::Media::Capture::MediaCapture ^currentCaptureObject)
{

	if (m_bRecording)
	{
		create_task(Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::High, ref new Windows::UI::Core::DispatchedHandler([this]()
		{

			ShowStatusMessage("Stopping Record on exceeding max record duration");
			btnStartStopRecord1->IsEnabled = false;

		}))).then([this]()
		{
			return m_mediaCaptureMgr->StopRecordAsync();
		}
		).then([this](task<void> recordTask)
		{
			try
			{
				recordTask.get();
				m_bRecording = false;
				btnStartStopRecord1->Content = "StartRecord";
				btnStartStopRecord1->IsEnabled = true;
				ShowStatusMessage("Stopped record on exceeding max record duration:" + m_recordStorageFile->Path);

				if (!m_mediaCaptureMgr->MediaCaptureSettings->ConcurrentRecordAndPhotoSupported)
				{
					//if camera does not support record and Takephoto at the same time
					//enable TakePhoto button again, after record finished
					btnTakePhoto1->Content = "TakePhoto";
					btnTakePhoto1->IsEnabled = true;
				}

			}
			catch (Exception ^e)
			{
				ShowExceptionMessage(e);
				m_bRecording = false;
				btnStartStopRecord1->Content = "StartRecord";
				btnStartStopRecord1->IsEnabled = true;
			}
		});
	}

}

void MainPage::Failed(Windows::Media::Capture::MediaCapture ^currentCaptureObject, Windows::Media::Capture::MediaCaptureFailedEventArgs^ currentFailure)
{
	String ^message = "Fatal error: " + currentFailure->Message;
	create_task(Dispatcher->RunAsync(Windows::UI::Core::CoreDispatcherPriority::High,
		ref new Windows::UI::Core::DispatchedHandler([this, message]()
	{
		ShowStatusMessage(message);
	})));
}

void MainPage::StartRecord()
{

	ShowStatusMessage("Starting Record");
	String ^fileName;
	fileName = VIDEO_FILE_NAME;

	create_task(KnownFolders::VideosLibrary->CreateFileAsync(fileName, Windows::Storage::CreationCollisionOption::GenerateUniqueName))
		.then([this](task<StorageFile^> fileTask)
	{
		this->m_recordStorageFile = fileTask.get();
		ShowStatusMessage("Create record file successful");

		MediaEncodingProfile^ recordProfile = nullptr;
		recordProfile = MediaEncodingProfile::CreateMp4(Windows::Media::MediaProperties::VideoEncodingQuality::Auto);

		return m_mediaCaptureMgr->StartRecordToStorageFileAsync(recordProfile, this->m_recordStorageFile);
	}).then([this](task<void> recordTask)
	{
		try
		{
			recordTask.get();
			m_bRecording = true;
			btnStartStopRecord1->Content = "StopRecord";
			btnStartStopRecord1->IsEnabled = true;

			ShowStatusMessage("Start Record successful");
		}
		catch (Exception ^e)
		{
			m_bRecording = false;
			btnStartStopRecord1->Content = "StartRecord";
			btnStartStopRecord1->IsEnabled = true;
			ShowExceptionMessage(e);
		}
	});
}

void MainPage::SetupVideoDeviceControl(Windows::Media::Devices::MediaDeviceControl^ videoDeviceControl, Slider^ slider)
{
	try
	{
		if ((videoDeviceControl->Capabilities)->Supported)
		{
			slider->IsEnabled = true;
			slider->Maximum = videoDeviceControl->Capabilities->Max;
			slider->Minimum = videoDeviceControl->Capabilities->Min;
			slider->StepFrequency = videoDeviceControl->Capabilities->Step;
			double controlValue = 0;
			if (videoDeviceControl->TryGetValue(&controlValue))
			{
				slider->Value = controlValue;
			}
		}
		else
		{
			slider->IsEnabled = false;
		}
	}
	catch (Platform::Exception^ e)
	{
		ShowExceptionMessage(e);
	}
}

void MainPage::SetupExtendedCustomControl(Windows::Media::Devices::VideoDeviceController^ videoDeviceController, Platform::String^  stringID, Slider^ slider)
{
	try
	{
		CustomExtendedProperty<KSCAMERA_EXTENDEDPROP_VIDEOPROCSETTING>* pControl = nullptr;
		HRESULT hr = ExtendedPropertyVideoProcSettingHelper::GetDeviceProperty(m_mediaCaptureMgr->VideoDeviceController, stringID, &pControl);

		if(hr == S_OK)
		{
			slider->IsEnabled = true;
			slider->Maximum = pControl->Value->Max;
			slider->Minimum = pControl->Value->Min;
			slider->StepFrequency = pControl->Value->Step;
			slider->Value = (double)pControl->Value->VideoProc.Value.ull;
		}
		else
		{
			slider->IsEnabled = false;
		}
	}
	catch (Platform::Exception^ e)
	{
		ShowExceptionMessage(e);
	}
}

void MainPage::SetupExtendedCustomControl(Windows::Media::Devices::VideoDeviceController^ videoDeviceController, Platform::String^  stringID, Button^ button)
{
	try
	{
		CustomExtendedProperty<KSCAMERA_EXTENDEDPROP_VALUE>* pControl = nullptr;
		HRESULT hr = ExtendedPropertyValueHelper::GetDeviceProperty(m_mediaCaptureMgr->VideoDeviceController, stringID, &pControl);

		if (hr == S_OK)
		{
			button->IsEnabled = true;
		}
		else
		{
			button->IsEnabled = false;
		}
	}
	catch (Platform::Exception^ e)
	{
		ShowExceptionMessage(e);
	}
}

//
// For Enumerating and choosing a device, we use MediaFrameSourceGroup to identify and use the first SensorGroup with an IR Stream
//  on the system. This example can be tailored to an application's needs for specific camera scenarios.
//
void MainPage::FindIRCameraDeviceSensorGroup()
{
	create_task(MediaFrameSourceGroup::FindAllAsync()).then([this](task<IVectorView<MediaFrameSourceGroup^>^> frameSourceTask)
	{
		try
		{
			auto groups = frameSourceTask.get();
            auto settings = m_settings.Get();
			
			MediaFrameSourceGroup^ selectedGroup = nullptr;
			MediaFrameSourceInfo^ selectedSourceInfo = nullptr;

			for (MediaFrameSourceGroup^ sourceGroup : groups)
			{
				for (MediaFrameSourceInfo^ sourceInfo : sourceGroup->SourceInfos)
				{
					if (sourceInfo->SourceKind == MediaFrameSourceKind::Infrared)
					{
						selectedSourceInfo = sourceInfo;
						break;
					}
				}

				if (selectedSourceInfo != nullptr)
				{
					selectedGroup = sourceGroup;
					break;
				}
			}


			if (selectedGroup != nullptr && selectedSourceInfo != nullptr)
			{
				settings->SourceGroup = selectedGroup;
				settings->StreamingCaptureMode = StreamingCaptureMode::Video;
			}

		}
		catch (Exception ^ e)
		{
			ShowExceptionMessage(e);
		}
	});
	return;
}

void MainPage::ShowStatusMessage(Platform::String^ text)
{
	NotifyUser(text, NotifyType::StatusMessage);
}

void MainPage::ShowExceptionMessage(Platform::Exception^ ex)
{
	NotifyUser(ex->Message, NotifyType::ErrorMessage);
}

void MainPage::NotifyUser(Platform::String^ strMessage, NotifyType type)
{
	switch (type)
	{
	case NotifyType::StatusMessage:
		StatusBorder->Background = ref new SolidColorBrush(Windows::UI::Colors::Green);
		break;
	case NotifyType::ErrorMessage:
		StatusBorder->Background = ref new SolidColorBrush(Windows::UI::Colors::Red);
		break;
	default:
		break;
	}
	StatusBlock->Text = strMessage;

	//Collapse the StatusBlock if it has no text to conserve real estate
	if (StatusBlock->Text != "")
	{
		StatusBlock->Visibility = Windows::UI::Xaml::Visibility::Visible;
		StatusBorder->Visibility = Windows::UI::Xaml::Visibility::Visible;
	}
	else
	{
		StatusBlock->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
		StatusBorder->Visibility = Windows::UI::Xaml::Visibility::Collapsed;
	}
}