# Virtual Camera Sample #
### Requirement:
- Minimum OS version: Windows 11 (10.0.22000.0)
- Windows SDK minimum version: [10.0.22000.0](https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/)
- Visual studio MSI project extension [link](https://marketplace.visualstudio.com/items?itemName=visualstudioclient.MicrosoftVisualStudio2017InstallerProjects)
- Latest VCRuntime redistributable installed [link](https://docs.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist)

This is a sample on how to create a virtual camera on Windows ([see API documentation](https://docs.microsoft.com/en-us/windows/win32/api/mfvirtualcamera/)). This project consists of 5 parts:
1. **VirtualCameraMediaSource** <br>
MediaSource for the virtual camera. This CustomMediaSource can run in 3 distinc modes: 
    1. **SimpleMediaSource** (also reffered to as *Synthetic* in application) which will create synthetic content to stream from the virtual camera.
    2. **HWMediaSource** (also reffered to as *CameraWrapper* in application) which will wrap an existing camera on the system but act as a passthrough.
    3. **AugmentedMediaSource** which will wrap an existing camera on the system but
        - filters the set of streams exposed (see [*AugmentedMediaSource::Initialize()*](VirtualCameraMediaSource/AugmentedMediaSource.cpp)
        - filters to set of MediaTypes exposed [*AugmentedMediaStream::Initialize()*](VirtualCameraMediaSource/AugmentedMediaStream.cpp) 
        - supports a custom DDI not supported originaly by the underlying existing camera [*AugmentedMediaSource::KsProperty()*](VirtualCameraMediaSource/AugmentedMediaSource.cpp)
        - communicates to the main app's system tray background process via RPC whenever it starts streaming (refer to [*AugmentedMediaSource::Start()*](VirtualCameraMediaSource/AugmentedMediaSource.cpp) and [*SystrayApplicationContext.CreateNamedPipeForStart()*](VirtualCameraSystray\SystrayApplicationContext.cs))

        > For the last 2 types of virtual camera which wrap an existing physical camera on the system, in **Windows build 22621** a new API was introduced to retrieve the associated camera's *IMFMediaSource* instance from *FrameServer* at activation time of the virtual camera instead of instantiating it using *MFCreateDeviceSource()* from within its implementation. This new API is used in *VirtualCameraMediaSourceActivateFactory::CreateInstance()* which in turn will put an *IMFAttribute* in its store called **MF_VIRTUALCAMERA_PROVIDE_ASSOCIATED_CAMERA_SOURCES**. This acts as a query to FrameServer to then provide the IMFMediaSource at activation time as seen in *VirtualCameraMediaSourceActivate::ActivateObject()* where the collection of *IMFDeviceSource* instances for the physical cameras corresponding to the symbolic link names specified at virtual camera creation time using *IMFVirtualCamera::AddDeviceSourceInfo()* can be retrieved by looking for an *IMFAttribute* named **MF_VIRTUALCAMERA_ASSOCIATED_CAMERA_SOURCES**. This alleviates some issues in apps regarding switching quickly between the physical camera and the virtual camera wrapping it, or with virtual camera not showing some MediaTypes exposed by the physical camera such as when FrameServer exposes decoded version (NV12) of MJPEG MediaType.

2. **VirtualCamera_Installer** <br>
A packaged Win32 application installed with *VirtualCameraMSI* that handles adding and removing virtual camera instances that are based on the *VirtualCameraMediaSource*.

3. **VirtualCamera_MSI** <br>
This project packages binaries from *VirtualCamera_Installer* and *VirtualCameraMediaSource* into MSI for deployment.
The MSI is reponsible for deploying binaries, register media source dll, and removal of the media source and the associate virtual camera.

4. **VirtualCameraManager_WinRT, VirtualCameraManager_App and VirtualCameraSystray**  <br>
WinRT component that projects [lower level APIs for virtual camera](https://docs.microsoft.com/en-us/windows/win32/api/mfvirtualcamera/) and a UWP application with a GUI to create / remove / configure and interact with virtual cameras. 
VirtualCameraSystray is a sidekick system tray fulltrust process launched from the UWP application to allow system tray user interactions as well as acting as a backround process listening for RPC calls coming from the virtual camera media source in order to launch the UWP app upon starting to stream with the virtual camera **AugmentedMediaSource** (won't launch with other types of virtual camera).
The UWP app requests to register a [startup task](https://docs.microsoft.com/en-us/uwp/api/Windows.ApplicationModel.StartupTask) upon launching for the first time and registers a [launch protocol](https://docs.microsoft.com/en-us/windows/uwp/launch-resume/how-to-launch-an-app-for-results) to be launched with arguments from it sidekick fulltrust system tray process.

5. **VirtualCameraTest** <br>
Set of unit tests to validate a virtual camera.

## Getting started
Download the project 
1. Build *VirtualCamera_Installer* (Release configuration)
2. Build *VirtualCameraMediaSource* (Release configuration)
3. Build *VirtualCamera_MSI*
4. Build *VirtualCameraSystray* and *VirtualCameraManager_WinRT* and *VirtualCameraManager_App* 

5. Install *VirtualCamera_MSI*
6. Install *VirtualCameraManager_App*

## VirtualCameraMediaSource 
----
MediaSource for Virtual Camera. In this sample, the mediasource is run with synthesized frames (SimpleMediaSource), as a passthrough wrapper of existing hardware camera (HWMediaSource) or an augmented wrapper of a existing hardware camera (AugmentedMediaSource).  
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
> Make sure you have already installed the *VirtualCameraMediaSource* by generating the previous MSI and installing it on the target system.
1. Build the *VirtualCameraSystray* project
2. Build the *VirtualCameraManager_WinRT* project
3. Build the *VirtualCameraManager_App*
4. Either launch the app right away (right click on the *VirtualCameraManager_App* project and select *deploy*) or create an app package you can share around and sideload (right click on the *VirtualCameraManager_App* project and select *publish*)

### What the app does:
- When you launch the app, it will contain a barebone UI with a ➕ sign button at the top. This opens a dialog to create a new virtual camera.
- If you create a virtual camera with the *Session* lifetime, this virtual camera would then be removed upon closing the app. However, if you create a virtual camera with the *System* lifetime, it will remain functional until the system reboots or you manually remove it by either relaunching the app or manually removing it from the Windows Settings (Devices &rarr; Camera) page. 
The app caches the set of virtual cameras it creates in a configuration file upon closure and invertedly reloads the virtual camera that were created (with a lifetime = *System*) in a previous session.
(see the methods ```LoadConfigFileAsync()``` and ```WriteToConfigFileAsync()``` in *App.xaml.cs* in the *VirtualCameraManager_App* project).
A user can recover virtual cameras created in the past after rebooting their system by relaunching the app. You could automate this process on behalf of the user by using a service that would start the app upon startup of the system.
- If a virtual camera is created from an existing camera that supports the eye gaze correction control, then the UI element for eye gaze correction will be available. This is to show how a virtual camera can forward supported controls to the original camera it is created from. Similarly, if you did not wrap an existing camera, there will be a UI element to interact with the color of the synthesized frame via a custom control defined in the virtual camera (see *VirtualCameraMediaSource.h* in the *VirtualCameraMediaSource* project).
- The *VirtualCameraManager_WinRT* project contains helpful WinRT runtime classes and methods to encapsulate a virtual camera and interact with them, as well as generate them and query for custom camera KSProperties (in our case either custom or extended). Refer to the 2 *.idl* files defining the API signatures, they provide a good base to expand upon: 
  - *VirtualCameraManager.idl*
  - *CustomCameraKsPropertyInquiry.idl*
- The project named *CameraKsPropertyHelper* is another project shared accross other samples in this repository to aid in interacting with known standard KSProperties from WinRT.

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
Check to ensure all dependencies required by the mediasource are registered.  Use dumpbin to find all the dependencies of the dll.
* Open Visual Studio command prompt (In Visual Studio ->select menu "Tools" -> Command line -> Developer Command prompt)
* run dumpbin 
  ```
  dumpbin /DEPENDENTS mediasource.dll
  ```

<b> 2. How to debug issues with media source? </b> <br/>
During VirtualCamera creation the mediaSource is loaded in the FrameServerMonitor service.  If you encounter an issue prior to starting the virtual camera <br/>
you will need to attach the debugger to the FrameServerMonitor service.
* Open task manager -> view tab "Services" -> look for the FrameServerMonitor service
* Attach debugger to the process
* Alternative: use the powershell script provided: scripts\Debug-FrameServerMonitor.ps1 

In the case where  the virtualCamera is used by application or after VirtualCamera has started, the media source is loaded in the FrameServer service. To investigate an issue with the mediasource you will need to attach the debugger to the FrameServer service.
* Open task manager -> view tab "Services" -> look for the FrameServer service 
* Attach debugger to the process
* Alternative: use the powershell script provided: scripts\Debug-FrameServer.ps1

<b> 3. Can I create a virtual camera that wraps another existing virtual camera with this sample </b><br/>this is not a supported scenario currently as it entails a lot of complexity and undesired behavior given the dependency chain.

<b> 4. In a UWP app, if I create a virtual camera on the UI thread without having been prompted for camera access first, the camera access prompt is unresponsive, what is going on? </b><br/>
If you are calling ```MFCreateVirtualCamera()``` (indirectly i.e. using the ```VirtualCameraRegistrar.RegisterNewVirtualCamera()``` method) on a UI thread, it will block it until camera access consent is given. However since that consent prompt is also running on the UI thread, it will actively block interaction and appear to freeze the app. A smart thing to do may be to either trigger consent upon launch of the app by another mean (i.e. calling ```MediaCapture.InitializeAsync()```) or by calling the API in a background thread (which the UWP VirtualCameraManager_App does).

<b> 5. Can 2 virtual cameras concurrently wrap the same existing camera on the system? </b><br/>
To avoid issue like a racing condition, only 1 virtual camera at a time can wrap and use an existing camera if properly identifying the physical camera using the *[IMFVirtualCamera::AddDeviceSourceInfo()](https://docs.microsoft.com/en-us/windows/win32/api/mfvirtualcamera/nf-mfvirtualcamera-imfvirtualcamera-adddevicesourceinfo)* API at virtual camera registration time. You should not either open an existing camera in sharing mode from within your virtual camera as there a lot of complication and undefined behavior associated with this such as seemingly random and undesired MediaType changes.

## Updates
----

07/04/2022 Critical fixes:
* Fix SetStreamState implementation on SimpleMediaSource
Change stream state to MF_STREAM_STATE_RUNNING will change state to running state (produce frame) but MEStreamStarted will not be send
Change stream state to MF_STREAM_STATE_STOPPED will stop stream, RequestSample will be rejected.
Change stream state to MF_STREAM_STATE_PAUSE will not stop the stream but RequestSample will be rejected.

* Fix GetSourceAttributes implementation
SourceAttributes must return a reference of the attributes (not copy).  The source is being use upstream components and any modification of the attribute store from upstream component must get preserve.

* MediaStream needs to allocate own workqueue

* Fix SimpleMediaSource::Start implementation
If stream was deselected, ensure stream is put into stop state.
When stream is reselected with the same mediatype, treat as no-op

* Update VirtualCameraMediaSource build to not link against vcruntime dynamically.



