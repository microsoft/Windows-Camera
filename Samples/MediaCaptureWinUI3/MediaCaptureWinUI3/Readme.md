
<!---
  samplefwlink: TBD
--->

# WinUI Desktop sample applications for using the [Windows Media Capture APIs](https://docs.microsoft.com/en-us/uwp/api/Windows.Media.Capture.MediaCapture)

    This folder contains a sample application that demonstrates basic camera application with media capture API.

## About the sample
    The sample demonstrates the following:
        1. Media capture settings and initialization
        2. Preview with Media player element.

## How to use the application
1. Build and run the project
2. Use the left panel to configure the media capture session.
    * [VideoDeviceId](https://learn.microsoft.com/en-us/uwp/api/windows.media.capture.mediacaptureinitializationsettings.videodeviceid?view=winrt-26100)
    * [StreamingCaptureMode](https://learn.microsoft.com/en-us/uwp/api/windows.media.capture.mediacaptureinitializationsettings.streamingcapturemode?view=winrt-26100#windows-media-capture-mediacaptureinitializationsettings-streamingcapturemode)
    * [VideoProfile](https://learn.microsoft.com/en-us/uwp/api/windows.media.capture.mediacaptureinitializationsettings.videoprofile?view=winrt-26100#windows-media-capture-mediacaptureinitializationsettings-videoprofile)
    * [MemoryPreference](https://learn.microsoft.com/en-us/uwp/api/windows.media.capture.mediacaptureinitializationsettings.memorypreference?view=winrt-26100#windows-media-capture-mediacaptureinitializationsettings-memorypreference)
    
3. Click start to initialize the media capture session. 

4. Select a preview source
    * This application show how to select a preview source from a camera device. 
    
5. Click start preview.

6. select a new  media type.
    * This application show how to select the a renderable format that is supported by media player element.

7. Stop preview, start preview to change media type.


## Requirements

    This sample is built using Visual Studio 2022 and [Windows App SDK](https://learn.microsoft.com/en-us/windows/apps/windows-app-sdk/set-up-your-development-environment).

## Using the samples

The easiest way to use these samples without using Git is to download the zip file containing the current version (using the following link or by clicking the "Download ZIP" button on the repo page). You can then unzip the entire archive and use the samples in Visual Studio 2022.

   [Download the samples ZIP](../../archive/master.zip)

   **Notes:** 
   * Before you unzip the archive, right-click it, select **Properties**, and then select **Unblock**.
   * Be sure to unzip the entire archive, and not just individual samples. The samples all depend on the SharedContent folder in the archive.   
   * In Visual Studio 2022, the platform target defaults to ARM64, so be sure to change that to x64 if you want to test on a non-ARM64 device. 


**Reminder:** If you unzip individual samples, they will not build due to references to other portions of the ZIP file that were not unzipped. You must unzip the entire archive if you intend to build the samples.

For more info about the programming models, platforms, languages, and APIs demonstrated in these samples, please refer to the guidance, tutorials, and reference topics provided in the Windows 10 documentation available in the [Windows Developer Center](http://go.microsoft.com/fwlink/p/?LinkID=532421). These samples are provided as-is in order to indicate or demonstrate the functionality of the programming models and feature APIs for Windows.

## Contributing

This project welcomes contributions and suggestions.  Most contributions require you to agree to a
Contributor License Agreement (CLA) declaring that you have the right to, and actually do, grant us
the rights to use your contribution. For details, visit https://cla.microsoft.com.

When you submit a pull request, a CLA-bot will automatically determine whether you need to provide
a CLA and decorate the PR appropriately (e.g., label, comment). Simply follow the instructions
provided by the bot. You will only need to do this once across all repos using our CLA.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/).
For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or
contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

