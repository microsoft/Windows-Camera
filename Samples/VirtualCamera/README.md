# Virtual Camera project #
### Requirement:
&emsp;&emsp;Minimum OS version: 10.0.21364.0

This is a sample on how to create a virtual camera on Windows. This project consists of 5 parts
1. VirtualCameraWin32 <br>
MediaSource for the virtual camera. 

2. VirtualCamera_ConsoleApp <br>
Application to be used to create virtual camera on the system.

3. VirtualCamera_AppMSI <br>
Installable application to deploy or remove media source from the system

4. VirtualCamera_AppMSIX <br>
Packaged win32 console appliaction in to UWP

5. VirtualCameraTest <br>
Set of unit tests to validate virtual camera.

## Getting started
Download the project 
1. Build VirtualCamera_AppMSI
2. Build VirtualCamera_AppMSIX
3. Install VirtualCamera_AppMSI 
4. Install VirtualCamera_AppMSIX
5. From start -> type VirtualCameraApp -> Launch the application

The console application has 3 options, 
1 - register virtual camera, 
2 - remove virtual camera, 
3 - TestVirtualCamera. <br>
To start select option 1 to create a virtual camera, one the virtual camera is installed on the system. <br>
To test the virtual camera you can run option 3 from the console application or simply launch the inbox camera app or other camera application of your choice.

## VirtualCameraWin32 
----
MediaSource for Virtual Camera
In this exmaple, the mediasource is run with synethsize frames (SimpleMediaSource) or as a wrapper of existing hardware camera (HWMediaSource) <br> <br>
Documentation on media source [link](https://docs.microsoft.com/en-us/windows-hardware/drivers/stream/frame-server-custom-media-source#custom-media-source-dll)


## VirtualCamera_AppMSI
----
This project create a msi that is used to register and remove VirtualCameraWin32 mediasource.  On installation, the dll will be register on the system such that media source can be use to create virutal camera. On uninstall all virtual cameras created with the this MediaSource will be uninstalled

### Project Setup

1. Install Visual studio MSI project extension [link](https://marketplace.visualstudio.com/items?itemName=visualstudioclient.MicrosoftVisualStudio2017InstallerProjects>)

2. Create MSI project in Visual studio 

3. Add binaries
This should include mediasource dll and the executable that is responsbile for the uninstallation of virtual camera.

    1. Right-click on the "Setup Project", go on "Add" then click "Assembly".

    2. Browse for your COM assembly (.dll) and add it to the "Setup Project".

    3. After adding the assembly, right-click on it in "Solution Explorer" and click "Properties".

    4. Look for "Register" property and set it to "vsdrfDoNoRegister".

4. Add registry - register dll during installation
    1. Right-click on the Project, select "View" then "Registry"
    2. Add these keys and values
    HKLM\software\classes\CLSID\<CLSID_GUID>
    HKLM\software\classes\CLSID\<CLSID_GUID>\InProcServer32
    HKLM\software\classes\CLSID\<CLSID_GUID>\InProcServer32\Default    value:[TARGETDIR]<MediaSourceDll>
    HKLM\software\classes\CLSID\<CLSID_GUID>\InProcServer32\Threading  value:"Both"

5. Add UnInstall routine
    1. Right-click on the Project, select "View" then "Custom Actions"
    2. Right-click on "UnInstall", select "Add Custom Action"
    3. From the popup windows select the executable with the uninstall routine.  In this example it is VirtualCamera_ConsoleApp.exe /uninstall

## VirtualCamera_AppMSIX
----
This project package the desktop VirtualCamera_ConsoleApp executable into a UWP application.
This is a sample configuration application that is responsible of create / remove / configure the virtual camera related to the mediasource install via VirtualCamera_AppMSI.

Alternatively you can create a UWP application 

## VirtualCameraTest
----
Basic unit test run VirutalCameraWin32 media soource implemented with Google test framework.  The test provides basic validation to mediasource implementation. <br> <br>
Both HWMediaSourceUT and SimpleMediaSourceUT unit tests are used to validate the implementation of the media source is confined with guideline [here] (https://docs.microsoft.com/en-us/windows-hardware/drivers/stream/frame-server-custom-media-source#custom-media-source-dll).  These tests will load the media source in the test process ensure the basic functionality of the media source. <br> <br>
VirtualCameraUnit required installation will create a virtual camera.  The media source in this case will be loaded in FrameServer process.  To debub the media source running as virtual camera you will need to attach debugger to FrameServer process.

### How to run test 
1. Build VirtualCamera_AppMSI
2. Install the application

Run test from Visual Studio
1. Select and Run VirtualCameraTest project 
2. View result from "Test Explorer" (View -> Select "Test Explorer")

Run test from console
1. Build VirtualCameraTest project
2. Run VirtualCameraTest.exe  

<b>List tests </b>
``` 
VirtualCameraTest.exe --gtest_list_tests
```

<b>Run Selected test </b>
```
VirtualCameraTest.exe --gtest_filter=<test name>
```