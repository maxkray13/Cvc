#include "../Native/Native.h"
#include "Cvc.h"

BOOLEAN CvcClosure = FALSE;
BOOLEAN Shutdown = FALSE;
PEPROCESS ClientProcess = NULL;
PVOID pfnDispatcher = NULL;
DWORD64 KiSystemCall64 = 0;
DWORD64 CvcpSyscallDispatcher;
DWORD64 CvcpTargetPscId = 0;
DWORD64 Sema = 0;

extern
NTSTATUS
FASTCALL
UsermodeCallout(
	PVOID	Dispatcher,
	PVOID	UmGsBase,
	PVOID*	OutputBuffer,
	ULONG*	OutputLen
);

typedef enum _CvcMessageType
{
	CVC_NOP,
	CVC_DISPLAY,
	CVC_MAX,
	CVC_FORCE_DWORD = 0x7fffffff
}CvcMessageType, *pCvcMessageType;

typedef struct _CALLOUT_FRAME {
	CvcMessageType	MsgType;
	PVOID			Buffer;
	ULONG			Length;
	ULONG			Spare[2];
} CALLOUT_FRAME, *PCALLOUT_FRAME;


#pragma code_seg(push)
#pragma code_seg("PAGE")

NTSTATUS
CvcpUsermodeCallout(
	CvcMessageType	MsgType,
	PVOID			InputBuffer,
	ULONG			InputLength,
	PVOID *			OutputBuffer,
	ULONG *			OutputLength
) {

	PAGED_CODE();
	NTSTATUS Status = STATUS_SUCCESS;
	PNT_TIB64 ClientTeb = NULL;

	__try {

		const _PKTRAP_FRAME Trap = *(_PKTRAP_FRAME*)((DWORD_PTR)KeGetCurrentThread() + 0x1d8);

		ClientTeb = PsGetThreadTeb(KeGetCurrentThread());

		ULONG Length = ALIGN_UP(InputLength + sizeof(CALLOUT_FRAME), MEMORY_ALLOCATION_ALIGNMENT);

		PCALLOUT_FRAME CalloutFrame = (PCALLOUT_FRAME)(Trap->Rsp - Length);
		RtlCopyMemory(CalloutFrame + 1, InputBuffer, InputLength);

		CalloutFrame->MsgType = MsgType;
		CalloutFrame->Buffer = (PVOID)(CalloutFrame + 1);
		CalloutFrame->Length = InputLength;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {

		return GetExceptionCode();
	}

	Status = UsermodeCallout(
		pfnDispatcher,
		ClientTeb,
		OutputBuffer,
		OutputLength
	);

	return Status;
}


VOID
CvcpDispatcher(
	VOID
) {

	while (!Shutdown) {

		STRING UserOutput = RTL_CONSTANT_STRING("Hello, user!");
		PVOID Output = NULL;
		ULONG OutputLen = 0;

		NTSTATUS Status = CvcpUsermodeCallout(CVC_DISPLAY,
			UserOutput.Buffer,
			UserOutput.MaximumLength,
			&Output,
			&OutputLen
		);

		DbgPrint("Status 0x%X", Status);

		LARGE_INTEGER WaitTime = { .QuadPart = -3000 };

		KeDelayExecutionThread(
			KernelMode,
			TRUE,
			&WaitTime
		);
	}
}

VOID
CvcMain(
	PVOID StartContext
) {

	UNREFERENCED_PARAMETER(StartContext);
	PAGED_CODE();

	while (!CvcClosure) {

		PAGED_CODE();

		while (!NT_SUCCESS(CvcCreate())) {

			if (CvcClosure) {

				PsTerminateSystemThread(0);
				return;
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

		DbgPrint("%s :exception 0x%X\n", __FUNCTION__, GetExceptionCode());
	}

	if (!Correct) {

		pfnDispatcher = NULL;
	}

	KeUnstackDetachProcess(&ApcState);

	return Correct;
}

NTSTATUS
CvcpPatchLStar(
	VOID
) {

	if (!KiSystemCall64) {

		KiSystemCall64 = __readmsr(0xC0000082);
	}

	const ULONG NumberOfProcessors = KeQueryActiveProcessorCountEx((USHORT)-1);

	NTSTATUS Status = STATUS_SUCCESS;
	GROUP_AFFINITY Affinity, OldAffinity;
	PROCESSOR_NUMBER ProcessorNumber;

	ULONG SucessPatches = 0;
	
	for (ULONG i = 0; i < NumberOfProcessors; i++) {
		
		Status = KeGetProcessorNumberFromIndex(i, &ProcessorNumber);

		if (!NT_SUCCESS(Status)) {

			break;
		}

		RtlZeroMemory(&Affinity, sizeof(Affinity));
		RtlZeroMemory(&OldAffinity, sizeof(OldAffinity));

		Affinity.Group = ProcessorNumber.Group;
		Affinity.Mask = (KAFFINITY)(1 << ProcessorNumber.Number);

		KeSetSystemGroupAffinityThread(&Affinity, &OldAffinity);
		__writemsr(0xC0000082, CvcpSyscallDispatcher);
		KeRevertToUserGroupAffinityThread(&OldAffinity);
	}

	DbgPrint("SucessPatches %d of %d processors\n", SucessPatches, NumberOfProcessors);

	return Status;
}

NTSTATUS
CvcCreate(
	VOID
) {

	PAGED_CODE();

	Shutdown = FALSE;

	NTSTATUS Status = PrFindProcesses(L"CvcUm.exe", &ClientProcess, CvcpPrCmp);

	if (!NT_SUCCESS(Status)) {

		return Status;
	}

	Status = CvcpPatchLStar();

	if (!NT_SUCCESS(Status)) {

		DbgPrint("%s:%d:Status 0x%X", __FUNCTION__, __LINE__, Status);
		CvcTerminate();

		return Status;
	}

	CvcpTargetPscId = *(DWORD64*)((DWORD_PTR)ClientProcess + 0x180);

	return STATUS_SUCCESS;
}

VOID
CvcTerminate(
	VOID
) {

	PAGED_CODE();

	Shutdown = TRUE;

	if (ClientProcess) {

		ObDereferenceObject(ClientProcess);
		ClientProcess = NULL;
	}

	pfnDispatcher = NULL;
	CvcpTargetPscId = 0;
}

#pragma code_seg(pop)