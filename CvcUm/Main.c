#include "stdafx.h"
#include "Cse/Cse.h"
#include "CvcCl/Cvc.h"


int Main() {

	NTSTATUS Status = STATUS_SUCCESS;

	if (!NT_SUCCESS(Status = InitCRT())) {

		TerminateProcess((HANDLE)-1, Status);
	}

	if (!NT_SUCCESS(Status = CseCreate())) {

		TerminateProcess((HANDLE)-1, Status);
	}

	if (!NT_SUCCESS(Status = CseOutputA("Hi"))) {

		TerminateProcess((HANDLE)-1, Status);
	}

	HANDLE Event = CreateEventA(NULL, TRUE, FALSE, "Cvc");

	if (!Event) {

		TerminateProcess((HANDLE)-1, NTSTATUS_FROM_WIN32(GetLastError()));
	}

	if (!NT_SUCCESS(Status = CvcCreate())) {

		TerminateProcess((HANDLE)-1, Status);
	}

	Status = CvcPostHelloWorld(Event);

	char buff[0x100];
	sprintf(buff, "Status = 0x%X", Status);

	CseOutputA(buff);

	WORD DosSignature = 0;

	Status = CvcPostRead(
		Event,
		(HANDLE)GetCurrentProcessId(),
		GetModuleHandleA(NULL),
		sizeof(WORD),
		&DosSignature
	);

	sprintf(buff, "Status = 0x%X", Status);
	CseOutputA(buff);

	sprintf(buff, "DosSignature = %x", DosSignature);
	CseOutputA(buff);

	if (!NT_SUCCESS(Status = CseWaitInput())) {

		TerminateProcess((HANDLE)-1, Status);
	}

	CvcTerminate();
	CloseHandle(Event);

	if (!NT_SUCCESS(Status = CseClear())) {

		TerminateProcess((HANDLE)-1, Status);
	}

	TerminateProcess((HANDLE)-1, 0);

	return 0;
}