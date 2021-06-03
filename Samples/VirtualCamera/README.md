# Virtual Camera Sample #
### Requirement:
&emsp;&emsp;Minimum OS version: --TBD-- (current: 10.0.21364.0)
Windows SDK, WDK minimum version: -- TBD-- (current: 10.0.21364.0)

This is a sample on how to create a virtual camera on Windows. This project consists of 5 parts
1. **VirtualCameraMediaSource** <br>
MediaSource for the virtual camera. 

2. **VirtualCamera_Installer** <br>
Application use by VirtualCameraMSI to handle removal of all virtual cameras create with the media source.

3. **VirtualCamera_MSI** <br>
This project package binaries from VirtualCamera_Installer and VirtualCameraMediaSource into msi for deployment.
The MSI is reponsible for deploying binaries, register media source dll, and removal of the media source and the associate virtual camera.

4. **VirtualCameraManager_WinRT and VirtualCameraManager_App**  <br>
WinRT component that projects lower level APIs for virtual camera and a UWP application with a GUI to create / remove / configure and interct with virtual cameras.

5. **VirtualCameraTest** <br>
Set of unit tests to validate virtual camera.

## Getting started
Download the project 
1. Build VirtualCamera_Installer 
2. Build VirtualCamera_MSI
3. Build VirtualCameraManager_WinRT and VirtualCameraManager_App 

4. Install VirtualCamera_MSI
5. Install VirtualCameraManager_App

