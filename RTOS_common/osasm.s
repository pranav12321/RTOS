;/*****************************************************************************/
;/* OSasm.s: low-level OS commands, written in assembly                       */
;/* derived from uCOS-II                                                      */
;/*****************************************************************************/

        AREA |.text|, CODE, READONLY, ALIGN=2
        THUMB
        REQUIRE8
        PRESERVE8

        EXTERN  RunPt            ; currently running thread

        EXPORT  StartOS
        EXPORT  ContextSwitch
        EXPORT  PendSV_Handler
        EXPORT  SVC_Handler
        EXPORT  StartOS
		EXPORT  Debug
		EXPORT  Debug2
		EXPORT  set_R9
		IMPORT   Runptr
		IMPORT   STACK
		IMPORT   threads
		IMPORT   Scheduler
		IMPORT   EnableInterrupts
		IMPORT   DisableInterrupts
		IMPORT   OS_Id
		IMPORT   OS_Kill
		IMPORT   OS_Time
		IMPORT   OS_AddThread
		IMPORT   OS_Sleep
		


NVIC_INT_CTRL   EQU     0xE000ED04                              ; Interrupt control state register.
NVIC_SYSPRI14   EQU     0xE000ED22                              ; PendSV priority register (position 14).
NVIC_SYSPRI15   EQU     0xE000ED23                              ; Systick priority register (position 15).
NVIC_LEVEL14    EQU           0xEF                              ; Systick priority value (second lowest).
NVIC_LEVEL15    EQU           0xFF                              ; PendSV priority value (lowest).
NVIC_PENDSVSET  EQU     0x10000000                              ; Value to trigger PendSV exception.
NVIC_SYS_HND_CTRL_R EQU    0xE000ED24

StartOS
; put your code here
    
    
    BX      LR                 ; start first thread

OSStartHang
    B       OSStartHang        ; Should never get here


;********************************************************************************************************
;                               PERFORM A CONTEXT SWITCH (From task level)
;                                           void ContextSwitch(void)
;
; Note(s) : 1) ContextSwitch() is called when OS wants to perform a task context switch.  This function
;              triggers the PendSV exception which is where the real work is done.
;********************************************************************************************************

ContextSwitch
; edit this code
	LDR R0, = NVIC_INT_CTRL
	;LDR R1, = NVIC_SYSPRI14
	;LDR R2, = NVIC_LEVEL15
	;LSL R2, #7
	;LDR R12, [R1]
	;ORR R12, R12, R2
	;STR R12, [R1]
	LDR R1, [R0]
	LDR R2, = NVIC_PENDSVSET
	ORR R1, R1, R2
	STR R1, [R0]
	PUSH{R0, LR}
	BL   EnableInterrupts         ; 2) Prevent interrupt during switch
	POP{R0, LR}
    
    BX      LR

    

;********************************************************************************************************
;                                         HANDLE PendSV EXCEPTION
;                                     void OS_CPU_PendSVHandler(void)
;
; Note(s) : 1) PendSV is used to cause a context switch.  This is a recommended method for performing
;              context switches with Cortex-M.  This is because the Cortex-M3 auto-saves half of the
;              processor context on any exception, and restores same on return from exception.  So only
;              saving of R4-R11 is required and fixing up the stack pointers.  Using the PendSV exception
;              this way means that context saving and restoring is identical whether it is initiated from
;              a thread or occurs due to an interrupt or exception.
;
;           2) Pseudo-code is:
;              a) Get the process SP, if 0 then skip (goto d) the saving part (first context switch);
;              b) Save remaining regs r4-r11 on process stack;
;              c) Save the process SP in its TCB, OSTCBCur->OSTCBStkPtr = SP;
;              d) Call OSTaskSwHook();
;              e) Get current high priority, OSPrioCur = OSPrioHighRdy;
;              f) Get current ready thread TCB, OSTCBCur = OSTCBHighRdy;
;              g) Get new process SP from TCB, SP = OSTCBHighRdy->OSTCBStkPtr;
;              h) Restore R4-R11 from new process stack;
;              i) Perform exception return which will restore remaining context.
;
;           3) On entry into PendSV handler:
;              a) The following have been saved on the process stack (by processor):
;                 xPSR, PC, LR, R12, R0-R3
;              b) Processor mode is switched to Handler mode (from Thread mode)
;              c) Stack is Main stack (switched from Process stack)
;              d) OSTCBCur      points to the OS_TCB of the task to suspend
;                 OSTCBHighRdy  points to the OS_TCB of the task to resume
;
;           4) Since PendSV is set to lowest priority in the system (by OSStartHighRdy() above), we
;              know that it will only be run when no other exception or interrupt is active, and
;              therefore safe to assume that context being switched out was using the process stack (PSP).
;********************************************************************************************************

