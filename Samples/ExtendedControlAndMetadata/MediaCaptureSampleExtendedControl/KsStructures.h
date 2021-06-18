//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

//
// KsStructures.h
//

#pragma once
#include <ks.h>
#include <ksmedia.h>

//
// The KS Structures needed for this sample are not defined for WinRT. We redefine them here, copying their definition
//  from ksmedia.h
//
#ifndef KSCAMERA_EXTENDEDPROP_HEADER
#define STATIC_KSPROPERTYSETID_ExtendedCameraControl \
     0x1CB79112, 0xC0D2, 0x4213, 0x9C, 0xA6, 0xCD, 0x4F, 0xDB, 0x92, 0x79, 0x72
DEFINE_GUIDSTRUCT("1CB79112-C0D2-4213-9CA6-CD4FDB927972", KSPROPERTYSETID_ExtendedCameraControl);
#define KSPROPERTYSETID_ExtendedCameraControl DEFINE_GUIDNAMED(KSPROPERTYSETID_ExtendedCameraControl)

typedef struct tagKSCAMERA_EXTENDEDPROP_HEADER {
	ULONG               Version;
	ULONG               PinId;
	ULONG               Size;
	ULONG               Result;
	ULONGLONG           Flags;
	ULONGLONG           Capability;
} KSCAMERA_EXTENDEDPROP_HEADER, *PKSCAMERA_EXTENDEDPROP_HEADER;

typedef struct tagKSCAMERA_EXTENDEDPROP_VALUE {
	union
	{
		double          dbl;
		ULONGLONG       ull;
		ULONG           ul;
		ULARGE_INTEGER  ratio;
		LONG            l;
		LONGLONG        ll;
	} Value;
} KSCAMERA_EXTENDEDPROP_VALUE, *PKSCAMERA_EXTENDEDPROP_VALUE;

typedef struct tagKSCAMERA_EXTENDEDPROP_VIDEOPROCSETTING {
	ULONG                               Mode;
	LONG                                Min;
	LONG                                Max;
	LONG                                Step;
	KSCAMERA_EXTENDEDPROP_VALUE         VideoProc;
	ULONGLONG                           Reserved;
} KSCAMERA_EXTENDEDPROP_VIDEOPROCSETTING, *PKSCAMERA_EXTENDEDPROP_VIDEOPROCSETTING;

#define KSCAMERA_EXTENDEDPROP_OIS_OFF                                       0x0000000000000000
#define KSCAMERA_EXTENDEDPROP_OIS_ON                                        0x0000000000000001
#define KSCAMERA_EXTENDEDPROP_OIS_AUTO                                      0x0000000000000002

#define KSCAMERA_EXTENDEDPROP_VIDEOPROCFLAG_AUTO                        0x0000000000000001
#define KSCAMERA_EXTENDEDPROP_VIDEOPROCFLAG_MANUAL                      0x0000000000000002
#define KSCAMERA_EXTENDEDPROP_VIDEOPROCFLAG_LOCK                        0x0000000000000004
#endif