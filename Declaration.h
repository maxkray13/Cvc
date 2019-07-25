#pragma once

/*
Kernelmode message type that describe what subroutine should be run on usermode
*/
typedef enum _CvcMsgTypeKe
{
	CVCKE_NOP,
	CVCKE_DISPLAY,
	CVCKE_MAX,
	CVCKE_FORCE_DWORD = 0x7fffffff
}CvcMsgTypeKe, *pCvcMsgTypeKe;

/*
Usermode message type that describe what subroutine should be run on kernelmode
*/
typedef enum _CvcMsgTypeCL
{
	CVCCL_HELLO_WORLD,
	CVCCL_READ,
	CVCCL_WRITE,
	CVCCL_MAX,
	CVCCL_FORCE_DWORD = 0x7fffffff
}CvcMsgTypeCL, *pCvcMsgTypeCL;

/*
Usermode structure that used for managing messages and creating CvcCLMsg structure.
Created internally and should be acessed internally only.
*/
typedef struct _CvcCLMsgHeader {
	/*
	User message bata len in bytes
	*/
	ULONG		DataLen;
	/*
	Double linked list that links user messages
	*/
	DECLSPEC_ALIGN(MEMORY_ALLOCATION_ALIGNMENT) LIST_ENTRY CvcMessagesLinks;
	/*
	Pointer to variable that will recive operation status
	*/
	volatile PNTSTATUS	pResultStatus;
	/*
	Event that used for synchronisation betwen kernelmode\usermode
	*/
	HANDLE		Event;
	/*
	Offset to user defined message
	*/
	CHAR		Data;
}CvcCLMsgHeader, *pCvcCLMsgHeader;


/*
User message that will be passed to kernelmode dispatcher
*/
typedef struct _CvcCLMsg {
	/*
	Pointer to variable that will recive operation status
	*/
	PNTSTATUS	pResultStatus;
	/*
	Event that used for synchronisation betwen kernelmode\usermode
	*/
	HANDLE		Event;
	/*
	Offset to user defined message
	*/
	CHAR		Data;
}CvcCLMsg, *pCvcCLMsg;

#ifndef CVCHEADER_COMMON
/***/#define CVCHEADER_COMMON sizeof(PNTSTATUS) + sizeof(HANDLE)
#endif

typedef struct _CvcNull {
	CvcMsgTypeCL Type;
}CvcNull, *pCvcNull;

typedef struct _CvcHelloWorld {
	CvcMsgTypeCL Type;
	DWORD Magic;
}CvcHelloWorld, *pCvcHelloWorld;

typedef struct _CvcRead {
	CvcMsgTypeCL Type;
	HANDLE Pid;
	DWORD64 Ptr;
	ULONG Size;
	PVOID pOut;
}CvcRead, *pCvcRead;

typedef struct _CvcWrite {
	CvcMsgTypeCL Type;
	HANDLE Pid;
	DWORD64 Ptr;
	ULONG Size;
	PVOID pSrc;
}CvcWrite, *pCvcWrite;