## VirtualCameraMediaSource 
----
MediaSource for Virtual Camera.  In this sample, the mediasource is run with synthesized frames (SimpleMediaSource) or as a wrapper of existing hardware camera (HWMediaSource).  
Documentation on media source: [link](https://docs.microsoft.com/en-us/windows-hardware/drivers/stream/frame-server-custom-media-source#custom-media-source-dll)

### Project Setup
1. Create new project -> select "Empty Dll for Drivers (Universal) " [link](https://docs.microsoft.com/en-us/windows-hardware/drivers/develop/building-a-windows-driver)

## VirtualCamera_MSI
----
This project creates a msi that is used to register and remove VirtualCameraMediasource.dll.  On installation, the dll will be register on the system <br>
such that media source can be use to create virtual camera. On uninstallation, all virtual cameras created with the this VirtualCameraMediasource.dll will be uninstalled.

### Project Setup
1. Install Visual studio MSI project extension [link](https://marketplace.visualstudio.com/items?itemName=visualstudioclient.MicrosoftVisualStudio2017InstallerProjects>)

2. Create MSI project in Visual studio 

3. Add binaries  
This should include the mediasource dll, the executable that is responsbile for the uninstallation of virtual camera, and all additional dependencies.

    1. Right-click on the "Setup Project", go on "Add" then click "Assembly".
    2. Browse for your COM assembly (.dll) and add it to the "Setup Project".
    3. After adding the assembly, right-click on it in "Solution Explorer" and click "Properties".
    4. Look for "Register" property and set it to "vsdrfDoNoRegister".

4. Add registry - register dll during installation
    1. Right-click on the Project, select "View" then "Registry"
    2. Add these keys and values  
    HKLM\software\classes\CLSID\\<CLSID_GUID>  
    HKLM\software\classes\CLSID\\<CLSID_GUID>\InProcServer32  
    HKLM\software\classes\CLSID\\<CLSID_GUID>\InProcServer32\Default, value:[TARGETDIR]<MediaSourceDll>  
    HKLM\software\classes\CLSID\\<CLSID_GUID>\InProcServer32\Threading, value:"Both"

5. Add UnInstall routine
    1. Right-click on the Project, select "View" then "Custom Actions"
    2. Right-click on "UnInstall", select "Add Custom Action"
    3. From the popup windows select the executable with the uninstall routine.  
    In this example it is VirtualCamera_ConsoleApp.exe /uninstall

## VirtualCameraManager_App
----
### To build:
1. Build the *VirtualCameraManager_WinRT* project
2. Build the *VirtualCameraManager_App*
3. Either launch the app right away (right click on the *VirtualCameraManager_App* project and select *deploy*) or create an app package you can share around and sideload (right click on the *VirtualCameraManager_App* project and select *publish*)

### What the app does:
- When you launch the app, it will contain a barebone UI with a ➕ sign button at the top. This opens a dialog to create a new virtual camera.
- If you create a virtual camera with the *Session* lifetime, this virtual camera would then be removed upon closing the app. However, if you create a virtual camera with the *System* lifetime, it will remain functional until the system reboots or you manually remove it by either relaunching the app or manually removing it from the Windows Settings (Devices &rarr; Camera) page. 
The app caches the set of virtual camera it creates in a configuration file upon closure and invertedly reloads the virtual camera that were previously created (with a lifetime = "system") in a previous session.
(see the methods ```LoadConfigFileAsync()``` and ```WriteToConfigFileAsync()``` in *App.xaml.cs* in the *VirtualCameraManager_App* project).
If a user would like to recover virtual cameras created in the past after rebooting their system, then relaunching the app would cause that. You could automate this process if you wish on behalf of the user using a service for example that would start the app upon startup of the system.
- Once a virtual camera is created, if hooked into an existing camera and that camera supports the eye gaze correction control, then the UI element for it will be available. This is to show how a virtual camera can relay existing control interactions if they are supported onto the original camera it is hooked into. Similarly, if you did not hook into an existing camera, there will be a UI element to interact with the color of the synthetized frame via a custom control defined in the virtual camera (see *VirtualCameraMediaSource.h* in the *VirtualCameraMediaSource* project).
- The *VirtualCameraManager_WinRT* project contains helpful WinRT runtime classes and methods to encapsulate a virtual camera and interact with them, as well as generate them and query for camera KSProperties (in our case either custom or extended). Refer to the 2 *.idl* files defining the API signatures, they provide a good base to expand upon: 
  - *VirtualCameraManager.idl*
  - *CameraKsPropertyInquiry.idl*


## VirtualCameraTest
----
Basic unit test implemented with Google test framework to validate the mediasource implementation. <br> <br>
The below two test groups are used to validate the implementation of the media source is confined with guideline [here](https://docs.microsoft.com/en-us/windows-hardware/drivers/stream/frame-server-custom-media-source#custom-media-source-dll).<br>
These tests will load the media source in the test process validating the basic functionality of the media source. <br>
To debug the media source simply attached debugger to the test process.

* SimpleMediaSourceTest
* HWMediaSourceTest
* CustomMediaSourceTest

HWMediaSourceTest is required to run with physical camera presence on the system.  You configure the test by modifing the content in VirtualCameraTestData.xml (TableId: HWMediaSourceTest)<br>
For example, if you have more than one camera on the system you can specify the camera with one of the two options

By Device enumeration index
```
<Table Id="HWMediaSourceTest">
    <!-- VidDeviceSymLink or VidDeviceIndex--> 
    <Row >
      <Parameter Name="vidDeviceIndex">0</Parameter>
    </Row>
  </Table>
```

By Device Symbolic link name
```
<Table Id="HWMediaSourceTest">
    <!-- VidDeviceSymLink or VidDeviceIndex--> 
    <Row >
      <Parameter Name="VidDeviceSymLink">\\?\USB#VID_045E&PID_0779&MI_00#7&2afe51f&0&0000#{e5323777-f976-4f5b-9b55-b94699c46e44}\GLOBAL</Parameter>
    </Row>
  </Table>
```

CustomMediaSourceTest can use to test your media source implementation.  To run this against your media source dll, modify the content in 
VirutalCameraTestData.xml (TableId: CustomMediaSourceTest).  Set the clsid parameter with the clsid of your media source

```
<Table Id="CustomMediaSourceTest">
    <!-- 
    clsid of media source: example {7B89B92E-FE71-42D0-8A41-E137D06EA184} 
    -->
    <Row >
      <Parameter Name="clsid">{7B89B92E-FE71-42D0-8A41-E137D06EA184}</Parameter>
    </Row>
  </Table>
```

The below two test groups are used to validate virtual camera created with the media source.
The mediasource will be loaded in FrameServerMonitor or FrameServer services.  To debug the mediasource 
see [How to debug issues with media source](#faq)
* VirtualCameraSimpleMediaSourceTest
* VirtualCameraHWMediaSourceTest
* VirtualCameraCustommediaSource

Similar to HWMediaSourceTest, VirtualCameraHWMediaSourceTest is required to run with physical camera presence on the system.  To configure the physical camera selection, modify the content VirtualCameraTestData.xml (TableId: VirtualCameraHWMediaSourceTest) per instruction above  

VirtualCameraCustommediaSource can be configure to run against your mediasource as virtual camera.  To run this against your media source dll, modify the content in VirutalCameraTestData.xml (TableId: VirtualCameraCustomMediaSourceTest) per instruction above.

### <b>How to run test </b>
1. Build VirtualCamera_MSI
2. Install the application
3. Build VirtualCameraTest project

Opiton 1: From Visual Studio
1. Select and Run VirtualCameraTest project 
2. View result from "Test Explorer" (View -> Select "Test Explorer")

Opiton 2: Run test from console
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

### FAQ
----
<b> 1. CoCreateInstance of the dll failed with MOD_NOT_FOUND ?</b> <br/>
Check to ensure all dependencies required by the mediasource are registered.  Use dumpbin to find all the dependecies of the dll.
* Open Visual Studio command prompt (In Visual Studio ->select menu "Tools" -> Command line -> Developer Command prompt)
* run dumpbin 
  ```
  dumpbin /DEPENDENTS mediasource.dll
  ```

<b> 2. How to debug issues with media source? </b> <br/>
During VirtualCamera creation the mediaSource is loaded in the FrameServerMonitor service.  If you encounter issue prior to starting the virtual camera <br/>
you will need to attach the debugger to the FrameServerMonitor service.
* Open task manager -> view tab "Services" -> look for the FrameServerMonitor service
* Attach debugger to the process
* Alternative: use the powershell script provided: scripts\Debug-FrameServerMonitor.ps1 

In the case where  the virtualCamera is used by application or after VirtualCamera has started, the media source is loaded in the FrameServer service. To investigate issue with the mediasource you will need to attach the debugger to the FrameServer service.
* Open task manager -> view tab "Services" -> look for the FrameServer service 
* Attach debugger to the process
* Alternative: use the powershell script provided: scripts\Debug-FrameServer.ps1

<b> 3. Can I create a virtual camera that hooks into another existing virtual camera with this sample </b><br/>no, virtual camera inception is not currently supported.

<b> 4. In a UWP app, if I create a virtual camera on the UI thread without having been prompted for camera access first, the camera access prompt is unresponsive, what is going on? </b><br/>
If you are calling ```MFCreateVirtualCamera()``` (indirectly i.e. using the ```VirtualCameraRegistrar.RegisterNewVirtualCamera()``` method) on a UI thread, it will block it until camera access consent is given. However since that consent prompt is also running on the UI thread, it will actively block interaction and appear to freeze the app. A smart thing to do may be to either trigger consent upon launch of the app by another mean (i.e. calling ```MediaCapture.InitializeAsync()```) or by calling the API in a background thread (which the UWP VirtualCameraManager_App does).



