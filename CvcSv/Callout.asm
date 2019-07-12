include ksamd64.inc


;
;PCR
;
PcrCurrentThread equ				0188H
PcrTss equ							0008H
PcrRspBase equ						01A8H
PcrUserRsp equ						0010H
PcrMxCsr equ						0180H
;
;Thread
;
ThrProcess equ						0210H
ThrCallbackStack equ				01E8H
ThrTrapFrame equ					01D8H
ThrInitialStack equ					0028H
ThrFirstArgument equ				01E0H
ThrSystemCallNumber equ				01F8H
;
; Process
;
PcsUniqueProcessId equ				0180H
;
;Callout Frame Offset's
;
CoCallbackStack equ					00D8H
CoInitialStack equ					0028H
CoOutputBuffer equ					00E0H
CoOutputLength equ					00E8H
CoTrapFrame equ						00D0H
;
;Kernel Stack Control Structure Offset's
;
KscPreviousBase equ					0028H
KscActualLimit equ					0020H
KscCurrentBase equ					0000H
;
;Tss
;
TsRsp0 equ							0004H
;
;Trap Frame Offset's
;
TfRsp equ							0180H
TfRbp equ							0158H
TfEFlags equ						0178H
;
KERNEL_STACK_CONTROL_LENGTH equ		0050H


KTrapFrame		struct
				P1Home					dq ?		;0H
				P2Home					dq ?		;8H
				P3Home					dq ?		;10H
				P4Home					dq ?		;18H
				P5						dq ?		;20H
				PreviousMode			db ?		;28H
				PreviousIrql			db ?		;29H
				FaultIndicator			db ?		;2AH
				ExceptionActive			db ?		;2BH
				MxCsr					dd ?		;2CH
				TfRax					dq ?		;30H
				TfRcx					dq ?		;38H
				TfRdx					dq ?		;40H
				TfR8					dq ?		;48H
				TfR9					dq ?		;50H
				TfR10					dq ?		;58H
				TfR11					dq ?		;60H
				GsSwap					dq ?		;68H
				TfXmm0					dq ?		;70H
				TfXmmPad0				dq ?		;78H
				TfXmm1					dq ?		;80H
				TfXmmPad1				dq ?		;88H
				TfXmm2					dq ?		;90H
				TfXmmPad2				dq ?		;98H
				TfXmm3					dq ?		;A0H
				TfXmmPad3				dq ?		;A8H
				TfXmm4					dq ?		;B0H
				TfXmmPad4				dq ?		;B8H
				TfXmm5					dq ?		;C0H
				TfXmmPad5				dq ?		;C8H
				ContextRecord			dq ?		;D0H
				TfDr0					dq ?		;D8H
				TfDr1					dq ?		;E0H
				TfDr2					dq ?		;E8H
				TfDr3					dq ?		;F0H
				TfDr6					dq ?		;F8H
				TfDr7					dq ?		;100H
				DebugControl			dq ?		;108H
				LastBranchToRip			dq ?		;110H
				LastBranchFromRip		dq ?		;118H
				LastExceptionToRip		dq ?		;120H
				LastExceptionFromRip	dq ?		;128H
				SegDs					dw ?		;130H
				SegEs					dw ?		;132H
				SegFs					dw ?		;134H
				SegGs					dw ?		;136H
				TrapFrame				dq ?		;138H
				TfRbx					dq ?		;140H
				TfRdi					dq ?		;148H
				TfRsi					dq ?		;150H
KTrapFrame		ends

EXTERN CvcpDispatcher: proc
EXTERN CvcpTargetPscId: qword
EXTERN KiSystemCall64: qword
EXTERN Sema: qword

.code PAGE$00

CvcpSyscallDispatcher PROC

		swapgs
		mov     gs:[PcrUserRsp], rsp
		mov     rsp, gs:[PcrRspBase]
		pushfq
		push	rax
		push	rcx
		push	rdx
		cmp		rax, 031H
		jne Final
		WaitLoop:
		cmp		[CvcpTargetPscId], 00H
		je	Final
		pause
		mov		rax, 00H
		mov		rcx, 01H
		lea		rdx, Sema
		lock cmpxchg [rdx], ecx
		jnz WaitLoop
		mov		rax, gs:[PcrCurrentThread]
		mov		rax, ThrProcess[rax]
		mov		rax, PcsUniqueProcessId[rax]
		mov		rcx, [CvcpTargetPscId]
		cmp		rcx, rax
		jne FreeLock
		xor		rax, rax
		mov		[CvcpTargetPscId], rax
		mov		rdx, [KiSystemCall64]
		mov		rax, rdx
		shr		rdx, 32
		mov		ecx, 0C0000082h
		wrmsr
		pop		rdx
		pop		rcx
		pop		rax
		popfq
		push    02Bh								; SS
		push	qword ptr gs:[PcrUserRsp]
		push    r11									; EFLAGS
		push    033h								; CS
		push	rcx									; RIP
		mov     rcx, r10							; first argument value
		sub     rsp, 8								; error code space
		mov		rbp,rsp
		stmxcsr	KTrapFrame.MxCsr[rbp]
		ldmxcsr gs:[PcrMxCsr]
		mov     rbx,gs:[PcrCurrentThread]
		sti											; enable maskable interrupts
		mov		ThrFirstArgument[rbx],rcx
		mov		ThrSystemCallNumber[rbx],eax
		mov		ThrTrapFrame[rbx],rsp

		call CvcpDispatcher

		ldmxcsr	[rbp + KTrapFrame.MxCsr]
		mov     r8, TfRsp[rbp]						; get previous RSP value
		mov     r9, TfRbp[rbp]						; get previous RBP value
		xor		rax,rax								;
		xor		rdx,rdx								;
		xor		rbx,rbx								;
		xor		rsi,rsi								;
		xor		rdi,rdi								;
		xor     r10, r10							;
		xor     r12, r12							;
		xor     r13, r13							;
		xor     r14, r14							;
		xor     r15, r15							;
		pxor    xmm0, xmm0							;
		pxor    xmm1, xmm1							;
		pxor    xmm2, xmm2							;
		pxor    xmm3, xmm3							;
		pxor    xmm4, xmm4							;
		pxor    xmm5, xmm5							;
		xor     r11, r11							; EFLAGS
		mov     rbp, r9								; restore RBP
		mov     rsp, r8								; restore RSP
		swapgs
		sysretq

		FreeLock:
		xor		rax,rax
		lock xchg [rdx],rax;
		Final:
		pop		rdx
		pop		rcx
		pop		rax
		popfq
		mov     rsp, gs:[PcrUserRsp]
		swapgs
		jmp KiSystemCall64
