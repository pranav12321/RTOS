// *************Interpreter.c**************

#include <stdint.h>
#include <string.h> 
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h> 
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/ST7735.h"
#include "../RTOS_Lab5_ProcessLoader/loader.h"
#include "../inc/ADCT0ATrigger.h"
#include "../inc/ADCSWTrigger.h"
#include "../RTOS_Labs_common/UART0int.h"
#include "../RTOS_Labs_common/eDisk.h"
#include "../RTOS_Labs_common/eFile.h"
#include "../RTOS_Labs_common/ST7735.h"
#include "../RTOS_Labs_common/OS.h"
#include "../inc/tm4c123gh6pm.h"
#include "../inc/CortexM.h"
#include "../RTOS_Labs_common/esp8266.h"
#include "../inc/LaunchPad.h"


// Command protocols (will be extended through)
#define PROTO "lcd_out"
#define ADCIN "adcin"
#define CLEAR_SCREEN "cls"
#define START_TIMER "start_timer"
#define GET_TIME_ELAPSED "get_time"
#define RESET_TIME "reset_time"
#define ADC_INIT   "adc_init"
#define JITTER_HIST "jitter_hist"
#define EN_DN_STATS "enable_frac"
#define CPU_UTIL    "cpu_util"
#define FORMAT_FILESYSTEM "format_filesys"
#define PRINT_DIRECTORY "ls"
#define CREATE_FILE     "touch"
#define WRITE_TO_FILE   "fwrite"
#define PRINT_FILE      "fread"
#define DELETE_FILE     "rm"
#define LOAD_ELF        "load"


#define RPC_ADC_IN "0"
#define RPC_OS_Time     "1"
#define RPC_ST7735_Message "2"
#define RPC_eFile_format   "3"
#define RPC_eFile_create   "4"
#define RPC_eFile_read   "5"
#define RPC_eFile_write   "6"
#define RPC_exec_elf     "7"
#define RPC_Toggle_led       "8"

#define RPC_CLIENT_RECEIVE "rpc_client_receive"


//Bounds and Errors (will be extended through)
#define MAXIMUM_COMMAND_WORD_LENGTH 20
#define MAXIMUM_NUM_ARGUMENTS       10

extern int32_t MaxJitter;             // largest time jitter between interrupts in usec
extern uint32_t const JitterSize;
extern uint32_t JitterHistogram[];
extern uint32_t FilterWork;
extern uint32_t Disabled;
extern uint32_t Enabled;
extern uint32_t Disabled_usecs;
extern uint32_t Disabled_percentage;
extern uint32_t CPUUtil;
void decode_and_transmit(char* buffer);
void number_to_string(uint32_t, char*);
void setup_args(char*, char**);

// Print jitter histogram
void Jitter(int32_t MaxJitter, uint32_t const JitterSize, uint32_t JitterHistogram[]){
  // write this for Lab 3 (the latest)
	UART_OutString("Max Jitter = ");
	UART_OutUDec(MaxJitter);
	UART_OutChar('\n');
	UART_OutChar('\r');
	
	UART_OutString("Jitter Size = ");
	UART_OutUDec(JitterSize);
	UART_OutChar('\n');
	UART_OutChar('\r');
	
	UART_OutString("The jitter histogram is \n\r");
	for(int i=0; i<JitterSize; i++){
	//UART_OutString("Max Jitter = ");
	UART_OutUDec(i);
	UART_OutChar(' ');
	UART_OutUDec(JitterHistogram[i]);
	UART_OutChar('\n');
	UART_OutChar('\r');
	}
	
	UART_OutUDec(FilterWork);
	UART_OutChar('\n');
	UART_OutChar('\r');
	
	
	
}

// *********** Command line interpreter (shell written from scratch) ************


// ******** split ************
// parses and splits the input command into desired form
// If command or argument length exceeds size limit, it becomes an invalid command
// and is just skipped (TODO: Send an error message in this case)
// input:  a pointer to arrays of characters which is where the individual tokens are to be stored
//         pointer to characters (array) which is the input line from user           
// output: none
void split(char (*tokens)[MAXIMUM_COMMAND_WORD_LENGTH], char* input){
		int i=0;
		int arg_ctr=0;
		int tmp_ctr=0;
		i=0;
		while(input[i]!='\0' && arg_ctr<=9){
		while(input[i]!=' ' && i<=99){
		tokens[arg_ctr][tmp_ctr]=input[i];
		i++;
		tmp_ctr++;

		}
		tokens[arg_ctr][tmp_ctr]='\0';             
		arg_ctr++;
		i++;
		tmp_ctr=0;
	}

}

int strCmp(char string1[], char string2[] )
{
	for (int i = 0; ; i++){
	 if (string1[i] != string2[i]){
				return string1[i] < string2[i] ? -1 : 1;
		}

		if (string1[i] == '\0'){
				return 0;
		}
	}
}
char* my_itoa(int number, char str[]) {
    //create an empty string to store number
   sprintf(str, "%d", number); //make the number into string using sprintf function
   return str;
}

int string_to_int(char* inp){
return atoi(inp);  // Just returns 0 if no number or invalid number is present
}

