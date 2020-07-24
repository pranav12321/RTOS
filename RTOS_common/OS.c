// *************os.c**************
// High-level OS functions


#include <stdint.h>
#include <stdio.h>
#include "../inc/tm4c123gh6pm.h"
#include "../inc/CortexM.h"
#include "../inc/PLL.h"
#include "../inc/LaunchPad.h"
#include "../inc/Timer1A.h"
#include "../inc/Timer2A.h"
#include "../inc/Timer3A.h"
#include "../inc/Timer4A.h"
#include "../inc/Timer5A.h"
#include "../inc/Timer2A.h"
#include "../inc/WTimer0A.h"
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/ST7735.h"
#include "../inc/ADCT0ATrigger.h"
#include "../RTOS_Labs_common/UART0int.h"
#include "../RTOS_Labs_common/heap.h"
#include "../RTOS_Labs_common/eFile.h"
#include "../inc/FIFO.h"




// Performance Measurements 
int32_t MaxJitter;             // largest time jitter between interrupts in usec
#define JITTERSIZE 64
uint32_t const JitterSize=JITTERSIZE;
uint32_t JitterHistogram[JITTERSIZE]={0,};
uint8_t performance=0;
#define NUM_THREADS    6
#define NUM_PROCESSES  4
#define MAX_STACK_SIZE 600
#define NUM_PERIODIC_THREADS 4

TCB* Runptr;
int STACK[NUM_THREADS][MAX_STACK_SIZE];
TCB threads[NUM_THREADS];
PCB processes[NUM_PROCESSES];
PThread periodic_threads[NUM_PERIODIC_THREADS];


void stack_init(void);
void SysTick_Init(uint32_t);
void launch_first_thread(void);
void Scheduler(void);

void periodic_threads_init(void);
void ContextSwitch(void);
void (*PeriodicTaskPF4)(void);   // user function
void (*PeriodicTaskPF2)(void);   // user function
void Debug(void);
void Debug2();
void set_R9(uint32_t*);

void System_Timer(void){
}


extern uint32_t Count3;
extern uint32_t  Count4;
extern uint32_t Count5;
uint32_t Disabled =0;
uint32_t Disabled_usecs = 0;
uint32_t Enabled =0;
uint32_t Start_disable=0;
uint32_t Disabled_percentage;
uint32_t Stop_disable=0;
uint32_t Start_disable_flag=0;
uint32_t Stop_enable=0;
//AddIndexFifo(Queue,1024,int, 1,0)
#define SIZE 32
#define SUCCESS 1
#define FAIL    0

uint32_t volatile QueuePutI;    
uint32_t volatile QueueGetI;    
uint32_t static Queue_Fifo [SIZE]; 

//Initialize Index based OS FIFO;
void QueueFifo_Init(void){ long sr;  
  sr = StartCritical();                 
  QueuePutI = QueueGetI = 0;      // set put and get indices to 0;
  EndCritical(sr);                      
}           
//Put an element into fifo and return SUCCESS if not full and return FAIL is fifo is full
int QueueFifo_Put (int data){   
  if(( QueuePutI - QueueGetI ) & ~(SIZE-1)){ 
			//UART_OutUDec(OS_Fifo_Size());
	//UART_OutString("\n\r");
    return(FAIL);  
		
  }                    
  Queue_Fifo[ QueuePutI &(SIZE-1)] = data; 
  QueuePutI ++; 
  return(SUCCESS);     
}  

//get an element and return success if not empty and return fail if empty 
int QueueFifo_Get (uint32_t *datapt){ 
  if( QueuePutI == QueueGetI ){ 
    return(FAIL);      
  }                    
  *datapt = Queue_Fifo[ QueueGetI &(SIZE-1)];  
  QueueGetI++;  
  return(SUCCESS);     
}


//MailBox struct for data and semaphores
typedef struct mmBox {
     int     Data;
		 Sema4Type NotFull;
		 Sema4Type NotEmpty;
}  mbox_t;

static mbox_t OS_MailBox;


//Return OS fifo size
unsigned short QueueFifo_Size (void){  
 return ((unsigned short)( QueuePutI - QueueGetI ));  
}

