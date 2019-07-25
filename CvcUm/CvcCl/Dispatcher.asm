EXTERN NtCallbackReturn: proc
EXTERN CvcpDispatcher:proc

Cvc		struct
        Msg		dq ?
        Data	dq ?
        DataLen	dd ?
Cvc		ends

_TEXT SEGMENT

PUBLIC KeUserCallbackDispatcher

KeUserCallbackDispatcher PROC
		;int 3
		mov		rcx, Cvc.Msg[rsp]				;
		mov		rdx, Cvc.Data[rsp]				;
		mov		r8d, Cvc.DataLen[rsp]			;
		call	CvcpDispatcher
		xor		rcx, rcx						; Result
		xor		rdx, rdx						; ResultLength
		mov		r8d, eax						; Status
		call	NtCallbackReturn
		ret

KeUserCallbackDispatcher ENDP

_TEXT ENDS

end