// ******** split ************
// Matches the command with the protocol and invokes the appropriate command with the arguments
// Does nothing if command is invalid (TODO: Sens an error message over UART)
// input:  a pointer to arrays of characters which is where the individual tokens are be stored        
// output: none
void decode_exec(char (*tokens)[MAXIMUM_COMMAND_WORD_LENGTH]){
	if(strCmp(tokens[0], PROTO )==0){
	int arg = string_to_int(tokens[1]);
	int arg2 = string_to_int(tokens[2]);
	ST7735_Message(arg, arg2, tokens[3], string_to_int(tokens[4]));

}

if(strCmp(tokens[0], "cls")==0){
}

if(strCmp(tokens[0], START_TIMER)==0){
	OS_ClearMsTime();
}

if(strCmp(tokens[0], GET_TIME_ELAPSED)==0){
	ST7735_Message(0, 0, "Time elapsed =", OS_MsTime());
}

if(strCmp(tokens[0], RESET_TIME)==0){
	OS_ClearMsTime();
}

if(strCmp(tokens[0], ADC_INIT)==0){
	ADC0_InitSWTriggerSeq3_Ch9();
}

if(strCmp(tokens[0], ADCIN)==0){
	ST7735_Message(0, 0, "adc_IN =", ADC0_InSeq3());
}

if(strCmp(tokens[0], JITTER_HIST)==0){
	Jitter(MaxJitter, JitterSize, JitterHistogram);
}

if(strCmp(tokens[0], LOAD_ELF)==0){
	int mount = eFile_Mount();
	int open = eFile_ROpen(tokens[1]);
static const ELFSymbol_t symtab[] = {
{ "ST7735_Message", ST7735_Message }
};
ELFEnv_t env = { symtab, 1 };
	int result =  exec_elf(tokens[1], &env);

}

if(strCmp(tokens[0], EN_DN_STATS)==0){
	UART_OutString("Disabled millisecs: ");
	UART_OutUDec(Disabled/80000);
	UART_OutString("\n\r");
	
	UART_OutString("Disabled percentage: ");
	UART_OutUDec(Disabled_percentage);
	UART_OutString("\n\r");
	
}

if(strCmp(tokens[0], CPU_UTIL)==0){
	UART_OutString("\n\r");
}


if(strCmp(tokens[0], FORMAT_FILESYSTEM)==0){
	eFile_Mount();
	eFile_Init();
	eFile_Format();
	UART_OutString("\n\r");
}

if(strCmp(tokens[0], PRINT_DIRECTORY)==0){
	eFile_DOpen("");
	char *name;
	unsigned long size;
	while(eFile_DirNext(&name, &size)!=1){
		int i=0;
		while(name[i]!=0){
		UART_OutChar(name[i]);
		i++;
		}
		UART_OutString("\n\r");
		UART_OutUDec(size);
		
		UART_OutString("\n\r");
		UART_OutString("\n\r");
		
	};
	UART_OutString("\n\r");
}

if(strCmp(tokens[0], CREATE_FILE)==0){
	int i=0;
	while(tokens[1][i] !=0){
		i++;
	}
	if(i<=7){
	int res = eFile_Create(tokens[1]);
	if(res){
		UART_OutString("Creation failure \n\r");
	}
	else{
		UART_OutString("Creation success \n\r");
	}
	}
	UART_OutString("\n\r");
}

if(strCmp(tokens[0], WRITE_TO_FILE)==0){
		int i=0;
	while(tokens[1][i] !=0){
		i++;
	}
	if(i<=7){
	int res = eFile_WOpen(tokens[1]);
	if(res){
		UART_OutString("Open failure \n\r");
	}
	else{
		UART_OutString("Open success \n\r");
		res = eFile_Write(tokens[2][0]);
			if(res){
				UART_OutString("Write failure \n\r");
			}
			else{
				UART_OutString("Write success \n\r");
				res = eFile_WClose();
			}
	}
	}
	UART_OutString("\n\r");
}

if(strCmp(tokens[0], PRINT_FILE)==0){
			int i=0;
	while(tokens[1][i] !=0){
		i++;
	}
	if(i<=7){
	int res = eFile_ROpen(tokens[1]);
	if(res){
		UART_OutString("Open failure \n\r");
	}
	else{
		UART_OutString("Open success \n\r");
		char ch;
		while( eFile_ReadNext(&ch)!=1){
			UART_OutChar(ch);
		}
		eFile_RClose();

	}
	}
	UART_OutString("\n\r");
}

if(strCmp(tokens[0], DELETE_FILE)==0){
	int res = eFile_Delete(tokens[1]);
	if(res){
	 UART_OutString("Delete failed\n\r");
	}
	else{
		UART_OutString("Delete successful\n\r");
	}
	UART_OutString("\n\r");
}




}

// ******** Interpreter ************
// Reads in command from UART, tokenizes the arguments and decodes and executes them by matching
// the commands to protocols
// input:  None         
// output: none
void Interpreter(){
	while(1){
	char string[MAXIMUM_COMMAND_WORD_LENGTH];
	UART_InString(string, 99);
	char args[MAXIMUM_NUM_ARGUMENTS][MAXIMUM_COMMAND_WORD_LENGTH];
	split(args, string);
	decode_exec(args);
	PF3 ^= 0x08;
	OS_Suspend();
	}
}





