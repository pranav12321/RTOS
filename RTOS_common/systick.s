; SysTick.s
; Runs on LM4F120/TM4C123



NVIC_ST_CTRL_R        EQU 0xE000E010
NVIC_ST_RELOAD_R      EQU 0xE000E014
NVIC_ST_CURRENT_R     EQU 0xE000E018
NVIC_SYS_PRI3_R       EQU 0xE000ED20
NVIC_ST_CTRL_COUNT    EQU 0x00010000  ; Count flag
NVIC_ST_CTRL_CLK_SRC  EQU 0x00000004  ; Clock Source
NVIC_ST_CTRL_INTEN    EQU 0x00000002  ; Interrupt enable
NVIC_ST_CTRL_ENABLE   EQU 0x00000001  ; Counter mode
NVIC_ST_RELOAD_M      EQU 0x00FFFFFF  ; Counter load value
GPIO_PORTF_DATA_R	  EQU 0x400253FC  ; PORTF DATA
PF2                   EQU 0x40025010
	
	
	   THUMB
	   PRESERVE8
        AREA DATA, ALIGN=2
	
        Counts   SPACE 4

           AREA    |.text|, CODE, READONLY, ALIGN=2
           EXPORT   SysTick_Init
           EXPORT   SysTick_Wait
           EXPORT   SysTick_Wait10ms
		EXPORT   SysTick_Handler
		EXPORT   Counts
	     EXPORT   launch_first_thread
		IMPORT   Runptr
		IMPORT   STACK
		IMPORT   threads
		IMPORT   Scheduler
		IMPORT   EnableInterrupts
		IMPORT   DisableInterrupts
;------------SysTick_Init------------
; Initialize SysTick with busy wait running at bus clock.
; Input: none
; Output: none
; Modifies: R0, R1
SysTick_Init
    ; disable SysTick during setup
    LDR R1, =NVIC_ST_CTRL_R         ; R1 = &NVIC_ST_CTRL_R
	MOV R2, R0
    MOV R0, #0                      ; R0 = 0
    STR R0, [R1]                    ; [R1] = R0 = 0
    ; maximum reload value
    LDR R1, =NVIC_ST_RELOAD_R       ; R1 = &NVIC_ST_RELOAD_R
    LDR R0, =NVIC_ST_RELOAD_M;      ; R0 = NVIC_ST_RELOAD_M
    STR R0, [R1]                    ; [R1] = R0 = NVIC_ST_RELOAD_M
	STR R2, [R1]
    ; any write to current clears it
    LDR R1, =NVIC_ST_CURRENT_R      ; R1 = &NVIC_ST_CURRENT_R
    MOV R0, #0                      ; R0 = 0
    STR R0, [R1]                    ; [R1] = R0 = 0
    ; enable SysTick with core clock
	LDR R1, =NVIC_SYS_PRI3_R
	LDR R2, = NVIC_ST_RELOAD_M
	LDR R3, [R1]
	AND R3, R3, R2
	MOV R0, #0xE0000000
	ORR R3, R3, R0
	STR R3, [R1]
    LDR R1, =NVIC_ST_CTRL_R         ; R1 = &NVIC_ST_CTRL_R
                                    ; R0 = ENABLE and CLK_SRC bits set
;loop
 ;   B loop
    MOV R0, #(NVIC_ST_CTRL_ENABLE+NVIC_ST_CTRL_CLK_SRC+NVIC_ST_CTRL_INTEN)
    STR R0, [R1]                    ; [R1] = R0 = (NVIC_ST_CTRL_ENABLE|NVIC_ST_CTRL_CLK_SRC)
    BX  LR                          ; return

;------------SysTick_Wait------------
; Time delay using busy wait.
; Input: R0  delay parameter in units of the core clock (units of 12.5 nsec for 80 MHz clock)
; Output: none
; Modifies: R0, R1, R3
SysTick_Wait
    LDR  R1, =NVIC_ST_RELOAD_R      ; R1 = &NVIC_ST_RELOAD_R
    SUB  R0, #1
    STR  R0, [R1]                   ;delay-1;  // number of counts to wait
    LDR  R1, =NVIC_ST_CTRL_R        ; R1 = &NVIC_ST_CTRL_R
SysTick_Wait_loop
    LDR  R3, [R1]                   ; R3 = NVIC_ST_CTRL_R
    ANDS R3, R3, #0x00010000       ; Count set?
    BEQ  SysTick_Wait_loop
    BX   LR                         ; return

;------------SysTick_Wait10ms------------
; Time delay using busy wait.  This assumes 50 MHz clock
; Input: R0  number of times to wait 10 ms before returning
; Output: none
; Modifies: R0
DELAY10MS             EQU 800000    ; clock cycles in 10 ms (assumes 80 MHz clock)
SysTick_Wait10ms
    PUSH {R4, LR}                   ; save current value of R4 and LR
    MOVS R4, R0                     ; R4 = R0 = remainingWaits
    BEQ SysTick_Wait10ms_done       ; R4 == 0, done
SysTick_Wait10ms_loop
    LDR R0, =DELAY10MS              ; R0 = DELAY10MS
    BL  SysTick_Wait                ; wait 10 ms
    SUBS R4, R4, #1                 ; R4 = R4 - 1; remainingWaits--
    BHI SysTick_Wait10ms_loop       ; if(R4 > 0), wait another 10 ms
SysTick_Wait10ms_done
    POP {R4, LR}                    ; restore previous value of R4 and LR
    BX  LR                          ; return
	
SysTick_Handler         ; Context switching occurs here
	CPSID I
	PUSH{R0, LR}
	BL   DisableInterrupts         ; 2) Prevent interrupt during switch
	POP{R0, LR}
	LDR R2, = PF2                 ; Toggle pin for timing and increment counter
	;LDR R2, [R2]
	MOV R3, #0
	STR R3, [R2]
	LDR R0, = Counts
	LDR R1, [R0]
	ADD R1, #1
	STR R1, [R0]
    

    PUSH    {R4-R11}  ; 3) Save remaining regs r4-11
	LDR R0, = Counts
	LDR R1, [R0]
	ADD R1, #1
	STR R1, [R0]
    LDR     R0,=Runptr  ; 4) R0=pointer to RunI, old thread
    LDR     R1,[R0]   ;    R1 = RunI
	STR     SP, [R1]    ; save current stack pointer
	;ADD     R1, #4
	;LDR     R1, [R1]
    ;STR     R1, [R0]
	PUSH {R0, LR, R2, R3, R1}
    BL  Scheduler
	POP  {R0, LR, R2, R3, R1}
	LDR  R1, [R0]
	LDR  SP, [R1]       ; load dstack pointer for new thread

    POP     {R4-R11}  ; 8) restore regs r4-11
	LDR R2, = PF2
	;LDR R2, [R2]
	MOV R3, #4
	STR R3, [R2]

	PUSH{R0, LR}
	BL   EnableInterrupts         ; 2) Prevent interrupt during switch
	POP{R0, LR}	
	BX LR

launch_first_thread      ; launches the first thread on OS_Launch

	LDR R1, = Runptr
	LDR R1, [R1]
	LDR SP, [R1]
	POP{R4-R11}
	POP{R0-R3}
	POP{R12}
	ADD SP, #4
	LDR R14, [SP]
	ADD SP, #4
	ADD SP, #4
	CPSIE I
	BX LR
	

    ALIGN                           ; make sure the end of this section is aligned
    END                             ; end of file
