<!---
  category: AudioVideoAndCamera
  samplefwlink: http://go.microsoft.com/fwlink/p/?LinkId=853072
--->

# Sample App targeting the use of windows compliant 360-degree Camera 

Shows how to support preview, video record and photo capture scenarios with a 360 camera on Windows.
A windows supported 360 camera is a camera with one or many lenses/sensors and a driver package (driver + DeviceMFT) that provides stitched Equirectangular frames,
and has appropriate spherical attributes set on stream and mediatype.

The sample uses the MediaPlayer class and MediaPlayerElement xaml element for 360-degree preview.
The MediaPlayer handles all aspects of the rendering.
MediaPlaybackSphericalVideoProjection member of MediaPlayer class handles the projection from Equirectangular frames output of the camera to a viewport.
The developer only needs to provide the look direction.
The sample uses MediaCapture Class for accessing the camera for preview,video record and photo capture.
The MediaCapture class handles the addition of appropriate spherical metadata on the captured photos and recorded files.


**Note:** This sample is part of a collection app samples for windows camera 

Specifically, this samples covers:

- Creating an application to use a windows supported 360/spherical camera.
- Demonstrating preview switching from spherical to regular camera based on spherical format properties.
- Demonstrating under-the-hood spherical metadata handling with the mediacapture class for video record and photo capture
- Sample implementation of UI controls to pan/control the spherical preview.


## Additional remarks

To obtain information about Windows 10 development, go to the [Windows Dev Center](http://go.microsoft.com/fwlink/?LinkID=532421).

### Reference
[MediaPlayer](https://docs.microsoft.com/en-us/uwp/api/windows.media.playback.mediaplayer)  
[MediaPlayerElement](https://docs.microsoft.com/en-us/uwp/api/windows.ui.xaml.controls.mediaplayerelement)
[MediaPlaybackSphericalVideoProjection](https://docs.microsoft.com/en-us/uwp/api/windows.media.playback.mediaplaybacksphericalvideoprojection)
[MediaCapture](https://docs.microsoft.com/en-us/uwp/api/windows.media.capture.mediacapture)

## System requirements
    Windows 10 build 17134,

## Build the sample

1. If you download the samples ZIP, be sure to unzip the entire archive, not just the folder with
   the sample you want to build.
2. Start Microsoft Visual Studio 2017 and select **File** \> **Open** \> **Project/Solution**.
3. Starting in the folder where you unzipped the samples, go to the Samples subfolder, then the
   subfolder for this specific sample, then the subfolder for your preferred language (C++, C#, or
   JavaScript). Double-click the Visual Studio Solution (.sln) file.
4. Press Ctrl+Shift+B, or select **Build** \> **Build Solution**.

## Run the sample

The next steps depend on whether you just want to deploy the sample or you want to both deploy and
run it.

### Deploying the sample

- Select Build > Deploy Solution. 

### Deploying and running the sample

- To debug the sample and then run it, press F5 or select Debug >  Start Debugging. To run the sample without debugging, press Ctrl+F5 or selectDebug > Start Without Debugging. 
