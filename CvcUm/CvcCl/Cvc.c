#include "../stdafx.h"
#include "Cvc.h"
#include "../Cse/Cse.h"

#pragma comment( lib, "gdi32.lib" )
#pragma comment( lib, "ntdll.lib" )

DECLSPEC_IMPORT
NTSTATUS
NTAPI
D3DKMTOpenAdapterFromHdc(
	PVOID Arg
);


DECLSPEC_IMPORT
NTSTATUS
NTAPI
NtCallbackReturn(
	IN PVOID                Result OPTIONAL,
	IN ULONG                ResultLength,
	IN NTSTATUS             Status
);

HANDLE		hDummyThread = 0;
BOOLEAN CalloutActive = FALSE;
LIST_ENTRY	CvcMessages;
SEMAPHORE	CvcIOSema;

#pragma comment(linker, "/export:KeUserCallbackDispatcher")

DWORD
DummyThread(
	LPVOID Param
) {

	char buff[0x60];
	*(HDC*)buff = GetDC(NULL);
	NTSTATUS Status = D3DKMTOpenAdapterFromHdc(buff);
	_mm_pause();

	return 0x1337;
}

NTSTATUS
CvcCreate(
	VOID
) {

	InitializeListHead(&CvcMessages);

	if (!(hDummyThread = CreateThread(NULL, 0, DummyThread, NULL, 0, NULL))) {

		return NTSTATUS_FROM_WIN32(GetLastError());
	}

	CalloutActive = TRUE;

	return STATUS_SUCCESS;
}

VOID
CvcTerminate(
	VOID
) {

	CalloutActive = FALSE;

	if (hDummyThread) {

		TerminateThread(hDummyThread, 0);
		CloseHandle(hDummyThread);
		hDummyThread = NULL;
	}
}

NTSTATUS CvcpDispatcher(
	CvcMsgTypeKe Msg,
	PVOID Data,
	ULONG DataLen
) {

	LockSemaphore(&CvcIOSema);

	if (Msg == CVCKE_DISPLAY) {

		char* Buffer = alloca(DataLen);
		sprintf(Buffer, Data);
		CseOutputA(Buffer);
	}

	if (IsListEmpty(&CvcMessages)) {

		UnlockSemaphore(&CvcIOSema);
		return NtCallbackReturn(NULL, 0, STATUS_SUCCESS);
	}

	const pCvcCLMsgHeader CLMessage =
		CONTAINING_RECORD(
			CvcMessages.Flink,
			CvcCLMsgHeader,
			CvcMessagesLinks
		);

	const ULONG MessageSize =
		CVCHEADER_COMMON +
		CLMessage->DataLen;

	PVOID Buffer = (PVOID)ALIGN_UP(
		alloca(MessageSize + MEMORY_ALLOCATION_ALIGNMENT),
		MEMORY_ALLOCATION_ALIGNMENT
	);

	memcpy(Buffer, &CLMessage->pResultStatus, MessageSize);

	RemoveEntryList(&CLMessage->CvcMessagesLinks);

	free(CLMessage);

	UnlockSemaphore(&CvcIOSema);

	return NtCallbackReturn(Buffer, MessageSize, STATUS_SUCCESS);
}


NTSTATUS
CvcPostEx(
	const HANDLE	Event,
	const PVOID		pData,
	const ULONG		DataLen
) {

	if (!CalloutActive) {

		return STATUS_CONNECTION_DISCONNECTED;
	}

	volatile NTSTATUS Status = STATUS_SUCCESS;

	if (!ResetEvent(Event)) {

		return NTSTATUS_FROM_WIN32(GetLastError());
	}

	const ULONG MessageSize =
		FIELD_OFFSET(CvcCLMsgHeader, Data) +
		DataLen;

	pCvcCLMsgHeader Message = (pCvcCLMsgHeader)malloc(MessageSize);

	if (Message == NULL) {

		return STATUS_NO_MEMORY;
	}

	ZeroMemory(Message, MessageSize);

	Message->DataLen = DataLen;
	memcpy(&Message->Data, pData, DataLen);
	Message->Event = Event;
	Message->pResultStatus = &Status;

	LockSemaphore(&CvcIOSema);
	InsertTailList(&CvcMessages, &Message->CvcMessagesLinks);
	UnlockSemaphore(&CvcIOSema);

	DWORD ExitCode = 0;

	while (GetExitCodeThread(hDummyThread, &ExitCode) && ExitCode == STILL_ACTIVE) {

		if (WaitForSingleObject(Event, 0x1000) == WAIT_OBJECT_0) {

			break;
		}
	}
	
	if (ExitCode != STILL_ACTIVE) {

		CvcTerminate();

		return STATUS_CONNECTION_DISCONNECTED;
	}

	return Status;
}

NTSTATUS
CvcPostHelloWorld(
	const HANDLE Event
) {

	CvcHelloWorld HelloWorld;
	HelloWorld.Type = CVCCL_HELLO_WORLD;
	HelloWorld.Magic = ' cvC';

	return CvcPostEx(Event, &HelloWorld, sizeof(HelloWorld));
}

NTSTATUS
CvcPostRead(
	const HANDLE Event,
	const HANDLE Pid,
	const DWORD64 Ptr,
	const ULONG Size,
	const PVOID pOut
) {

	CvcRead Read;
	Read.Type = CVCCL_READ;
	Read.Pid = Pid;
	Read.Ptr = Ptr;
	Read.Size = Size;
	Read.pOut = pOut;

	return CvcPostEx(Event, &Read, sizeof(Read));
}


NTSTATUS
CvcPostWrite(
	const HANDLE Event,
	const HANDLE Pid,
	const DWORD64 Ptr,
	const ULONG Size,
	const PVOID pSrc
) {

	CvcWrite Write;
	Write.Type = CVCCL_WRITE;
	Write.Pid = Pid;
	Write.Ptr = Ptr;
	Write.Size = Size;
	Write.pSrc = pSrc;

	return CvcPostEx(Event, &Write, sizeof(Write));
}