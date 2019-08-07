EXTERN NtCallbackReturn			:	proc
EXTERN CvcpDispatcher			:	proc
EXTERN D3DKMTOpenAdapterFromHdc		:	proc

Cvc		struct
		Msg		dd	?
		DataLen		dd	?
		Data		dq	?
		pConnection	dq	?
Cvc		ends

_TEXT SEGMENT

PUBLIC KeUserCallbackDispatcher

KeUserCallbackDispatcher PROC
		;int 3
		mov		ecx, Cvc.Msg[rsp]				;
		mov		rdx, Cvc.Data[rsp]				;
		mov		r8d, Cvc.DataLen[rsp]			;
		mov		r9, Cvc.pConnection[rsp]		;
		call	CvcpDispatcher
		xor		rcx, rcx						; Result
		xor		rdx, rdx						; ResultLength
		mov		r8d, eax						; Status
		call	NtCallbackReturn
		ret

KeUserCallbackDispatcher ENDP

PUBLIC CvcpProcessConnect

CvcpProcessConnect PROC

		sub		rsp, 60h
		push		rbp
		mov		rbp, rsp
		mov		[rbp+8h], rdx
		call	D3DKMTOpenAdapterFromHdc
		pop		rbp
		add		rsp, 60h
		ret

CvcpProcessConnect ENDP

end
