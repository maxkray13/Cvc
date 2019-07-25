#pragma once
#include "../../Declaration.h"
/*
Cvc - Communication via callback function prefix
*/

NTSTATUS
CvcCreate(
	VOID
);

VOID
CvcTerminate(
	VOID
);

NTSTATUS
CvcPostEx(
	const HANDLE Event,
	const PVOID pData,
	const ULONG DataLen
);

NTSTATUS
CvcPostHelloWorld(
	const HANDLE Event
);

NTSTATUS
CvcPostRead(
	const HANDLE Event,
	const HANDLE Pid,
	const DWORD64 Ptr,
	const ULONG Size,
	const PVOID pOut
);

NTSTATUS
CvcPostWrite(
	const HANDLE Event,
	const HANDLE Pid,
	const DWORD64 Ptr,
	const ULONG Size,
	const PVOID pSrc
);
