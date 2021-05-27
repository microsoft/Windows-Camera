#pragma once

#include <windows.h>
#include <propvarutil.h>
#include <ole2.h>  // include unknown.h this must come before winrt header
#include <winerror.h>

#include <initguid.h>
#include <devpropdef.h>
#include "devpkey.h"
#include "cfgmgr32.h"

#include <Ks.h>
#include <ksproxy.h>
#include <ksmedia.h>
#include <mfapi.h>
#include <mfvirtualcamera.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <nserror.h>
#include <winmeta.h>
#include <d3d9types.h>

#include <iostream>

#define RESULT_DIAGNOSTICS_LEVEL 4 // include function name

#include <wil/cppwinrt.h> // must be before the first C++ WinRT header, ref:https://github.com/Microsoft/wil/wiki/Error-handling-helpers
#include <wil/result.h>
#include "wil/com.h"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Media.Capture.h>
#include <winrt/Windows.Media.Capture.Core.h>
#include <winrt/Windows.Media.Capture.Frames.h>
#include <winrt/Windows.Media.Devices.h>
#include <winrt/Windows.Media.Devices.Core.h>
#include <winrt/Windows.Media.MediaProperties.h>
#include <winrt/Windows.ApplicationModel.h>

namespace winrt
{
    using namespace winrt::Windows::Foundation;
}
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::Devices::Enumeration;
using namespace winrt::Windows::Media::Capture;
using namespace winrt::Windows::Media::Capture::Core;
using namespace winrt::Windows::Media::Capture::Frames;
using namespace winrt::Windows::Media::Devices;
using namespace winrt::Windows::Media::Core;
using namespace winrt::Windows::Media::MediaProperties;
using namespace winrt::Windows::ApplicationModel;

#pragma comment(lib, "windowsapp")
#pragma comment(lib, "mfuuid")
#pragma comment(lib, "Mfplat")
#pragma comment(lib, "Mf.lib")

#pragma comment(lib, "Cfgmgr32.lib")
#pragma comment(lib, "Propsys.lib")
 
#include "Logger.h"

