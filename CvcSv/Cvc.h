#pragma once
/*
Cvc - Communication via callback function prefix
*/

extern BOOLEAN CvcClosure;

VOID
CvcMain(
	PVOID StartContext
);

NTSTATUS
CvcCreate(
	VOID
);

VOID
CvcTerminate(
	VOID
);