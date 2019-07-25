#include "../Native/Native.h"
#include "../hde/hde64.h"
#include "../../Declaration.h"
#include "Cvc.h"
#include "CvcInternal.h"

BOOLEAN CvcClosure = FALSE;
BOOLEAN Shutdown = FALSE;
KGUARDED_MUTEX CvcMutex;
PEPROCESS ClientProcess = NULL;
PVOID pfnDispatcher = NULL;

extern OpenAdapter_t pfOpenAdapter;

#pragma code_seg(push)
#pragma code_seg("PAGE")

/*
Main function of processing user messages.
Determines if connection should be closed.
*/
BOOLEAN
CvcpProcessUserMessage(
	const PVOID InputData,
	const ULONG InputDataLen
);

NTSTATUS
CvcpProcessHelloWorldMsg(
	const pCvcHelloWorld Message
);

NTSTATUS
CvcpProcessReadMsg(
	const pCvcRead Message
);

NTSTATUS
CvcpProcessWriteMsg(
	const pCvcWrite Message
);

VOID
CvcpDispatcher(
	VOID
) {

	while (!Shutdown && !PsIsThreadTerminating(KeGetCurrentThread())) {

		PVOID Input = NULL;
		ULONG InputLen = 0;

		CvciUsermodeCallout(
			CVCKE_NOP,
			pfnDispatcher,
			NULL,
			0,
			&Input,
			&InputLen
		);

		if (Input && InputLen) {

			if (!CvcpProcessUserMessage(Input, InputLen)) {

				/*
				Something hardly messed up - break connection
				*/
				DbgPrint("%s: Break connection\n", __FUNCTION__);
				return;
			}
		}

		LARGE_INTEGER WaitTime = { .QuadPart = -8000 };

		KeDelayExecutionThread(
			UserMode,
			TRUE,
			&WaitTime
		);
	}
}