void stack_init(){
	for(int i=0; i<NUM_THREADS; i++){
		STACK[i][MAX_STACK_SIZE-16]=4;
		STACK[i][MAX_STACK_SIZE-15]=5;
		STACK[i][MAX_STACK_SIZE-14]=6;
		STACK[i][MAX_STACK_SIZE-13]=7;       // Set register values to register numbers initially for debugging
		STACK[i][MAX_STACK_SIZE-12]=8;
		STACK[i][MAX_STACK_SIZE-11]=0x20005B78;
		STACK[i][MAX_STACK_SIZE-10]=10;
		STACK[i][MAX_STACK_SIZE-9]=11;
		STACK[i][MAX_STACK_SIZE-8]=0;
		STACK[i][MAX_STACK_SIZE-7]=1;
		STACK[i][MAX_STACK_SIZE-6]=2;
		STACK[i][MAX_STACK_SIZE-5]=3;
		STACK[i][MAX_STACK_SIZE-4]=12;
		STACK[i][MAX_STACK_SIZE-3]=14;
		STACK[i][MAX_STACK_SIZE-1]=0x01000000;
		(threads[i].sp) = &STACK[i][MAX_STACK_SIZE-16];
		(threads[i].next) = &threads[(i+1)%NUM_THREADS]; 
		if(i==0){
			threads[i].prev = &threads[NUM_THREADS-1];
		}   
		else{
			threads[i].prev = &threads[i-1];
		}
//Initialize next pointer to next stack block in the contiguous allocation
		threads[i].status = THREAD_FREE;
		threads[i].id=i;
		threads[i].num_children=0;
		threads[i].is_process_root_thread=0;
		threads[i].root_process = 0;
		threads[i].process_created = 0;
		threads[i].parent = 0;
		threads[i].sleep_time_usecs = 0;
		//threads[i].childre
	}
	
}


/*------------------------------------------------------------------------------
  Systick Interrupt Handler
  SysTick interrupt happens every 10 ms
  used for preemptive thread switch
 *------------------------------------------------------------------------------*/
extern uint32_t Counts;
//void SysTick_Handler(void) {
//  //PF2 ^= 0x04;                // toggle PF2
//  //PF2 ^= 0x04;                // toggle PF2
//  Counts = Counts + 1;
////  PF2 ^= 0x04;                // toggle PF2
//  __asm{
//		CPSID I
//		PUSH {R4-R11}
//		LDR R0, = Runptr
//		LDR R1, [R0]
//	}		
//  
//} // end SysTick_Handler

unsigned long OS_LockScheduler(void){
  // lab 4 might need this for disk formating
  return 0;
}
void OS_UnLockScheduler(unsigned long previous){
  // lab 4 might need this for disk formating
}


void Scheduler(void){
	
	//PRIORITY SCHEDULER
	if(Runptr->block_semaphore == 0){
	Runptr->WorkingPriority = Runptr->FixedPriority;         // Set current priority back to original if the thread is not blocking on semaphore
	}
	Runptr->Age = 0;  // The thread that just ran now has age 0, it completed its slice.
	TCB* pt = Runptr;            
	TCB* bestPt = Runptr;
	pt = Runptr;
	uint32_t max = 255;
	for(int i=0; i<NUM_THREADS; i++){
		if(threads[i].status==THREAD_SLEEP){
			if(threads[i].sleep_time_usecs <= 0){
				threads[i].status = THREAD_RUNNING_ACTIVE;                 // Increment sleep time of sleeping threads.
			}
			if(threads[i].sleep_time_usecs > 0){
				threads[i].sleep_time_usecs -= CURRENT_CONTEXT_SWITCH_PERIOD_USECS;
			}
			

		}
		
		if(threads[i].status==THREAD_RUNNING_ACTIVE){
			threads[i].Age += 2;
			if((threads[i].Age>=10) && ((threads[i].Age%10)==0) && (threads[i].WorkingPriority>0)){  // Aging. Boost priority if thread hasn't been scheduled in 10ms
				//UART_OutUDec(threads[i].id);
				//UART_OutString("\n\r");
				threads[i].WorkingPriority -=1;
			}

		}
	}
	pt = pt->next;
	//Runptr = Runptr->next;
	while(pt!=Runptr){
		if((pt->WorkingPriority<max) && (pt->status==THREAD_RUNNING_ACTIVE)){   // maximum Priority running thread s set as best pointer
			bestPt = pt;
			max = pt->WorkingPriority;
			
		}
		pt = pt->next;
		
	}
	pt = bestPt->next;
	if(bestPt->status == THREAD_BLOCKED){                 //If best pointer is blocked, use the next running thread
		while(pt!=bestPt){
			if(pt->status == THREAD_RUNNING_ACTIVE){
				bestPt = pt;
			}
			
		}
	}
	Runptr = bestPt;


////Round robin scheduler
//	for(int i=0; i<NUM_THREADS; i++){
//		if(threads[i].status==THREAD_SLEEP){
//			if(threads[i].sleep_time_usecs <= 0){
//				threads[i].status = THREAD_RUNNING_ACTIVE;
//			}
//			if(threads[i].sleep_time_usecs > 0){                                   //Update sleep times
//				threads[i].sleep_time_usecs -= CURRENT_CONTEXT_SWITCH_PERIOD_USECS;
//			}
//			

//		}
//	}
//	Runptr = Runptr->next;
//	while(Runptr->status==THREAD_SLEEP || Runptr->status==THREAD_BLOCKED || Runptr->status==THREAD_FREE){  //Schedule next running thread regardless of priority
//		Runptr = Runptr->next;
//	}
}

