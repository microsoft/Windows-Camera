
<!---
  samplefwlink: TBD
--->

# Windows Platform sample applications and tools for using and developing the Camera features

This repo contains a collection of samples that demonstrate the API usage patterns for using the camera related features in WinUI3, .Net, Universal Windows Platform (UWP) and Win32 Desktop applications for Windows 10 and Windows 11.

> **Note:** If you are unfamiliar with Git and GitHub, you can download the entire collection as a 
> [ZIP file](https://github.com/microsoft/Windows-Camera/archive/master.zip), but be 
> sure to unzip everything to access shared dependencies. 

## Content

- [Windows Studio Effects (*WSE*) camera sample application - C# .Net WinUI & WinRT](Samples/WindowsStudio/README.md)

- [Windows Studio Effects Driver-Defined Interfaces *(DDI)* documentation with C++ win32 code snippets](Samples/WindowsStudio/Windows%20Studio%20Effects%20DDIs.md)

- [Sample of Control Monitoring app that listens changes to various camera controls](Samples/ControlMonitorApp/readme.md)

- [Sample of Virtual Camera mediasource (IMFVirtualCamera) and configuration app](Samples/VirtualCamera/README.md)

- [Sample of Companion Settings app to read and save default configurations via IMFCameraConfigurationManager APIs](Samples/CameraSettingsExternalSettingsApp/readme.md)

- [Sample to demonstrate how to use the IMFSensorActivityMonitor API](Samples/SensorActivityMonitorConsoleApp/readme.md)

- [Sample to demonstrate basic camera application with MediaCapture APIs and WinUI3](Samples/MediaCaptureWinUI3/MediaCaptureWinUI3/Readme.md)

- [Win32 Desktop console application demonstrating use of WinRT Windows Media Capture APIs](Samples/WMCConsole_winrtcpp/README.md)

- [WinRT applications demonstrating use of custom KS Camera Extended Properties and extraction of frame metadata](Samples/ExtendedControlAndMetadata/README.md)

- [Win32 Desktop application and libraries demonstrating use of Windows APIs and Windows Media Foundation APIs for RTP/RTSP streaming from camera](Samples/NetworkMediaStreamer/Readme.md)

- [360-degree camera capture/record/preview](Tools/Cam360/README.md)

## Visual Studio requirement

These samples require Visual Studio and the Windows Software Development Kit (SDK) of varying minimal version for Windows 10 or higher on a per-sample basis.

   [Get a free copy of Visual Studio Community Edition](https://visualstudio.microsoft.com/)

Additionally, to stay on top of the latest updates to Windows and the development tools, become a Windows Insider by joining the Windows Insider Program.

   [Become a Windows Insider](https://insider.windows.com/)

## Using the samples

The easiest way to use these samples without using Git is to download the zip file containing the current version (using the following link or by clicking the "Download ZIP" button on the repo page). You can then unzip the entire archive and use the samples in Visual Studio.

   [Download the samples ZIP](../../archive/master.zip)

   **Notes:** 
   * Before you unzip the archive, right-click it, select **Properties**, and then select **Unblock**.
   * Be sure to unzip the entire archive, and not just individual samples. The samples all depend on the SharedContent folder in the archive.   
   * In Visual Studio 2017, the platform target defaults to ARM, so be sure to change that to x64 or x86 if you want to test on a non-ARM device. 
   
The samples use Linked files in Visual Studio to reduce duplication of common files, including sample template files and image assets. These common files are stored in the SharedContent folder at the root of the repository, and are referred to in the project files using links.

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


