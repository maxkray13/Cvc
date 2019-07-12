#pragma once
/*
Pr - Process function prefix
*/

typedef BOOLEAN(__fastcall*PrCmp_t)(PEPROCESS, PSYSTEM_THREAD_INFORMATION, DWORD_PTR);

NTSTATUS
PrFindProcesses(
	const wchar_t *Name,
	PEPROCESS * OutPr,
	PrCmp_t ProcessCompareFunc
);