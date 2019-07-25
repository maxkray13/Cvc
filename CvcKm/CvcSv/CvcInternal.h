#pragma once

NTSTATUS
CvcInitInternals(
	VOID
);

VOID
CvciHookOpenAdapter(
	PVOID pFn
);

VOID
CvciUnhookOpenAdapter(
	VOID
);

VOID
CvciExit(
	VOID
);

typedef struct _CALLOUT_FRAME {
	CvcMsgTypeKe	MsgType;
	PVOID			Buffer;
	ULONG			Length;
	ULONG			Spare[2];
} CALLOUT_FRAME, *PCALLOUT_FRAME;

NTSTATUS
CvciUsermodeCallout(
	CvcMsgTypeKe	MsgType,
	PVOID			Dispatcher,
	PVOID			InputBuffer,
	ULONG			InputLength,
	PVOID *			OutputBuffer,
	ULONG *			OutputLength
);