void periodic_threads_init(){
	for(int i=0; i<NUM_PERIODIC_THREADS; i++){
		periodic_threads[i].current_count=0;
		periodic_threads[i].is_active=0;
	}
}

/**
 * @details  Initialize operating system, disable interrupts until OS_Launch.
 * Initialize OS controlled I/O: serial, ADC, systick, LaunchPad I/O and timers.
 * Interrupts not yet enabled.
 * @param  none
 * @return none
 * @brief  Initialize OS
 */
void OS_Init(void){
	DisableInterrupts();
	stack_init();
	Runptr = &threads[0];
	NVIC_SYS_PRI3_R = (NVIC_SYS_PRI3_R&0x00FFFFFF)|0xE0000000;
	NVIC_SYS_PRI3_R = (NVIC_SYS_PRI3_R&0xFF00FFFF)|0x00F00000;
	Timer2A_Init(&System_Timer, 0xFFFFFFFF, 6);
	
	for(int i=0; i<4; i++){
	periodic_threads[i].is_active = 0;
	}
	periodic_threads[0].timer_init = &Timer1A_Init;
	periodic_threads[1].timer_init = &Timer3A_Init;        // Allocate a timer for each periodic thread. Supports 4 periodic tasks currently.
	periodic_threads[2].timer_init = &Timer4A_Init;
	periodic_threads[3].timer_init = &Timer5A_Init;
	
	

}; 

// ******** OS_InitSemaphore ************
// initialize semaphore 
// input:  pointer to a semaphore
// output: none
void OS_InitSemaphore(Sema4Type *semaPt, int32_t value){
	(*semaPt).Value=value;

}; 

// ******** OS_Wait ************
// decrement semaphore 
// Lab2 spinlock
// Lab3 block if less than zero
// input:  pointer to a counting semaphore
// output: none
void OS_Wait(Sema4Type *semaPt){

	DisableInterrupts();
	(*semaPt).Value--;             // Decrement semaPt value to indicate thread needs to acquire the semaphore.
	
	if(((*semaPt).Value)<0){
		Runptr->block_semaphore = semaPt;
		Runptr->status = THREAD_BLOCKED;             // If semaphore is in use, set status to BLOCKED on the pointer to semaphore and yield
		
		//EnableInterrupts();
		OS_Suspend();
		//DisableInterrupts();
		//UART_OutUDec((*semaPt).Value);
		//int x = 2;
	}
	//Runptr->WorkingPriority=0;                   
	//(*semaPt).Value = ((*semaPt).Value) -1;
	
	EnableInterrupts();
	
	
	
	
	
//		while(((*semaPt).Value)<=0){
//		EnableInterrupts();
//		OS_Suspend();
//		DisableInterrupts();
//		//UART_OutUDec((*semaPt).Value);            // Spinlock semaphore implementation
//		//int x = 2;
//	}
//	(*semaPt).Value = ((*semaPt).Value) -1;
//	
//	EnableInterrupts();

  
}; 

// ******** OS_Signal ************
// increment semaphore 
// Lab2 spinlock
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a counting semaphore
// output: none
void OS_Signal(Sema4Type *semaPt){

	TCB *pt;
	DisableInterrupts();
	((*semaPt).Value)++;
	if(((*semaPt).Value)<=0){
		pt = Runptr->next;
		while(pt ->block_semaphore != semaPt){            // Iterate through all threads and check for one blocked on the current semaphore. 
			pt = pt->next;
		}
		pt->block_semaphore=0;					
		//pt->WorkingPriority = pt->FixedPriority;               
		pt->status = THREAD_RUNNING_ACTIVE;                // Set the status to running, and semaphore pointer to null.
	}
	EnableInterrupts();
	
	
	
	
	
	
//		DisableInterrupts();
//	((*semaPt).Value)++;
//	EnableInterrupts();

}; 

