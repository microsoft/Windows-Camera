========================================================================
About the Project
========================================================================
Basic console application that validate virtual camera source implementation, 
register as virtual camera, remove virtual camera, validate virtual camera works 
thru mediacapture API

The mediasource used by the project as followed: 
 VirtualCameraWin32
 * A dll implemented with cppwinrt.

 VirutlaCameraWinRT
 * A winrt implementation of the MediaSource.

    UPDATE: This implementation is NOT RECOMMENEDED, winrt component has dependency 
    on VCRUNTIMExxx_APP.dll etc.. that is not in the system by default.
    The mediasource will fail to load due to missing dependency

Getting Started:
    To configure this project to build, you must have access to OS repo 
    and reference branch with the private / unpublish APIs. 
    See step 3 in "Project Setup" to configure the project to reference
    OS repo.

WARNING:
    Please stick with x64 only, project is not properly configure for x86!

========================================================================
Project Setup - working with internal/non-publish API
========================================================================

1. Add Wil nudget 
2. Configure project to reference internal/unpublish interfaces

    Reference the latest headers from OS repo: project -> right click -> select property -> C\C++
        $(OSRoot)\$(ProcessorArchitecture)fre.nocil\onecore\internal\avcore\inc;
        $(OSRoot)\$(ProcessorArchitecture)fre.nocil\onecoreuap\private\avcore\inc;
        $(OSRoot)\$(ProcessorArchitecture)fre.nocil\onecoreuap\internal\avcore\inc;
        $(OSRoot)\$(ProcessorArchitecture)fre.nocil\onecore\external\shared\inc;
        $(OSRoot)\$(ProcessorArchitecture)fre.nocil\onecoreuap\external\sdk\inc;
        %(AdditionalIncludeDirectories)

    Reference the latest lib from OS repro: project -> right click -> select property -> Linker
        These will reference libs from OS repo
        $(OSRoot)\$(ProcessorArchitecture)fre.nocil\onecoreuap\private\avcore\lib\amd64\mfsensorgroupp.lib;
        %(AdditionalDependencies)


    $(OSRoot)\$(PlatformArchitecture)fre.nocil  == K:\os.2020\public\amd64fre.nocil
    * see step 3 in updating macro

3. Add/Update custom macro

    * open propertySheet.props
    * find the following entry and update the value "K:\os.2020\public\" to the value you need.

      <PropertyGroup Label="UserMacros">
        <OSROOT>K:\os.2020\public\</OSROOT>  <-- update this to a different os repo
      </PropertyGroup>

4. Reference SimpleMediaSource Winrt
    * RegFree winrt component
        Reference: https://blogs.windows.com/windowsdeveloper/2019/04/30/enhancing-non-packaged-desktop-apps-using-windows-runtime-components/
        https://github.com/microsoft/RegFree_WinRT/tree/master/Cpp

         *?? add a reference to the WinRTComponent (right click the project ->| Add | Reference | Projects | WinRTComponent). 
         Adding the reference also ensures every time we build our app, the component is also built to keep track of any new changes in the component.

         * Update propertysheets to include winmd and dll to the project 
         This will ensure that the winmd and dll get copy to project build output

         * right click | Add | New Item | Visual C# | Application Manifest File. The manifest file naming convention is that it must have the same name as our application’s .exe and 
         have the .manifest extension, in this case I named it “WinFormsApp.exe.manifest”. 

         * app nuget: Microsoft.VCRTForwarders.140 

     Win32 dll
         * Update propertysheets to include dll to the project 
         This will ensure that the dll get copy to project build output

========================================================================
Project Setup - Publishing console application as msix
========================================================================
1. Follow instruction to create msix packaging project
https://docs.microsoft.com/en-us/windows/msix/desktop/desktop-to-uwp-packaging-dot-net


========================================================================
Project Setup - Publishing console application as msi
========================================================================
Visual studio MSI project extension
https://marketplace.visualstudio.com/items?itemName=visualstudioclient.MicrosoftVisualStudio2017InstallerProjects

https://docs.microsoft.com/en-us/cpp/windows/walkthrough-deploying-a-visual-cpp-application-by-using-a-setup-project?view=msvc-160&viewFallbackFrom=vs-2019
https://marketplace.visualstudio.com/items?itemName=VisualStudioClient.MicrosoftVisualStudio2017InstallerProjects

Add binaries, 
1. Right-click on the "Setup Project", go on "Add" then click "Assembly".

2. Browse for your COM assembly (.dll) and add it to the "Setup Project".

3. After adding the assembly, right-click on it in "Solution Explorer" and click "Properties".

4. Look for "Register" property and set it to "vsdraCOM".

Add registry
1. Right-click on the Project, select "View" then "Registry"
2. Add these keys and values
HKLM\software\classes\CLSID\<CLSID_GUID>
HKLM\software\classes\CLSID\<CLSID_GUID>\InProcServer32
HKLM\software\classes\CLSID\<CLSID_GUID>\InProcServer32\Default    value:[TARGETDIR]<MediaSourceDll>
HKLM\software\classes\CLSID\<CLSID_GUID>\InProcServer32\Threading  value:"Both"


Reference: Add registry entry in msi
https://docs.microsoft.com/en-us/windows/win32/msi/registry-table



Create MediaPlayer to render source
https://docs.microsoft.com/en-us/windows/win32/medfound/how-to-play-unprotected-media-files
Sample code: https://docs.microsoft.com/en-us/windows/win32/medfound/media-session-playback-example