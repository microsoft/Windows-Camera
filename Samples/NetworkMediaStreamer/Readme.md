
# NetworkMediaStreamer
## Introduction
The NetworkMediaStreamer is a set of basic implementations of RTP streaming and RTSP server to demonstrate usage of [Windows Media Foundation APIs](https://docs.microsoft.com/en-us/windows/win32/medfound/media-foundation-programming-guide) related to-
 1. Using and configuring OS built-in codecs
 2. Consuming samples/frames from the MediaFoundation stack of APIs
 
 The implementation also demonstrates -
 1. Using Windows APIs for network to implement RTP video streaming and RTSP server , and using [schannel APIs](https://docs.microsoft.com/en-us/windows/win32/com/schannel) for secure RTSP.
 2. Using credential store using [PasswordVault APIs](https://docs.microsoft.com/en-us/uwp/api/windows.security.credentials.passwordvault?view=winrt-19041) 

 This base implementation suports H264 RTP via RTSP . The code can be extended very easily to support more RTP payloads and other protocols as well. Refer to this figure to understand the interaction between various components.

![NetworkStreamer Block Diagram](docs\RTSPVideoStreamer.jpg)
 ## RTPSink
 The RTP streaming is implemented as a COM class which implements [IMFMediaSink](https://docs.microsoft.com/en-us/windows/win32/api/mfidl/nn-mfidl-imfmediasink). This class encapsulates the Network media stream sink which does the actual packetization work and udp streaming. The NetworkMediaStream sink can be controlled via two interfaces -
 1. [IMFStreamSink](https://docs.microsoft.com/en-us/windows/win32/api/mfidl/nn-mfidl-imfstreamsink)
    - negotiating/setting media types and streaming states
    - supplying encoded samples to the sink.
 2. [INetworkMediaStreamSink] 
    - adding network desitnation end points for streaming
    - adding custom packet transport handlers

### Creating an RTPSink
The RTPSink instance can be created using the factory method

---
```
 HRESULT CreateRTPMediaSink( 
    IMFMediaType** apMediaTypes,
    DWORD          dwMediaTypeCount, 
    IMFMediaSink** ppMediaSink)
``` 
#### Arguments  
apMediaTypes
: Input array of MFMediaType objects which describe each stream of the sink  
dwMediaTypeCount
: Input number of Media types in the array (number of streams to be created)  
ppMediaSink
: Output pointer to the instance of the RTP MediaSink

#### Returns  
HRESULT 
: S_OK if succeeded. HRESULT error if failed. 

---

### Feeding samples/video to the RTPSink
This can be acheived using one of the following (but not limited to) options
1. Use [MFCreateSinkWriterFromMediaSink](https://docs.microsoft.com/en-us/windows/win32/api/mfreadwrite/nf-mfreadwrite-mfcreatesinkwriterfrommediasink) and write samples using the obtained [IMFSinkWriter](https://docs.microsoft.com/en-us/windows/win32/api/mfreadwrite/nn-mfreadwrite-imfsinkwriter) interface
2. Using a [Media Session](https://docs.microsoft.com/en-us/windows/win32/medfound/media-session)
3. Streaming Video from Camera using [Mediacapture](https://docs.microsoft.com/en-us/uwp/api/Windows.Media.Capture.MediaCapture?view=winrt-19041) and [Record to custom Sink](https://docs.microsoft.com/en-us/uwp/api/windows.media.capture.mediacapture.preparelowlagrecordtocustomsinkasync?view=winrt-19041)


## RTSP Server
The RTSP server control implements RTSP protocol to negotiate and setup RTP streaming to the clients from the RTPSink instances it holds. 
The RTSP Server controls the network side interface (INetworkMediaStreamSink) for all the sinks that it controls.
An instance of RTSP Server can be created by using the factory method-  

---
```
 HRESULT CreateRTSPServer(
    ABI::RTSPSuffixSinkMap *streamers,
    uint16_t socketPort,
    bool bSecure,
    IRTSPAuthProvider* pAuthProvider,
    PCCERT_CONTEXT* serverCerts,
    size_t uCertCount,
    IRTSPServerControl** ppRTSPServerControl);

``` 
#### Arguments  
streamers
: Input map of MediaSink objects as IMediaExtention interface and the corrosponsing url suffix.    

socketPort
: Input socket port on which the Server wil listen for RTSP requests.  

bSecure
: Input bool flag indicating if the server uses secure tcp connection  

pAuthProvider
: Input pointer to the IRTSPAuthProvider interface to the authentication provider to be used by the server  

serverCerts
: Input array of server certificate contexts to use for sercure tcp tls.  

uCertCount
: Input number of certificate contexts in the array

ppRTSPServerControl  
: Output pointer to the IRTSPServerControl interface to the RTSP server instance

#### Returns  
HRESULT 
: S_OK if succeeded. HRESULT error if failed. 

---

## Authentication provider

The authentication provider implements the following interfaces 
1. IRTSPAuthProvider - This interface is used by the RTSP server to authenticate incoming client connections
2. IRTSPAuthProviderCredStore - This interface is used by the administrative module of the app to add and remove credentials etc.

As base implentation of AuthProvider is included in the source. An instance to the authentication provider can be created using -


---
```
 HRESULT GetAuthProviderInstance(
     AuthType authType,
    LPCWSTR resourceName,
    IRTSPAuthProvider** ppRTSPAuthProvider);
``` 
#### Arguments  
AuthType
: Input array of MFMediaType objects which describe each stream of the sink  

resourceName
: Input resource name to which the credentials are bound

ppRTSPAuthProvider  
: Output pointer to the IRTSPAuthProvider interface to the auth provider instance

#### Returns  
HRESULT 
: S_OK if succeeded. HRESULT error if failed. 

---