// ******** OS_bWait ************
// Lab2 spinlock, set to 0
// Lab3 block if less than zero
// input:  pointer to a binary semaphore
// output: none
void OS_bWait(Sema4Type *semaPt){
	DisableInterrupts();
	if((*semaPt).Value==0){
		Runptr->block_semaphore = semaPt;
		Runptr->status = THREAD_BLOCKED;
		EnableInterrupts();
		OS_Suspend();
	}
	(*semaPt).Value = 0;
	EnableInterrupts();

}; 

// ******** OS_bSignal ************
// Lab2 spinlock, set to 1
// Lab3 wakeup blocked thread if appropriate 
// input:  pointer to a binary semaphore
// output: none
void OS_bSignal(Sema4Type *semaPt){

	DisableInterrupts();
	TCB* pt;
	(*semaPt).Value=1;
		pt = Runptr->next;
		while(pt ->block_semaphore != semaPt){
			pt = pt->next;
			if(pt==Runptr)
				break;
		}
		pt->block_semaphore=0;
		pt->status = THREAD_RUNNING_ACTIVE;
	
	EnableInterrupts();

}; 



//******** OS_AddThread *************** 
// add a foregound thread to the scheduler
// Inputs: pointer to a void/void foreground task
//         number of bytes allocated for its stack
//         priority, 0 is highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// stack size must be divisable by 8 (aligned to double word boundary)
// In Lab 2, you can ignore both the stackSize and priority fields
// In Lab 3, you can ignore the stackSize fields
int OS_AddThread(void(*task)(void), 
   uint32_t stackSize, uint32_t priority){
		 long st = StartCritical();

		for(int i =0; i<NUM_THREADS; i++){
			if(threads[i].status==THREAD_FREE){
				STACK[i][MAX_STACK_SIZE-2]=(uint32_t)task;
				STACK[i][MAX_STACK_SIZE-16]=4;
				STACK[i][MAX_STACK_SIZE-15]=5;
				STACK[i][MAX_STACK_SIZE-14]=6;
				STACK[i][MAX_STACK_SIZE-13]=7;
				STACK[i][MAX_STACK_SIZE-12]=8;
				STACK[i][MAX_STACK_SIZE-11]=0x20005B78;
				STACK[i][MAX_STACK_SIZE-10]=10;
				STACK[i][MAX_STACK_SIZE-9]=11;
				STACK[i][MAX_STACK_SIZE-8]=0;
				STACK[i][MAX_STACK_SIZE-7]=1;
				STACK[i][MAX_STACK_SIZE-6]=2;
				STACK[i][MAX_STACK_SIZE-5]=3;
				STACK[i][MAX_STACK_SIZE-4]=12;                      // Set the value equal to register number for debugging
				STACK[i][MAX_STACK_SIZE-3]=14;
				STACK[i][MAX_STACK_SIZE-1]=0x01000000;
				(threads[i].sp) = &STACK[i][MAX_STACK_SIZE-16];
				(threads[i].next) = &threads[(i+1)%NUM_THREADS];
				//threads[i].status = THREAD_FREE;
				threads[i].WorkingPriority = priority;
				threads[i].FixedPriority   = priority;               // Initial priority is the fixed priority
				threads[i].Age = 0;
				threads[i].id=i;
				threads[i].status=THREAD_RUNNING_ACTIVE;
				threads[i].parent = Runptr;
				threads[i].num_children = 0;
				threads[i].parent->children[threads[i].parent->num_children] = &threads[i];
				threads[i].parent->num_children++;
				threads[i].process_creator = 0;
				threads[i].is_process_root_thread=0;
				threads[i].root_process = Runptr->root_process;
				if(Runptr->process_creator==1){
				threads[i].is_process_root_thread=1;
				threads[i].root_process = Runptr->process_created;

				}
				
				//EnableInterrupts();
				Count4 =i;
				EndCritical(st);
				
				return 1;
			}
			
		}
	Count4=0;
  EndCritical(st);
	
  return 0; 
};
	 
