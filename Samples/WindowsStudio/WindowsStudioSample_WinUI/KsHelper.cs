// Copyright (c) Microsoft. All rights reserved.

using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using Windows.Graphics.Imaging;
using Windows.Media.Capture.Frames;
using Windows.Media.Devices;
using WinRT;

[ComImport]
[System.Runtime.InteropServices.Guid("5b0d3235-4dba-4d44-865e-8f1d0e4fd04d")]
[InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
unsafe interface IMemoryBufferByteAccess
{
    void GetBuffer(out byte* buffer, out uint capacity);
}

namespace WindowsStudioSample_WinUI
{
    class KsHelper
    {
        // redefinition of certain constant values and structures defined in win32 in ksmedia.h 
        //
        #region KsMedia
        public static readonly Guid KSPROPERTYSETID_ExtendedCameraControl =
                Guid.Parse("1CB79112-C0D2-4213-9CA6-CD4FDB927972");

        private const uint KSCAMERA_EXTENDEDPROP_FILTERSCOPE = 0xFFFFFFFF;

        // refer to KSPROPERTY_CAMERACONTROL_EXTENDED_PROPERTY
        public enum ExtendedControlKind : uint
        {
            KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION = 40,
            KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION = 41,
            KSPROPERTY_CAMERACONTROL_EXTENDED_DIGITALWINDOW = 43
        };

        public enum BackgroundSegmentationCapabilityKind : uint
        {
            KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_OFF = 0,
            KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_BLUR = 1,
            KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_MASK = 2,
            KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_SHALLOWFOCUS = 4
        }

        public enum EyeGazeCorrectionCapabilityKind : uint
        {
            KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_OFF = 0,
            KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_ON = 1,
            KSCAMERA_EXTENDEDPROP_EYEGAZECORRECTION_STARE = 2,
        };

        // --> Windows Studio Effects custom KsProperties
        public static readonly Guid KSPROPERTYSETID_WindowsCameraEffect = 
            Guid.Parse("1666d655-21A6-4982-9728-52c39E869F90");

        // Custom KsProperties exposed in version 2
        public enum KSPROPERTY_CAMERACONTROL_WINDOWS_EFFECTS : uint
        {
            KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_SUPPORTED = 0,
            KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_STAGELIGHT = 1,
            KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_CREATIVEFILTER = 2,
            KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_SETNOTIFICATION = 3,
            KSPROPERTY_CAMERACONTROL_WINDOWSSTUDIO_PERFORMANCEMITIGATION = 4
        };

        public enum StageLightCapabilityKind : uint
        {
            KSCAMERA_WINDOWSSTUDIO_STAGELIGHT_OFF = 0,
            KSCAMERA_WINDOWSSTUDIO_STAGELIGHT_ON = 1
        };

        public enum CreativeFilterCapabilityKind : uint
        {
            KSCAMERA_WINDOWSSTUDIO_CREATIVEFILTER_OFF = 0,
            KSCAMERA_WINDOWSSTUDIO_CREATIVEFILTER_ILLUSTRATED = 1,
            KSCAMERA_WINDOWSSTUDIO_CREATIVEFILTER_ANIMATED = 2,
            KSCAMERA_WINDOWSSTUDIO_CREATIVEFILTER_WATERCOLOR = 4
        };
        // <-- Windows Studio Effects custom KsProperties

        [StructLayout(LayoutKind.Sequential)]
        public struct KsProperty
        {
            public enum KsPropertyKind : uint
            {
                KSPROPERTY_TYPE_GET = 0x00000001,
                KSPROPERTY_TYPE_SET = 0x00000002,
                KSPROPERTY_TYPE_TOPOLOGY = 0x10000000
            };

            [MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)]
            public byte[] Set;
            public uint Id;
            public uint Flags;

            public KsProperty(Guid set, uint id = 0, uint flags = 0) : this()
            {
                Set = set.ToByteArray();
                Id = id;
                Flags = flags;
            }
        };

        [StructLayout(LayoutKind.Sequential)]
        public struct KSCAMERA_EXTENDEDPROP_HEADER
        {
            public uint Version;
            public uint PinId;
            public uint Size;
            public uint Result;
            public ulong Flags;
            public ulong Capability;
        };

        [StructLayout(LayoutKind.Sequential)]
        public struct KSCAMERA_EXTENDEDPROP_VALUE
        {
            public ulong Value;
        };

        // wrapper for a basic camera extended property payload
        [StructLayout(LayoutKind.Sequential)]
        public struct KsBasicCameraExtendedPropPayload
        {
            public KSCAMERA_EXTENDEDPROP_HEADER header;
            public KSCAMERA_EXTENDEDPROP_VALUE value;
        };

        [StructLayout(LayoutKind.Sequential)]
        public struct KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_CONFIGCAPS
        {
            public int MaskResolutionX;
            public int MaskResolutionY;
            public int MaxFrameRateNumerator;
            public int MaxFrameRateDenominator;
            public int ResolutionX;
            public int ResolutionY;
            public Guid SubType;
        };

        // wrapper for a background segmentation camera extended property payload
        [StructLayout(LayoutKind.Sequential)]
        public struct KsBackgroundCameraExtendedPropPayload
        {
            public KSCAMERA_EXTENDEDPROP_HEADER header;
            public KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_CONFIGCAPS[] value;
        };

        [StructLayout(LayoutKind.Sequential)]
        public struct KSCAMERA_METADATA_ITEMHEADER
        {
            public uint MetadataId;
            public uint Size;         // Size of this header + metadata payload following
        };

        [StructLayout(LayoutKind.Sequential)]
        public struct RECT
        {
            public int left;
            public int top;
            public int right;
            public int bottom;
        };

        [StructLayout(LayoutKind.Sequential)]
        public struct SIZE
        {
            public int cx;
            public int cy;
        };

        // wrapper for a background segmentation mask metadata

        // MFSampleExtension_CaptureMetadata
        public static readonly Guid MFSampleExtension_CaptureMetadata = new Guid(
            0x2EBE23A8, 0xFAF5, 0x444A, 0xA6, 0xA2, 0xEB, 0x81, 0x08, 0x80, 0xAB, 0x5D);
        
        // MF_CAPTURE_METADATA_FRAME_BACKGROUND_MASK
        public static readonly Guid MF_CAPTURE_METADATA_FRAME_BACKGROUND_MASK = new Guid(
            0x3f14dd3, 
            0x75dd, 
            0x433a, 
            new byte[]{ 0xa8, 0xe2, 0x1e, 0x3f, 0x5f, 0x2a, 0x50, 0xa0 }); 

        [StructLayout(LayoutKind.Sequential)]
        public struct KSCAMERA_METADATA_BACKGROUNDSEGMENTATIONMASK
        {
            public KSCAMERA_METADATA_ITEMHEADER Header;
            public RECT MaskCoverageBoundingBox;
            public SIZE MaskResolution;
            public RECT ForegroundBoundingBox;
            //public byte[] MaskData;
        };

        // Lookup table to match a known camera profile ID with a legible name
        public static readonly Dictionary<string, string> CameraProfileIdLUT =
            new Dictionary<string, string>()
            {
                { "{B4894D81-62B7-4EEC-8740-80658C4A9D3E}", "Legacy" }, // KSCAMERAPROFILE_Legacy
                { "{A0E517E8-8F8C-4F6F-9A57-46FC2F647EC0}", "VideoRecording" }, // KSCAMERAPROFILE_VideoRecording
                { "{32440725-961B-4CA3-B5B2-854E719D9E1B}", "HighQualityPhoto" }, // KSCAMERAPROFILE_HighQualityPhoto
                { "{6B52B017-42C7-4A21-BFE3-23F009149887}", "BalancedVideoAndPhoto" }, // KSCAMERAPROFILE_BalancedVideoAndPhoto
                { "{C5444A88-E1BF-4597-B2DD-9E1EAD864BB8}", "VideoConferencing" }, // KSCAMERAPROFILE_VideoConferencing
                { "{02399D9D-4EE8-49BA-BC07-5FF156531413}", "PhotoSequence" }, // KSCAMERAPROFILE_PhotoSequence
                { "{566E6113-8C35-48E7-B89F-D23FDC1219DC}", "HighFrameRate" }, // KSCAMERAPROFILE_HighFrameRate
                { "{9FF2CB56-E75A-49B1-A928-9985D5946F87}", "VariablePhotoSequence" }, // KSCAMERAPROFILE_VariablePhotoSequence
                { "{D4F3F4EC-BDFF-4314-B1D4-008E281F74E7}", "VideoHDR8" }, // KSCAMERAPROFILE_VideoHDR8
                { "{81361B22-700B-4546-A2D4-C52E907BFC27}", "FaceAuth_Mode" }, // KSCAMERAPROFILE_FaceAuth_Mode
                { "{E4ED96D9-CD40-412F-B20A-B7402A43DCD2}", "WSENoEffectsColorPassthrough" }, // KSCAMERAPROFILE_WindowsStudioNoEffectsColorPassthrough
            };

        #endregion KsMedia
        //
        // end of redefinition of constant values and structures defined ksmedia.h 

        /// <summary>
        /// Helper method to send a GET call for a camera extended control DDI and retrieve the resulting payload
        /// </summary>
        /// <param name="controller"></param>
        /// <param name="propertySet"></param>
        /// <param name="controlId"></param>
        /// <returns></returns>
        /// <exception cref="Exception"></exception>
        public static byte[] GetExtendedControlPayload(
            VideoDeviceController controller,
            Guid propertySet,
            uint controlId)
        {
            // create a KsProperty for the specified extended control
            var ksProp = new KsProperty(
               propertySet,
               controlId,
               (uint)KsProperty.KsPropertyKind.KSPROPERTY_TYPE_GET);
            var bytePayload = KsHelper.ToBytes(ksProp);

            // send a GET command with the KsProperty and retrieve the result
            VideoDeviceControllerGetDevicePropertyResult resultPayload = controller.GetDevicePropertyByExtendedId(bytePayload, null);
            if (resultPayload == null)
            {
                throw new Exception($"Unexpectedly could not GET payload for propertySet={propertySet} and control={controlId}, null payload");
            }
            if (resultPayload.Status != VideoDeviceControllerGetDevicePropertyStatus.Success)
            {
                throw new Exception($"Unexpectedly could not GET payload for propertySet={propertySet} and control={controlId}, status={resultPayload.Status}");
            }
            Object resultPayloadValue = resultPayload.Value;
            if (resultPayloadValue != null)
            {
                return (byte[])resultPayload.Value;
            }
            else
            {
                throw new Exception($"null value");
            }
        }

        /// <summary>
        /// Helper method to send a SET call for a camera extended control DDI with the specified flags value
        /// </summary>
        /// <param name="controller"></param>
        /// <param name="propertySet"></param>
        /// <param name="controlId"></param>
        /// <param name="flags"></param>
        /// <exception cref="Exception"></exception>
        public static void SetExtendedControlFlags(
            VideoDeviceController controller,
            Guid propertySet,
            uint controlId,
            ulong flags)
        {
            // create a KsProperty for the specified extended control
            var ksProp = new KsProperty(
            propertySet,
               controlId,
               (uint)KsProperty.KsPropertyKind.KSPROPERTY_TYPE_SET);
            var byteKsProp = KsHelper.ToBytes(ksProp);

            // create a payload for the specified extended control that includes the specified flags value
            var payload = new KsBasicCameraExtendedPropPayload()
            {
                header = new KSCAMERA_EXTENDEDPROP_HEADER()
                {
                    Flags = flags,
                    Capability = 0,
                    Size = (uint)System.Runtime.InteropServices.Marshal.SizeOf(typeof(KsBasicCameraExtendedPropPayload)),
                    Version = 1,
                    PinId = KSCAMERA_EXTENDEDPROP_FILTERSCOPE
                },
                value = new KSCAMERA_EXTENDEDPROP_VALUE()
                {
                    Value = 0
                }
            };
            var bytePayload = KsHelper.ToBytes(payload);

            // send a SET command with the KsProperty and payload and retrieve the result status
            var resultStatus = controller.SetDevicePropertyByExtendedId(byteKsProp, bytePayload);
            
            if (resultStatus != VideoDeviceControllerSetDevicePropertyStatus.Success)
            {
                throw new Exception($"Unexpectedly could not SET flags={flags} for propertySet={propertySet} and control={controlId}, status={resultStatus}");
            }
        }

        // A method to marshal a structure into a byte[]
        public static byte[] ToBytes<T>(T item)
        {
            var size = Marshal.SizeOf<T>();
            var intPtr = Marshal.AllocHGlobal(size);
            var bytes = new byte[size];
            Marshal.StructureToPtr<T>(item, intPtr, false);
            Marshal.Copy(intPtr, bytes, 0, size);
            Marshal.FreeHGlobal(intPtr);
            return bytes;
        }

        // A method to marshal a byte[] into a structure
        public static T FromBytes<T>(byte[] bytes, int size = -1, int startIndex = 0)
        {
            if (size == -1)
            {
                size = Marshal.SizeOf<T>();
            }
            var intPtr = Marshal.AllocHGlobal(size);
            Marshal.Copy(bytes, startIndex, intPtr, size);
            var item = Marshal.PtrToStructure<T>(intPtr);
            Marshal.FreeHGlobal(intPtr);
            return item;
        }

        // Specialization of the above generic method to deserialize properly a variably sized payload
        // for background segmentation
        public static KsBackgroundCameraExtendedPropPayload FromBytes(byte[] bytes, int startIndex = 0)
        {
            KsBackgroundCameraExtendedPropPayload payload = new KsBackgroundCameraExtendedPropPayload();
            var headerSize = Marshal.SizeOf<KSCAMERA_EXTENDEDPROP_HEADER>();
            var backgroundCapsSize = Marshal.SizeOf<KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_CONFIGCAPS>();
            int capabilityCount = (bytes.Length - headerSize) / backgroundCapsSize;
            payload.value = new KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_CONFIGCAPS[capabilityCount];

            // deserizalize the header portion of the payload
            {
                var intPtr = Marshal.AllocHGlobal(headerSize);
                Marshal.Copy(bytes, startIndex, intPtr, headerSize);
                payload.header = Marshal.PtrToStructure<KSCAMERA_EXTENDEDPROP_HEADER>(intPtr);
                Marshal.FreeHGlobal(intPtr);
                startIndex += headerSize;
            }
            // deserizalize the background segmentation caps portion of the payload
            for (int i = 0; i < capabilityCount; i++)
            {
                var intPtr = Marshal.AllocHGlobal(backgroundCapsSize);
                Marshal.Copy(bytes, startIndex, intPtr, backgroundCapsSize);
                payload.value[i] = Marshal.PtrToStructure<KSCAMERA_EXTENDEDPROP_BACKGROUNDSEGMENTATION_CONFIGCAPS>(intPtr);
                Marshal.FreeHGlobal(intPtr);
                startIndex += backgroundCapsSize;
            }

            return payload;
        }

        public static SoftwareBitmap ExtractBackgroundSegmentationMaskMetadataAsBitmap(MediaFrameReference frame)
        {
            SoftwareBitmap maskSoftwareBitmap = null;
            if (frame.Properties.TryGetValue(MFSampleExtension_CaptureMetadata, out var captureMetadata))
            {
                if (captureMetadata is IReadOnlyDictionary<Guid, object> captureMetadataLookUp)
                {
                    if (captureMetadataLookUp.TryGetValue(MF_CAPTURE_METADATA_FRAME_BACKGROUND_MASK, out var backgroundSegmentationMask))
                    {
                        byte[] payloadBytes = (byte[])backgroundSegmentationMask;
                        KSCAMERA_METADATA_BACKGROUNDSEGMENTATIONMASK payload = FromBytes<KSCAMERA_METADATA_BACKGROUNDSEGMENTATIONMASK>(payloadBytes);
                        maskSoftwareBitmap = new SoftwareBitmap(BitmapPixelFormat.Gray8, payload.MaskResolution.cx, payload.MaskResolution.cy);
                        using var buffer = maskSoftwareBitmap.LockBuffer(BitmapBufferAccessMode.Write);
                        using var reference = buffer.CreateReference();
                        if (reference == null)
                        {
                            throw new Exception("Cannot access pixel buffer of mask frame");
                        }

                        unsafe
                        {
                            byte* gray8Data;
                            uint gray8Size;
                            reference.As<IMemoryBufferByteAccess>().GetBuffer(out gray8Data, out gray8Size);
                            Marshal.Copy(payloadBytes, Marshal.SizeOf(payload), (IntPtr)gray8Data, (int)gray8Size);
                        }
                    }
                }
            }
            return maskSoftwareBitmap;

        }
    }
}