CvcpSyscallDispatcher ENDP

;
;	NTSTATUS
;	FASTCALL
;	UsermodeCallout(
;		PVOID	Dispatcher,
;		PVOID	UmGsBase,
;		PVOID*	OutputBuffer,
;		PULONG	OutputLen
;	)
;
; Arguments :
; rcx	- address of usermode dispatcher function
; rdx	- usermode gs base
; r8	- output buffer
; r9	- output len
;
NESTED_ENTRY UsermodeCallout, PAGE$00

		GENERATE_EXCEPTION_FRAME<rbp>		
		mov     [rax-8h], rbp						; set up exception frame properly
		mov		rbp,rsp								;
		mov     CoOutputBuffer[rbp], r8				; save output buffer
        mov     CoOutputLength[rbp], r9				; save output length
		mov     rbx, gs:[PcrCurrentThread]			; get current thread address
		sub     rsp, KERNEL_STACK_CONTROL_LENGTH
		mov     rax, ThrCallbackStack[rbx]			; save current callback stack address
        mov     CoCallbackStack[rbp], rax			;
		;
		; Setup kernel stack segment descriptors
		;
		mov     r8, ThrInitialStack[rbx]			; save initial stack address
        mov     CoInitialStack[rbp], r8				; will be restored in NtCallbackReturn
		mov     qword ptr KscPreviousBase[rsp], 0	; clear previous base
        mov     rax, KscCurrentBase[r8]				; set current stack base
        mov     KscCurrentBase[rsp], rax			;
		mov     rax, KscActualLimit[r8]				; set current stack limit
        mov     KscActualLimit[rsp], rax			;
		mov     rsi, ThrTrapFrame[rbx]				; get current trap frame address
        mov     CoTrapFrame[rbp], rsi				; save trap frame address to callout frame
		;
		; due to NtCallbackReturn add size of exception frame( 138h) to saved callback stack
		; we jumps directly to return address of this function
		;
		mov     ThrCallbackStack[rbx], rbp			; set new callback stack address
		cli											; disable maskable interrupts
		;mov		r9, rcx								;
		;mov		ecx, 0C0000102h						;
		;mov		rax, rdx							;
		;shr		rdx, 32								;
		;wrmsr											; set IA32_KERNEL_GS_BASE to usermode gs base 
		;mov		rcx, r9								;
        mov     rdi, gs:[PcrTss]					; get processor TSS address
        mov     ThrInitialStack[rbx], rsp			; set new initial stack address in ETHREAD
        mov     TsRsp0[rdi], rsp					; set initial stack address in TSS
        mov     gs:[PcrRspBase], rsp				; set initial stack address in PRCB
		;
		; Prepare for system return
		;
		mov     r8, TfRsp[rsi]						; get previous RSP value
        mov     r9, TfRbp[rsi]						; get previous RBP value
		xor		rax,rax								;
		xor		rdx,rdx								;
		xor		rbx,rbx								;
		xor		rsi,rsi								;
		xor		rdi,rdi								;
		xor     r10, r10							;
		xor     r12, r12							;
		xor     r13, r13							;
		xor     r14, r14							;
		xor     r15, r15							;
        pxor    xmm0, xmm0							;
        pxor    xmm1, xmm1							;
        pxor    xmm2, xmm2							;
        pxor    xmm3, xmm3							;
        pxor    xmm4, xmm4							;
        pxor    xmm5, xmm5							;
        xor     r11, r11							; EFLAGS
        mov     rbp, r9								; restore RBP
        mov     rsp, r8								; restore RSP
		swapgs										; exchange current gs base with msr value
		sysretq										; x64 system return

NESTED_END UsermodeCallout, PAGE$00

end