//******** OS_AddProcess *************** 
// add a process with foregound thread to the scheduler
// Inputs: pointer to a void/void entry point
//         pointer to process text (code) segment
//         pointer to process data segment
//         number of bytes allocated for its stack
//         priority (0 is highest)
// Outputs: 1 if successful, 0 if this process can not be added
// This function will be needed for Lab 5
// In Labs 2-4, this function can be ignored
int OS_AddProcess(void(*entry)(void), void *text, void *data, 
  unsigned long stackSize, unsigned long priority){
		int flag=-1;
		DisableInterrupts();
		Runptr->process_creator = 1;
		
	for(int i=0; i<NUM_PROCESSES; i++){    // Set active process to check when creating thread
		if(processes[i].is_active==0){
			processes[i].is_active=1;
			processes[i].num_children=0;
			processes[i].code_segment=text;
			processes[i].data_segment=data;
			flag=i;
			break;
		}
	}
	if(flag==-1){
		return 0;
	}
	Runptr->process_created = &processes[flag];
  int root_thread = OS_AddThread(entry, stackSize, priority);
	
	if(root_thread==0){
		processes[flag].is_active=0;               // Fail if thread creation fails
		Runptr->process_creator = 0;
		return 0;
	}
	Runptr->process_created = &processes[flag];   
	
	
	//Runptr->is_process_root_thread = 1;
  set_R9(data);                // R9 for Data offset
	EnableInterrupts();
	
     
  return 1; 
}


//******** OS_Id *************** 
// returns the thread ID for the currently running thread
// Inputs: none
// Outputs: Thread ID, number greater than zero 
uint32_t OS_Id(void){
  
  return Runptr->id; 
};


//******** OS_AddPeriodicThread *************** 
// add a background periodic task
// typically this function receives the highest priority
// Inputs: pointer to a void/void background function
//         period given in system time units (12.5ns)
//         priority 0 is the highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// You are free to select the time resolution for this function
// It is assumed that the user task will run to completion and return
// This task can not spin, block, loop, sleep, or kill
// This task can call OS_Signal  OS_bSignal   OS_AddThread
// This task does not have a Thread ID
// In lab 1, this command will be called 1 time
// In lab 2, this command will be called 0 or 1 times
// In lab 2, the priority field can be ignored
// In lab 3, this command will be called 0 1 or 2 times
// In lab 3, there will be up to four background threads, and this priority field 
//           determines the relative priority of these four threads

// Just one timer jused for lab 2. Will be made generic and more availiable timers for lab3.
int OS_AddPeriodicThread(void(*task)(void), 
   uint32_t period, uint32_t priority){

		 DisableInterrupts();
		 for(int i=0; i<NUM_PERIODIC_THREADS; i++){
			 if(periodic_threads[i].is_active==0){
				 periodic_threads[i].current_count=period;                        // Search for a free timer, and initialize the timer with period
				 periodic_threads[i].period=period;                               // and priority for the periodic thread.
				 periodic_threads[i].is_active=1;
				 periodic_threads[i].periodic_function = task;
				 (*periodic_threads[i].timer_init)(task, period, priority);
				  EnableInterrupts();
			    return 1;		 
			 }

			 
		 }
		 
  
  EnableInterrupts(); 
  return 0; 
//		 DisableInterrupts();
//		 Timer4A_Init(task, period, priority);
//		 EnableInterrupts();
};





/*----------------------------------------------------------------------------
  PF1 Interrupt Handler
 *----------------------------------------------------------------------------*/
