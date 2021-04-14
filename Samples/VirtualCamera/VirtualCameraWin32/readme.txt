========================================================================
About the Project
========================================================================
Virtual camera MediaSource dll implemented using cppwinrt

Getting started:
To configure this project to build, you must have access to OS repo 
and reference branch with the private / unpublish APIs. 
See step 3 in "Project Setup" to configure the project to reference
OS repo.

WARNING:
    Please stick with x64 only, project is not properly configure for x86!

========================================================================
Project Setup
========================================================================
1. include cppwinrt and wil nudget
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


========================================================================
NOTEs
========================================================================
Author COM with C++/Winrt
Reference: https://docs.microsoft.com/en-us/windows/uwp/cpp-and-winrt-apis/author-coclasses


Interface chaining
Reference: https://devblogs.microsoft.com/oldnewthing/20200424-00/?p=103702
* define: overload specialize is_guid_of function  (before the declaration of the class)
namespace winrt
{
    template<> bool is_guid_of<IMFMediaSourceEx>(guid const& id) noexcept
    {
        return is_guid_of<IMFMediaSourceEx, IMFMediaSource, IMFMediaEventGenerator>(id);
    }
}

NOTE: update linker command with /FORCE:MULTIPLE to fix linker error