BOOLEAN
CvcpProcessUserMessage(
	const PVOID InputData,
	const ULONG InputDataLen
) {

	NTSTATUS UserStatus = STATUS_SUCCESS;
	pCvcCLMsg ClMessage = (pCvcCLMsg)ALIGN_UP_BY(
		alloca(InputDataLen + MEMORY_ALLOCATION_ALIGNMENT),
		MEMORY_ALLOCATION_ALIGNMENT
	);

	__try {

		RtlSecureZeroMemory(ClMessage, InputDataLen);
		RtlCopyMemory(ClMessage, InputData, InputDataLen);

	}
	__except (EXCEPTION_EXECUTE_HANDLER) {

		ClMessage = NULL;
		UserStatus = GetExceptionCode();
	}

	if (!ClMessage || !NT_SUCCESS(UserStatus)) {

		DbgPrint("%s: Exception occured while trying to access user message\n", __FUNCTION__);
		return FALSE;
	}

	__try {

		*ClMessage->pResultStatus = STATUS_SUCCESS;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {

		UserStatus = GetExceptionCode();
	}

	if (!NT_SUCCESS(UserStatus)) {

		DbgPrint("%s: Exception while trying to write ResultStatus\n", __FUNCTION__);
		return FALSE;
	}

	PRKEVENT Event;
	UserStatus = ObReferenceObjectByHandleWithTag(
		ClMessage->Event,
		(ACCESS_MASK)2,
		*ExEventObjectType,
		UserMode,
		'tlfD',
		&Event,
		NULL
	);

	if (!NT_SUCCESS(UserStatus)) {

		DbgPrint("%s: Cannot reference event\n", __FUNCTION__);
		return FALSE;
	}

	switch (((pCvcNull)&ClMessage->Data)->Type)
	{
	case CVCCL_HELLO_WORLD:
		UserStatus = CvcpProcessHelloWorldMsg((pCvcHelloWorld)&ClMessage->Data);
		break;
	case CVCCL_READ:
		UserStatus = CvcpProcessReadMsg((pCvcRead)&ClMessage->Data);
		break;
	case CVCCL_WRITE:
		UserStatus = CvcpProcessWriteMsg((pCvcWrite)&ClMessage->Data);
		break;
	default:
		UserStatus = STATUS_INVALID_PARAMETER;
		break;
	}

	*ClMessage->pResultStatus = UserStatus;

	KeSetEvent(Event, 1, FALSE);
	ObDereferenceObject(Event);
	return TRUE;
}

NTSTATUS
CvcpProcessHelloWorldMsg(
	const pCvcHelloWorld Message
) {

	NTSTATUS Status = STATUS_UNSUCCESSFUL;

	if (Message->Magic == ' cvC') {

		DbgPrint("Hello world!\n");

		ANSI_STRING UserOutput = RTL_CONSTANT_STRING("Hello, user!");

		PVOID Input = NULL;
		ULONG InputLen = 0;

		Status = CvciUsermodeCallout(
			CVCKE_DISPLAY,
			pfnDispatcher,
			UserOutput.Buffer,
			UserOutput.MaximumLength,
			&Input,
			&InputLen
		);
	}

	return Status;
}


NTSTATUS
CvcpProcessReadMsg(
	const pCvcRead Message
) {

	NTSTATUS Status = STATUS_SUCCESS;
	PEPROCESS Process = NULL;

	Status = PsLookupProcessByProcessId(Message->Pid, &Process);


	if (!NT_SUCCESS(Status)) {

		return Status;
	}

	SIZE_T Result = 0;

	__try {

		Status = MmCopyVirtualMemory(
			Process,
			(PVOID)Message->Ptr,
			PsGetCurrentProcess(),
			(PVOID)Message->pOut,
			Message->Size,
			KernelMode,
			&Result
		);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {

		Status = GetExceptionCode();
	}


	ObDereferenceObject(Process);

	return Status;
}

NTSTATUS
CvcpProcessWriteMsg(
	const pCvcWrite Message
) {

	NTSTATUS Status = STATUS_SUCCESS;
	PEPROCESS Process = NULL;

	Status = PsLookupProcessByProcessId(Message->Pid, &Process);

	if (!NT_SUCCESS(Status)) {

		return Status;
	}

	SIZE_T Result = 0;

	__try {

		Status = MmCopyVirtualMemory(
			PsGetCurrentProcess(),
			(PVOID)Message->pSrc,
			Process,
			(PVOID)Message->Ptr,
			Message->Size,
			KernelMode,
			&Result
		);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {

		Status = GetExceptionCode();
	}

	ObDereferenceObject(Process);

	return Status;
}

VOID
CvcMain(
	PVOID StartContext
) {

	UNREFERENCED_PARAMETER(StartContext);
	PAGED_CODE();

	KeInitializeGuardedMutex(&CvcMutex);

	while (!CvcClosure) {

		PAGED_CODE();

		while (!NT_SUCCESS(CvcCreate())) {

			if (CvcClosure) {

				CvciExit();
				CvcTerminate();
				PsTerminateSystemThread(0);
			}

			LARGE_INTEGER WaitTime = { .QuadPart = -1000 };

			KeDelayExecutionThread(
				KernelMode,
				TRUE,
				&WaitTime
			);
		}

		KeWaitForSingleObject(
			ClientProcess,
			Executive,
			KernelMode,
			TRUE,
			NULL
		);

		CvcTerminate();
	}

	CvciExit();

	PsTerminateSystemThread(0);
}

BOOLEAN CvcpPrCmp(
	PEPROCESS TempPr,
	PSYSTEM_THREAD_INFORMATION ThreadsInfo,
	DWORD_PTR Limit
) {

	UNREFERENCED_PARAMETER(ThreadsInfo);
	UNREFERENCED_PARAMETER(Limit);
	PAGED_CODE();

	BOOLEAN Correct = FALSE;
	KAPC_STATE ApcState;
	KeStackAttachProcess(TempPr, &ApcState);

	__try {

		PPEB Peb = PsGetProcessPeb(TempPr);

		pfnDispatcher = LdrFindProcAdressA(Peb->ImageBaseAddress, "KeUserCallbackDispatcher");

		if (pfnDispatcher) {

			Correct = TRUE;
		}
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {

		DbgPrint("%s: Exception 0x%X\n", __FUNCTION__, GetExceptionCode());
	}

	if (!Correct) {

		pfnDispatcher = NULL;
	}

	KeUnstackDetachProcess(&ApcState);

	return Correct;
}

NTSTATUS
CvcpOpenAdapterHook(
	PVOID Arg
) {

	KeAcquireGuardedMutex(&CvcMutex);

	if (IoGetCurrentProcess() == ClientProcess) {

		CvciUnhookOpenAdapter();
		KeReleaseGuardedMutex(&CvcMutex);

		CvcpDispatcher();

		return STATUS_CONNECTION_ABORTED;
	}

	KeReleaseGuardedMutex(&CvcMutex);

	return pfOpenAdapter(Arg);
}

NTSTATUS
CvcCreate(
	VOID
) {

	PAGED_CODE();
	Shutdown = FALSE;

	NTSTATUS Status = UtFindProcesses(L"CvcUm.exe", &ClientProcess, CvcpPrCmp);

	if (!NT_SUCCESS(Status)) {

		return Status;
	}

	KAPC_STATE Kapc;

	KeStackAttachProcess(ClientProcess, &Kapc);
	CvciHookOpenAdapter((PVOID)CvcpOpenAdapterHook);//patch dxgk interface for process session
	KeUnstackDetachProcess(&Kapc);

	return Status;
}

VOID
CvcTerminate(
	VOID
) {

	PAGED_CODE();
	Shutdown = TRUE;

	if (ClientProcess) {

		KAPC_STATE Kapc;

		KeStackAttachProcess(ClientProcess, &Kapc);
		CvciUnhookOpenAdapter();
		KeUnstackDetachProcess(&Kapc);

		ObDereferenceObject(ClientProcess);
		ClientProcess = NULL;
	}

	pfnDispatcher = NULL;
}
#pragma code_seg(pop)