void GPIOPortF_Handler(void){
	//while(1){}
	//DisableInterrupts();
//		 if(Runptr->status==THREAD_FREE){
//			 DisableInterrupts();            // A thread kill race condition if this happens
//			 while(1){};
//		 }
 //Debug();
 if((GPIO_PORTF_RIS_R & 0x10) == 0x10){
	GPIO_PORTF_ICR_R = 0x10;      // acknowledge flag4
	 
	(*PeriodicTaskPF4)();               // execute user task
	// Debug();
	
 }
 if((GPIO_PORTF_RIS_R & 0x01) == 0x01){
	GPIO_PORTF_ICR_R = 0x01;      // acknowledge flag4
	(*PeriodicTaskPF2)();               // execute user task
 }
 //Count5++;
 GPIO_PORTF_IM_R &= ~0x02;
 //Debug2();
 
 //EnableInterrupts();
 
}
//******** OS_AddSW1Task *************** 
// add a background task to run whenever the SW1 (PF4) button is pushed
// Inputs: pointer to a void/void background function
//         priority 0 is the highest, 5 is the lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// It is assumed that the user task will run to completion and return
// This task can not spin, block, loop, sleep, or kill
// This task can call OS_Signal  OS_bSignal   OS_AddThread
// This task does not have a Thread ID
// In labs 2 and 3, this command will be called 0 or 1 times
// In lab 2, the priority field can be ignored
// In lab 3, there will be up to four background threads, and this priority field 
//           determines the relative priority of these four threads
int OS_AddSW1Task(void(*task)(void), uint32_t priority){
	DisableInterrupts();
   SYSCTL_RCGC2_R |= 0x00000020; // (a) activate clock for port F

//  FallingEdges = 0;             // (b) initialize count and wait for clock
  GPIO_PORTF_DIR_R &= ~0x10;    // (c) make PF4 in (built-in button)

  GPIO_PORTF_AFSEL_R &= ~0x10;  //     disable alt funct on PF4

  GPIO_PORTF_DEN_R |= 0x10;     //     enable digital I/O on PF4

  GPIO_PORTF_PCTL_R &= ~0x000F0000; //  configure PF4 as GPIO

  GPIO_PORTF_AMSEL_R &= ~0x10;  //    disable analog functionality on PF4

  GPIO_PORTF_PUR_R |= 0x10;     //     enable weak pull-up on PF4

  GPIO_PORTF_IS_R &= ~0x10;     // (d) PF4 is edge-sensitive

  GPIO_PORTF_IBE_R &= ~0x10;    //     PF4 is not both edges

  GPIO_PORTF_IEV_R &= ~0x10;    //     PF4 falling edge event

  GPIO_PORTF_ICR_R = 0x10;      // (e) clear flag4

  GPIO_PORTF_IM_R |= 0x10;      // (f) arm interrupt on PF4

  NVIC_PRI7_R = (NVIC_PRI7_R&0xFF00FFFF)|(priority<<21); // (g) priority 5

  NVIC_EN0_R = 0x40000000;      // (h) enable interrupt 30 in NVIC
	PeriodicTaskPF4 = task;

  EnableInterrupts();           // (i) Enable global Interrupt flag (I)
	return 1;

}


//******** OS_AddSW2Task *************** 
// add a background task to run whenever the SW2 (PF0) button is pushed
// Inputs: pointer to a void/void background function
//         priority 0 is highest, 5 is lowest
// Outputs: 1 if successful, 0 if this thread can not be added
// It is assumed user task will run to completion and return
// This task can not spin block loop sleep or kill
// This task can call issue OS_Signal, it can call OS_AddThread
// This task does not have a Thread ID
// In lab 2, this function can be ignored
// In lab 3, this command will be called will be called 0 or 1 times
// In lab 3, there will be up to four background threads, and this priority field 
//           determines the relative priority of these four threads
int OS_AddSW2Task(void(*task)(void), uint32_t priority){
 DisableInterrupts();
   SYSCTL_RCGC2_R |= 0x00000020; // (a) activate clock for port F

//  FallingEdges = 0;             // (b) initialize count and wait for clock
	GPIO_PORTF_LOCK_R = 0x4C4F434B;   // 2) unlock GPIO Port F
  GPIO_PORTF_DIR_R &= ~0x01;    // (c) make PF2 in (built-in button)

  GPIO_PORTF_AFSEL_R &= ~0x01;  //     disable alt funct on PF0

  GPIO_PORTF_DEN_R |= 0x01;     //     enable digital I/O on PF0

  GPIO_PORTF_PCTL_R &= ~0x0000000F; //  configure PF0 as GPIO

  GPIO_PORTF_AMSEL_R &= ~0x01;  //    disable analog functionality on PF0

  GPIO_PORTF_PUR_R |= 0x01;     //     enable weak pull-up on PF0

  GPIO_PORTF_IS_R &= ~0x01;     // (d) PF0 is edge-sensitive

  GPIO_PORTF_IBE_R &= ~0x01;    //     PF0 is not both edges

  GPIO_PORTF_IEV_R &= ~0x01;    //     PF0 falling edge event

  GPIO_PORTF_ICR_R = 0x01;      // (e) clear flag4

  GPIO_PORTF_IM_R |= 0x01;      // (f) arm interrupt on PF0

  NVIC_PRI7_R = (NVIC_PRI7_R&0xFF00FFFF)|(priority<<21); // (g) priority 5

  NVIC_EN0_R = 0x40000000;      // (h) enable interrupt 30 in NVIC
	PeriodicTaskPF2 = task;

  EnableInterrupts();           // (i) Enable global Interrupt flag (I)   
  return 1;
};