PendSV_Handler
; put your code here
	CPSID I
	PUSH{R0, LR}
	BL   DisableInterrupts         ; 2) Prevent interrupt during switch
	POP{R0, LR}
	LDR R0, =0xE000ED04
	LDR R1, [R0]
	ORR R1, R1, #0x08000000
	STR R1, [R0]
	;LDR R2, = PF2
	;LDR R2, [R2]
	;EOR R3, R3, #0x04
	;STR R3, [R2]
    PUSH    {R4-R11}  ; 3) Save remaining regs r4-11
    LDR     R0,=Runptr  ; 4) R0=pointer to RunI, old thread
    LDR     R1,[R0]   ;    R1 = RunI
    ;LSL     R2,R1,#2  ; R2 = RunI*4
	STR     SP, [R1]    ; save current stack pointer
	;ADD     R1, #4
	;LDR     R1, [R1]
    ;STR     R1, [R0]
	PUSH {R0, R1, R2, LR, R3}
    BL  Scheduler
	POP  {R0, R1, R2, LR, R3}
	LDR  R1, [R0]
	LDR  SP, [R1]       ; load dstack pointer for new thread

    POP     {R4-R11}  ; 8) restore regs r4-11
	;EOR R3, R3, #0x04
	;STR R3, [R2]
	PUSH{R0, LR}
	BL   EnableInterrupts         ; 2) Prevent interrupt during switch
	POP{R0, LR}
    
    
    BX      LR                 ; Exception return will restore remaining context   
    
Debug
	;CPSID I
	;LDR R0, = st1
	;LDR R1, [R0]
	;MOV R1, SP
	;STR R1, [R0]
	;LDR R0, = Count1
	
	;LDR R1, [R0]
	;ADD R1, #1
	;STR R1, [R0]
	;CPSIE I
	BX LR

Debug2
	;CPSID I
	;LDR R0, = st1
	;LDR R1, [R0]
	;MOV R1, SP
	;MOV SP, R1
	;LDR R0, = st2
	;STR R1, [R0]
	;LDR R0, = Count2
	
	;LDR R1, [R0]
	;ADD R1, #1
	;STR R1, [R0]
	;CPSIE I
	;PUSH{R0, LR}
	;BL   EnableInterrupts         ; 2) Prevent interrupt during switch
	;POP{R0, LR}
	BX LR

    
    

    

;********************************************************************************************************
;                                         HANDLE SVC EXCEPTION
;                                     void OS_CPU_SVCHandler(void)
;
; Note(s) : SVC is a software-triggered exception to make OS kernel calls from user land. 
;           The function ID to call is encoded in the instruction itself, the location of which can be
;           found relative to the return address saved on the stack on exception entry.
;           Function-call paramters in R0..R3 are also auto-saved on stack on exception entry.
;********************************************************************************************************

        IMPORT    OS_Id
        IMPORT    OS_Kill
        IMPORT    OS_Sleep
        IMPORT    OS_Time
        IMPORT    OS_AddThread
		IMPORT    OS_TimeDifference

SVC_Handler
; put your Lab 5 code here
	CPSID I
	PUSH {LR}
	LDR R12,[SP,#28] ; Return address
	LDRH R12,[R12,#-2] ; SVC instruction is 2 bytes
	BIC R12,#0xFF00 ; Extract ID in R12
	MOV R2, SP
	ADD R2, R2, #4
	LDM R2,{R0-R3} ; Get any parameters
	
ID	    
    CMP R12, #0
	BNE KILL
	BL  OS_Id
	B Done
	
KILL	
    CMP R12, #1
	BNE SLEEP
	BL  OS_Kill
	B Done
	
SLEEP	
    CMP R12, #2
	BNE TIME
	BL OS_Sleep
	B Done
	
TIME	
    CMP R12, #3
	BNE TIME_DIF
	BL OS_Time
	B Done

TIME_DIF	
    CMP R12, #4
	BNE THREAD
	BL OS_TimeDifference
	B Done
	
THREAD	
    CMP R12, #5
	BNE Done2
	BL OS_AddThread
	B Done
	

Done
	
    STR R0,[SP, #4] ; Store return value
Done2
;	LDR R1, = NVIC_SYS_HND_CTRL_R
;	LDR R1, [R1]
;	LDR R2, [R1]
;	AND R2, R2, #0xFFFF7FFF
;	STR R2, [R1]
	POP {LR}
	CPSIE I
    BX      LR                   ; Return from exception

set_R9
	MOV R9, R0
	BX LR


    ALIGN
    END
