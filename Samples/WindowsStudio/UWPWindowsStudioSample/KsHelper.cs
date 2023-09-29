// Copyright (c) Microsoft. All rights reserved.

using System;
using System.Runtime.InteropServices;
using Windows.Media.Devices;

namespace UWPWindowsStudioSample
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
            public UInt64 Flags;
            public UInt64 Capability;
        };

        [StructLayout(LayoutKind.Sequential)]
        public struct KSCAMERA_EXTENDEDPROP_VALUE
        {
            public UInt64 Value;
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
        #endregion KsMedia
        //
        // end of redefinition of constant values and structures defined ksmedia.h 

        /// <summary>
        /// Helper method to send a GET call for a camera extended control DDI and retrieve the resulting payload
        /// </summary>
        /// <param name="controller"></param>
        /// <param name="controlKind"></param>
        /// <returns></returns>
        /// <exception cref="Exception"></exception>
        public static byte[] GetExtendedControlPayload(VideoDeviceController controller, KsHelper.ExtendedControlKind controlKind)
        {
            // create a KsProperty for the specified extended control
            var ksProp = new KsProperty(
               KsHelper.KSPROPERTYSETID_ExtendedCameraControl,
               (uint)controlKind,
               (uint)KsProperty.KsPropertyKind.KSPROPERTY_TYPE_GET);
            var bytePayload = KsHelper.ToBytes(ksProp);

            // send a GET command with the KsProperty and retrieve the result
            var resultPayload = controller.GetDevicePropertyByExtendedId(bytePayload, null);
            if (resultPayload == null)
            {
                throw new Exception($"Unexpectedly could not GET payload for control={controlKind}, null payload");
            }
            if (resultPayload.Status != VideoDeviceControllerGetDevicePropertyStatus.Success)
            {
                throw new Exception($"Unexpectedly could not GET payload for control={controlKind}, status={resultPayload.Status}");
            }
            return (byte[])resultPayload.Value;
        }

        /// <summary>
        /// Helper method to send a SET call for a camera extended control DDI with the specified flags value
        /// </summary>
        /// <param name="controller"></param>
        /// <param name="controlKind"></param>
        /// <param name="flags"></param>
        /// <exception cref="Exception"></exception>
        public static void SetExtendedControlFlags(
            VideoDeviceController controller, 
            KsHelper.ExtendedControlKind controlKind,
            ulong flags)
        {
            // create a KsProperty for the specified extended control
            var ksProp = new KsProperty(
               KsHelper.KSPROPERTYSETID_ExtendedCameraControl,
               (uint)controlKind,
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
                throw new Exception($"Unexpectedly could not SET flags={flags} for control={controlKind}, status={resultStatus}");
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
        public static T FromBytes<T>(byte[] bytes, int startIndex = 0)
        {
            var size = Marshal.SizeOf<T>();
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
    }
}