// ******** OS_Sleep ************
// place this thread into a dormant state
// input:  number of msec to sleep
// output: none
// You are free to select the time resolution for this function
// OS_Sleep(0) implements cooperative multitasking
void OS_Sleep(uint32_t sleepTime){
	DisableInterrupts();
	Runptr->status= THREAD_SLEEP;
	Runptr->sleep_time_usecs = sleepTime*1000;    // Set sleep status, time in micros and yield
	EnableInterrupts();
	ContextSwitch();
  

};  

// ******** OS_Kill ************
// kill the currently running thread, release its TCB and stack
// input:  none
// output: none
void OS_Kill(void){
	DisableInterrupts();
	//UART_OutUDec(OS_Id());
	//UART_OutString(" killed \n\r");
	if(Runptr->is_process_root_thread==1){
		for(int i=0; i<Runptr->num_children; i++){
			Runptr->children[i]->status = THREAD_FREE;
		}
		Heap_Free(Runptr->root_process->data_segment);
		Heap_Free(Runptr->root_process->code_segment);
		
	}
  Runptr->status = THREAD_FREE;          // Set status to free and yield
  //EnableInterrupts();   // end of atomic section 
	OS_Suspend();
  //for(;;){};        // can not return
    
}; 

// ******** OS_Suspend ************
// suspend execution of currently running thread
// scheduler will choose another thread to execute
// Can be used to implement cooperative multitasking 
// Same function as OS_Sleep(0)
// input:  none
// output: none
void OS_Suspend(void){
	//Runptr->status = THREAD_SUSPENDED;
	//OS_Sleep(0);
	ContextSwitch();         // Trigger PendSV interrupt
  

};
  
// ******** OS_Fifo_Init ************
// Initialize the Fifo to be empty
// Inputs: size
// Outputs: none 
// In Lab 2, you can ignore the size field
// In Lab 3, you should implement the user-defined fifo size
// In Lab 3, you can put whatever restrictions you want on size
//    e.g., 4 to 64 elements
//    e.g., must be a power of 2,4,8,16,32,64,128
void OS_Fifo_Init(uint32_t size){
	 QueueFifo_Init();
  
}


// ******** OS_Fifo_Put ************
// Enter one data sample into the Fifo
// Called from the background, so no waiting 
// Inputs:  data
// Outputs: true if data is properly saved,
//          false if data not saved, because it was full
// Since this is called by interrupt handlers 
//  this function can not disable or enable interrupts
int OS_Fifo_Put(uint32_t data){
    return QueueFifo_Put(data); 
};  

// ******** OS_Fifo_Get ************
// Remove one data sample from the Fifo
// Called in foreground, will spin/block if empty
// Inputs:  none
// Outputs: data 
uint32_t OS_Fifo_Get(void){
  uint32_t data;
  while(QueueFifo_Get(&data)==0){}       // wait till it receives an element
  return data; 
};

// ******** OS_Fifo_Size ************
// Check the status of the Fifo
// Inputs: none
// Outputs: returns the number of elements in the Fifo
//          greater than zero if a call to OS_Fifo_Get will return right away
//          zero or less than zero if the Fifo is empty 
//          zero or less than zero if a call to OS_Fifo_Get will spin or block
int32_t OS_Fifo_Size(void){
	//int* data;
  //int result = QueueFifo_Get(data);
  return QueueFifo_Size(); 
};


// ******** OS_MailBox_Init ************
// Initialize communication channel
// Inputs:  none
// Outputs: none
void OS_MailBox_Init(void){
     OS_InitSemaphore(&(OS_MailBox.NotFull), 1);
     OS_InitSemaphore(&(OS_MailBox.NotEmpty), 0);
     OS_MailBox.Data = 0;
};

// ******** OS_MailBox_Send ************
// enter mail into the MailBox
// Inputs:  data to be sent
// Outputs: none
// This function will be called from a foreground thread
// It will spin/block if the MailBox contains data not yet received 
void OS_MailBox_Send(uint32_t data){

     OS_Wait(&(OS_MailBox.NotFull));
     OS_MailBox.Data = data;
     OS_Signal(&(OS_MailBox.NotEmpty));   

};

