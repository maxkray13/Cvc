#include "Native.h"

#pragma code_seg(push)
#pragma code_seg("PAGE")

NTSTATUS
PrFindProcesses(
	const wchar_t *Name,
	PEPROCESS * OutPr,
	PrCmp_t ProcessCompareFunc
) {

	if (!Name) {

		return STATUS_INVALID_PARAMETER_1;
	}

	if (!OutPr) {

		return STATUS_INVALID_PARAMETER_2;
	}

	UNICODE_STRING target_name = { 0 };
	RtlInitUnicodeString(&target_name, Name);

	if (!target_name.Buffer || !target_name.Length) {

		return STATUS_INVALID_PARAMETER_1;
	}

	*OutPr = NULL;
	PVOID pData = NULL;
	ULONG InfoLength = 0x8000;
	NTSTATUS Status = STATUS_SUCCESS;

	for (pData = ExAllocatePoolWithTag(NonPagedPool, InfoLength, MEMORY_TAG)
		;
		;pData = ExAllocatePoolWithTag(NonPagedPool, InfoLength, MEMORY_TAG)) {

		if (pData == NULL) {

			return STATUS_MEMORY_NOT_ALLOCATED;
		}

		Status = NtQuerySystemInformation(5, pData, InfoLength, NULL);

		if (Status != STATUS_INFO_LENGTH_MISMATCH) {

			break;
		}

		ExFreePoolWithTag(pData, MEMORY_TAG);
		InfoLength += 0x8000;
	}

	if (!NT_SUCCESS(Status))
	{
		ExFreePoolWithTag(pData, MEMORY_TAG);
		return Status;
	}

	PEPROCESS TargetProcess = NULL;

	__try {

		PSYSTEM_PROCESS_INFORMATION pProcessInfo = (PSYSTEM_PROCESS_INFORMATION)pData;
		do {

			if (RtlEqualUnicodeString(&pProcessInfo->ImageName, &target_name, TRUE)) {

				PEPROCESS TempEprocess = NULL;
				Status = PsLookupProcessByProcessId(pProcessInfo->UniqueProcessId, &TempEprocess);

				if (NT_SUCCESS(Status)) {

					if (ProcessCompareFunc) {

						if (ProcessCompareFunc(
							TempEprocess,
							pProcessInfo->Threads,
							(DWORD_PTR)pProcessInfo + pProcessInfo->NextEntryOffset)
							) {

							TargetProcess = TempEprocess;
							break;
						}
					}
					else {

						TargetProcess = TempEprocess;
						break;
					}
					ObfDereferenceObject(TempEprocess);
				}
			}
			if (!pProcessInfo->NextEntryOffset) {

				break;
			}

			pProcessInfo = (PSYSTEM_PROCESS_INFORMATION)((DWORD_PTR)pProcessInfo + pProcessInfo->NextEntryOffset);

		} while ((DWORD_PTR)pProcessInfo < (DWORD_PTR)pData + InfoLength);
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {

		DbgPrint("%s:exception 0x%X\n", __FUNCTION__, GetExceptionCode());
	}

	ExFreePoolWithTag(pData, MEMORY_TAG);

	if (TargetProcess == NULL) {

		return STATUS_UNSUCCESSFUL;
	}

	*OutPr = TargetProcess;
	return STATUS_SUCCESS;
}


#pragma code_seg(pop)