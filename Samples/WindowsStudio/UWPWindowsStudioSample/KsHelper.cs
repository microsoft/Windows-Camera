using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace UWPWindowsStudioSample
{
    class KsHelper
    {
        public static readonly Guid KSPROPERTYSETID_ExtendedCameraControl =
                Guid.Parse("1CB79112-C0D2-4213-9CA6-CD4FDB927972");

        // refer to KSPROPERTY_CAMERACONTROL_EXTENDED_PROPERTY
        public enum ExtendedControlKind : uint
        {
            KSPROPERTY_CAMERACONTROL_EXTENDED_EYEGAZECORRECTION = 40,
            KSPROPERTY_CAMERACONTROL_EXTENDED_BACKGROUNDSEGMENTATION = 41,
        };

        // A clunky method to marshal a structure into a byte[]
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

        // A clunky method to marshal a byte[] into a structure
        public static T FromBytes<T>(byte[] bytes, int startIndex = 0)
        {
            var size = Marshal.SizeOf<T>();
            var intPtr = Marshal.AllocHGlobal(size);
            Marshal.Copy(bytes, startIndex, intPtr, size);
            var item = Marshal.PtrToStructure<T>(intPtr);
            Marshal.FreeHGlobal(intPtr);
            return item;
        }
    }

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
    public struct KspNode
    {
        public KsProperty Property;
        public uint NodeId;
        public uint Reserved;

        public KspNode(KsProperty property, uint nodeId = 0)
        {
            property.Flags |= (uint)KsProperty.KsPropertyKind.KSPROPERTY_TYPE_TOPOLOGY;
            Property = property;
            NodeId = nodeId;
            Reserved = 0;
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
}