// ******** OS_MailBox_Recv ************
// remove mail from the MailBox
// Inputs:  none
// Outputs: data received
// This function will be called from a foreground thread
// It will spin/block if the MailBox is empty 
uint32_t OS_MailBox_Recv(void){

      OS_Wait(&(OS_MailBox.NotEmpty));
     uint32_t recv = OS_MailBox.Data;
     OS_Signal(&(OS_MailBox.NotFull));
     return (recv);
};

// ******** OS_Time ************
// return the system time 
// Inputs:  none
// Outputs: time in 12.5ns units, 0 to 4294967295
// The time resolution should be less than or equal to 1us, and the precision 32 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_TimeDifference have the same resolution and precision 
uint32_t OS_Time(void){
	//return Counts;
	
  return 4294967295- TIMER2_TAV_R; // replace this line with solution
};

// ******** OS_TimeDifference ************
// Calculates difference between two times
// Inputs:  two times measured with OS_Time
// Outputs: time difference in 12.5ns units 
// The time resolution should be less than or equal to 1us, and the precision at least 12 bits
// It is ok to change the resolution and precision of this function as long as 
//   this function and OS_Time have the same resolution and precision 
uint32_t OS_TimeDifference(uint32_t start, uint32_t stop){
	if(start<=stop){
		return stop-start;
	}
		return stop+ 4294967295-start;
};


// ******** OS_ClearMsTime ************
// sets the system time to zero (solve for Lab 1), and start a periodic interrupt
// Inputs:  none
// Outputs: none
// You are free to change how this works
void OS_ClearMsTime(void){
	//SysTick_Init(160000);
	Counts=0;

};

// ******** OS_MsTime ************
// reads the current time in msec (solve for Lab 1)
// Inputs:  none
// Outputs: time in ms units
// You are free to select the time resolution for this function
// For Labs 2 and beyond, it is ok to make the resolution to match the first call to OS_AddPeriodicThread
uint32_t OS_MsTime(void){
  return Counts;
};



// Performance measure functgions to measure the percentage time interrupts are disabled
void Capture_dis_start(){
	if(Start_disable_flag==0 && performance){
		Start_disable_flag = 1;
		Start_disable = OS_Time();
	}
	
}

void Capture_dis_stop(){
	if(performance){
		if(Start_disable_flag==1){
		Start_disable_flag = 0;
		Stop_disable = OS_Time();
	
		Disabled += OS_TimeDifference(Start_disable, Stop_disable);
	  Disabled_usecs = Disabled/80;
		Disabled_percentage = (Disabled*100)/(OS_Time());
		}
	}
}

// Either activate or disactivate performance measure checks.
void enable_performance(){
	performance=1;
}
void disable_performance(){
	performance=0;
}

//******** OS_Launch *************** 
// start the scheduler, enable interrupts
// Inputs: number of 12.5ns clock cycles for each time slice
//         you may select the units of this parameter
// Outputs: none (does not return)
// In Lab 2, you can ignore the theTimeSlice field
// In Lab 3, you should implement the user-defined TimeSlice field
// It is ok to limit the range of theTimeSlice to match the 24-bit SysTick
void OS_Launch(uint32_t theTimeSlice){
	SysTick_Init(theTimeSlice);
	launch_first_thread();
  
    
};



//************** I/O Redirection *************** 
// redirect terminal I/O to UART or file (Lab 4)

int StreamToDevice=0;                // 0=UART, 1=stream to file (Lab 4)

//int fputc (int ch, FILE *f) { 
//  if(StreamToDevice==1){  // Lab 4
//    if(eFile_Write(ch)){          // close file on error
//       OS_EndRedirectToFile(); // cannot write to file
//       return 1;                  // failure
//    }
//    return 0; // success writing
//  }
//  
//  // default UART output
//  UART_OutChar(ch);
//  return ch; 
//}

//int fgetc (FILE *f){
//  char ch = UART_InChar();  // receive from keyboard
//  UART_OutChar(ch);         // echo
//  return ch;
//}

int OS_RedirectToFile(const char *name){  // Lab 4
  eFile_Create(name);              // ignore error if file already exists
  if(eFile_WOpen(name)) return 1;  // cannot open file
  StreamToDevice = 1;
  return 0;
}

int OS_EndRedirectToFile(void){  // Lab 4
  StreamToDevice = 0;
  if(eFile_WClose()) return 1;    // cannot close file
  return 0;
}

int OS_RedirectToUART(void){
  StreamToDevice = 0;
  return 0;
}

int OS_RedirectToST7735(void){
  
  return 1;
}

