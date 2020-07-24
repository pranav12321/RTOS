// filename ************** eFile.c *****************************
// High-level routines to implement a solid-state disk 


#include <stdint.h>
#include <string.h>
#include "../RTOS_Labs_common/OS.h"
#include "../RTOS_Labs_common/eDisk.h"
#include "../RTOS_Labs_common/eFile.h"
#include <stdio.h>


#define FILE_CLOSED    0
#define FILE_WOPEN      1
#define FILE_ROPEN      2
#define MAX_OPEN_FILES 1
#define MAX_FILE_NAME_SIZE 8
#define MAX_DIRECTORY_NAME_SIZE 8
#define DIRECTORY_ENTRY_SIZE (MAX_FILE_NAME_SIZE+8)
#define MAX_DATA_BYTES_PER_SECTOR 504
// Private Global variables
typedef struct file_info{
	char file_name[MAX_FILE_NAME_SIZE];
	uint32_t file_last_sector_id;
	uint32_t file_first_sector_id;
	uint8_t file_status;
	uint32_t current_file_size;
	uint16_t read_ptr;
	uint32_t total_bytes_read;
	char last_block[512];
	
} file_info;

// Directory entry struct tjhat contains the information in directory entry
//File name, number of bytes
typedef struct dir_file_info{
	char file_name[MAX_FILE_NAME_SIZE];
	uint32_t num_bytes;
} dir_file_info;


// Directory information. Maintains all the directory entries in the current directory
typedef struct directory_info{
	char directory_name[MAX_DIRECTORY_NAME_SIZE];
	uint16_t num_entries;
	uint32_t entry_ptr;
	dir_file_info files[10];
	
} directory_info;



static int open_file_count=0;
static file_info loaded_files[MAX_OPEN_FILES];
static directory_info current_directory_info;


//Private (static) Functions
// check if two character arrays are the same; return 0 if same;
static int strCmp(const char string1[],const char string2[] )
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



// Private helper function to Allocate a free sector 
static DRESULT allocate_free_filesector(uint32_t current_last_filesector){
	int root_directory_buffer[128];
	DRESULT result = eDisk_ReadBlock((BYTE*)root_directory_buffer, 0);  //Directory 
	  if(result!=RES_OK){
			return result;
		}
		int free_block = root_directory_buffer[127];
		if(free_block==0){
			return RES_ERROR;     //No more free sectors
		}
		
		root_directory_buffer[127] = 0;
		int current_last_filesector_buffer[128];
		result = eDisk_ReadBlock((BYTE*)current_last_filesector_buffer, current_last_filesector);  //Directory 
	  if(result!=RES_OK){
			return result;
		}	
		int new_sector[128];
		result = eDisk_ReadBlock((BYTE*)new_sector, free_block);  //Directory 
	  if(result!=RES_OK){
			return result;
		}	
		// FAT approach. Free space manager takes the first free sector link at last entry of root sector
		// Everyh free sector points to the next free sector (this is done in the formatting of file system)
		int next_free_sector = new_sector[0];
		root_directory_buffer[127] = next_free_sector;
		new_sector[0] = 0;
		new_sector[1]=  0;
		
		current_last_filesector_buffer[0] = free_block;
		
		result = eDisk_WriteBlock((BYTE*)current_last_filesector_buffer, current_last_filesector);  //Directory 
	  if(result!=RES_OK){
			return result;
		}	

		result = eDisk_WriteBlock((BYTE*)new_sector, free_block);  //Directory 
	  if(result!=RES_OK){
			return result;
		}	
		result = eDisk_WriteBlock((BYTE*)root_directory_buffer, 0);  //Directory 
	  if(result!=RES_OK){
			return result;
		}	
		loaded_files[0].file_last_sector_id = free_block;

		int* int_ptr_open_file_block = (int*)(loaded_files[0].last_block);
		for(int i=0; i<128; i++){
		int_ptr_open_file_block[i] = new_sector[i];
		}		
//		for(int i=0; i<128; i++){
//			loaded_files[0].last_block[i] = new_sector[i];
//		}
		return RES_OK;
			
}



//---------- eFile_Init-----------------
// Activate the file system, without formating
// Input: none
// Output: 0 if successful and 1 on failure (already initialized)
int eFile_Init(void){ // initialize file system
	DSTATUS result = eDisk_Init(0);  // Init Disk
	if(result==RES_OK){
		return 0;
	}
	// Set all file statuses to closed
	for (int i=0; i<MAX_OPEN_FILES; i++){
		loaded_files[i].file_status = FILE_CLOSED;
	}
  return 1;

}

//---------- eFile_Format-----------------
// Erase all files, create blank directory, initialize free space manager
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Format(void){ // erase disk, add format
	int buffer[128];
	for(int i=0; i<128; i++){    // Free space set to all 1s
		buffer[i]=-1;
	}
		buffer[0] = 0;
		buffer[127]=1; // Last entry of root sector contains the link to first free sector
	  
		DRESULT result = eDisk_WriteBlock((const BYTE*)buffer, 0);  //Directory 
																																//last entry points to first free block
		if(result!=RES_OK)
			return 1;
		
	// Formats the FAT approach free space manager where each free sector points to the next
	for(int i=1; i<128; i++){
		buffer[0]= (i+1)%128;
		DRESULT result = eDisk_WriteBlock((const BYTE*)buffer, i);  // Free block points to next free block
		if(result!=RES_OK)                                          // Initially all blocks free
			return 1;
	}
	
	
  return 0;   // replace

}

//---------- eFile_Mount-----------------
// Mount the file system, without formating
// Input: none
// Output: 0 if successful and 1 on failure
int eFile_Mount(void){ // initialize file system

  return 0;   // replace
}


//---------- eFile_Create-----------------
// Create a new, empty file with one allocated block
// Input: file name is an ASCII string up to seven characters 
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Create( const char name[]){  // create new file, make it empty 

	int root_directory_buffer[128];
	for(int i=0; i<128; i++){    // Free space set to all 1s
		root_directory_buffer[i]=-1;
	}
		
		//buffer[127]=1;
	
	
	  // Read the root directory sector into root_directory_buffer
		DRESULT result = eDisk_ReadBlock((BYTE*)root_directory_buffer, 0);  //Directory 
	  if(result!=RES_OK){
			return 1;
		}
		
		//Last 4 bytes of the root_directory_buffer points to the first free sector
		int first_free_sector = root_directory_buffer[127];
		
		
		int first_free_sector_buffer[128];
		if(first_free_sector==0){
			return 1;                 // allocated disk space full. no more free sectors
		}
		
		// Read the first free sector into first_free_sector_buffer
		result = eDisk_ReadBlock((BYTE*)first_free_sector_buffer, first_free_sector);  //Directory 
	  if(result!=RES_OK){
			return 1;
		}
		
		// Find the next free sector from the currently allocated sector and set the last entry in root directory
		// (which points to first free sector) equal to this next free sector
		int next_freesector = first_free_sector_buffer[0];
		root_directory_buffer[127] = next_freesector;
		
		// First 4 bytes in root directory contains number of files in this directory
		int current_num_files = root_directory_buffer[0];
		
		// Store the file name in 8 byte char array
		char file_name[8];
		int ctr = 0;
		while(name[ctr]!=0){
			file_name[ctr]= name[ctr];
			ctr++;
		}
		file_name[ctr]=0;
		
		
		// Compute the position in root directory to make the entry for the new file
		//Entry contains 3 components
		// 1) File name 8 bytes
		// 2) First sector of the new file
		// 3) Number of bytes of the file (file size)
		int new_file_entry_pos = 4 + (DIRECTORY_ENTRY_SIZE)*current_num_files;
		char* root_directory_buffer_ptr = (char*)root_directory_buffer;
		// Write file name
		for(int i=0; i<MAX_FILE_NAME_SIZE; i++){
			root_directory_buffer_ptr[new_file_entry_pos+i] = file_name[i];
		}
		
		//After entering the file name, enter the first free sector of the new file, and the file size
		int* file_size_ptr = (int*)((char*)(root_directory_buffer_ptr)+new_file_entry_pos+MAX_FILE_NAME_SIZE);
		file_size_ptr[0] = first_free_sector;  // First sector of the new file
		file_size_ptr[1] = 0;   // New file; size = 0 bytes
		
		first_free_sector_buffer[0] = 0;       // Point to root directory since this is last sector of file
		first_free_sector_buffer[1] = 0;     //Number of used bytes in sector
		
		//Increment the file count in root directory
		root_directory_buffer[0] = current_num_files+1;
		//Write the modified root directory and first sector of new file to disk
		result = eDisk_WriteBlock((const BYTE*)root_directory_buffer, 0);
		if(result!=RES_OK)                                          
			return 1;		
		result = eDisk_WriteBlock((const BYTE*)first_free_sector_buffer, first_free_sector);
		if(result!=RES_OK)                                          
			return 1;	
		//root_directory_buffer[new_file_entry_pos+]
		
	//Return 0 if no failure in any of above steps
  return 0;   // replace

}


//---------- eFile_WOpen-----------------
// Open the file, read into RAM last block
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WOpen( const char name[]){      // open a file for writing 

	  // Read the root directory sector into root_directory_buffer
	  int root_directory_buffer[128];
		DRESULT result = eDisk_ReadBlock((BYTE*)root_directory_buffer, 0);  //Directory 
	  if(result!=RES_OK){
			return 1;
		}
		int num_files = root_directory_buffer[0];
		int file_size=0;
		int file_first_sector=0;
		for(int i=0; i<num_files; i++){
			int name_pos = 4+(i*DIRECTORY_ENTRY_SIZE);
			char* file_name_ptr = (char*)(root_directory_buffer)+name_pos;
			if(strCmp(name, file_name_ptr)==0){
				int* first_sector_ptr = (int*)((char*)(file_name_ptr+MAX_FILE_NAME_SIZE));
				int* file_size_ptr = (first_sector_ptr+1);
				file_first_sector = first_sector_ptr[0];
				file_size         = first_sector_ptr[1];
				loaded_files[0].current_file_size = file_size;  // If file found load the current file info struct entries
				strcpy(loaded_files[0].file_name, name);
				loaded_files[0].file_first_sector_id = file_first_sector;
				break;
			}
		}
		if(file_first_sector==0){
			return 1;                 // File name not found
		}

		int file_to_open_buffer[128];
		result = eDisk_ReadBlock((BYTE*)file_to_open_buffer, file_first_sector);  //Directory 
	  if(result!=RES_OK){
			return 1;
		}
		int last_sector = file_first_sector;
		while(file_to_open_buffer[0]!=0){
			last_sector = file_to_open_buffer[0];
		  result = eDisk_ReadBlock((BYTE*)file_to_open_buffer, last_sector);  //Directory 
	    if(result!=RES_OK){
			   return 1;
		  }
		}
		int* int_ptr_open_file_block = (int*)(loaded_files[0].last_block);
		for(int i=0; i<128; i++){
		int_ptr_open_file_block[i] = file_to_open_buffer[i];
		}
		loaded_files[0].file_status = FILE_WOPEN;
		loaded_files[0].file_last_sector_id = last_sector;
		
		
//		result = eDisk_WriteBlock((const BYTE*)root_directory_buffer, 0);
//		if(result!=RES_OK)                                          
//			return 1;		
		

  return 0;   // replace  

}

//---------- eFile_Write-----------------
// save at end of the open file
// Input: data to be saved
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Write( const char data){
	if(loaded_files[0].file_status==FILE_CLOSED){
		return 1;
	}
	// Append to the current write pointer int he last sector. If sector ends, call the private free sector allocator
	// helper to allocate a new sector.
	int* open_file_lastblock = (int*)loaded_files[0].last_block;
	int sector_size = open_file_lastblock[1];
	if(sector_size==MAX_DATA_BYTES_PER_SECTOR){
		  DRESULT result = eDisk_WriteBlock((BYTE*)loaded_files[0].last_block, loaded_files[0].file_last_sector_id);  //Directory 
	    if(result!=RES_OK){
			   return 1;
		  }
			
		DRESULT create_new_block = allocate_free_filesector(loaded_files[0].file_last_sector_id);
		if(create_new_block!=RES_OK){
			return 1;
		}
		sector_size = open_file_lastblock[1];
		if(sector_size!= 0){
			return 1;
		}		
		sector_size = 0;	
		  result = eDisk_WriteBlock((BYTE*)loaded_files[0].last_block, loaded_files[0].file_last_sector_id);  //Directory 
	    if(result!=RES_OK){
			   return 1;
		  }
	}
	
	char* open_file_lastblock_byte_ptr = (char*)open_file_lastblock;
	open_file_lastblock_byte_ptr[8+sector_size]=data;
	open_file_lastblock[1] += 1;
	loaded_files[0].current_file_size += 1;
	
	
  
    return 0;   // replace

}

//---------- eFile_WClose-----------------
// close the file, left disk in a state power can be removed
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WClose(void){ // close the file for writing
	if(loaded_files[0].file_status!=FILE_WOPEN){
		return 1;
	}
	
	// Write to disk the open sector in RAM, and updat ethe file size in the root sector
	int root_directory_buffer[128];
	DRESULT result = eDisk_ReadBlock((BYTE*)root_directory_buffer, 0);  //Directory 
	if(result!=RES_OK){
		 return 1;
	}
	
	int current_num_files = root_directory_buffer[0];
	
	result = eDisk_WriteBlock((BYTE*)loaded_files[0].last_block, loaded_files[0].file_last_sector_id);  //Directory 
	if(result!=RES_OK){
		 return 1;
	}
	
		int file_found_flag=0;
	  for(int i=0; i<current_num_files; i++){
		char* root_directory_buffer_ptr = (char*)root_directory_buffer;
		char* file_entry_ptr = (char*)((char*)(root_directory_buffer_ptr)+(4+i*DIRECTORY_ENTRY_SIZE));
		//file_size_ptr[0] = first_free_sector;  // First sector of the new file
		//int first_sector_id = file_size_ptr[0];
		if(strCmp(file_entry_ptr, loaded_files[0].file_name)==0){
		int* file_entry = (int*)((char*)(file_entry_ptr)+MAX_FILE_NAME_SIZE);
		file_entry[1] = loaded_files[0].current_file_size; 
		file_found_flag=1;
		break;
		}


			// New file; size = 0 bytes		int new_file_entry_pos = 4 + (DIRECTORY_ENTRY_SIZE)*current_num_files;
		//char* root_directory_buffer_ptr = (char*)root_directory_buffer;
		}
		if(file_found_flag==0){
			return 1;            // Something seriously wrong if this occurs; 
			                     // it means the open file isn't found in directory
		}
	result = eDisk_WriteBlock((BYTE*)root_directory_buffer, 0);  //Directory 
	if(result!=RES_OK){
		 return 1;
	}		
		
  return 0;   // replace

}


//---------- eFile_ROpen-----------------
// Open the file, read first block into RAM 
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble read to flash)
int eFile_ROpen( const char name[]){      // open a file for reading 
	  int root_directory_buffer[128];
		DRESULT result = eDisk_ReadBlock((BYTE*)root_directory_buffer, 0);  //Directory 
	  if(result!=RES_OK){
			return 1;
		}
		int num_files = root_directory_buffer[0];
		int file_size=0;
		int file_first_sector=0;
		for(int i=0; i<num_files; i++){
			int name_pos = 4+(i*DIRECTORY_ENTRY_SIZE);
			char* file_name_ptr = (char*)(root_directory_buffer)+name_pos;
			if(strCmp(name, file_name_ptr)==0){
				int* first_sector_ptr = (int*)((char*)(file_name_ptr+MAX_FILE_NAME_SIZE));
				int* file_size_ptr = (first_sector_ptr+1);
				file_first_sector = first_sector_ptr[0];
				file_size         = first_sector_ptr[1];
				loaded_files[0].current_file_size = file_size;
				strcpy(loaded_files[0].file_name, name);
				loaded_files[0].file_first_sector_id = file_first_sector;
				break;
			}
		}
		if(file_first_sector==0){
			return 1;                 // File name not found
		}

		int file_to_open_buffer[128];
		result = eDisk_ReadBlock((BYTE*)file_to_open_buffer, file_first_sector);  //Directory 
	  if(result!=RES_OK){
			return 1;
		}
		loaded_files[0].file_status=FILE_ROPEN;
		loaded_files[0].read_ptr = 0;
		loaded_files[0].total_bytes_read =0;
//		for(int i=0; i<128; i++){
//		loaded_files[0].last_block[i] = file_to_open_buffer[i];
//		}
		int* int_ptr_open_file_block = (int*)(loaded_files[0].last_block);
		for(int i=0; i<128; i++){
		int_ptr_open_file_block[i] = file_to_open_buffer[i];
		}
		//loaded_files[0].file_status = FILE_WOPEN;
		//loaded_files[0].file_first_sector_id = ;

  return 0;   // replace   
 
}
 
//---------- eFile_ReadNext-----------------
// retreive data from open file
// Input: none
// Output: return by reference data
//         0 if successful and 1 on failure (e.g., end of file)
int eFile_ReadNext( char *pt){       // get next byte 
	if(loaded_files[0].file_status!=FILE_ROPEN){
		return 1;
	}

	int* open_file_lastblock = (int*)loaded_files[0].last_block;
	int sector_size = open_file_lastblock[1];
	if(loaded_files[0].read_ptr==sector_size){
		return 1;                               // End of file reached
	}
	if(sector_size==0){
		return 1;          // This should never happen;
											 // It means that the current loaded sector is empty
											// Something wrong with implementation if this occurs
	}
	
	char* open_file_lastblock_byte_ptr = (char*)open_file_lastblock;
	(*pt) = open_file_lastblock_byte_ptr[8+loaded_files[0].read_ptr];
	loaded_files[0].read_ptr++;
	loaded_files[0].total_bytes_read += 1;
	
	if(loaded_files[0].read_ptr==MAX_DATA_BYTES_PER_SECTOR && loaded_files[0].total_bytes_read<loaded_files[0].current_file_size){
		loaded_files[0].read_ptr =0;
		int next_file_sector = open_file_lastblock[0];
		if(next_file_sector == 0){// && loaded_files[0].read_ptr!=sector_size){
			return 1;                 // Again, something wrong with implementation if this happens
			
		}
		DRESULT	result = eDisk_ReadBlock((BYTE*)loaded_files[0].last_block, next_file_sector);  //Directory 
	  if(result!=RES_OK){
			return 1;
		}
	}

	//open_file_lastblock[1] += 1;
	//loaded_files[0].current_file_size += 1;
	
	
  
    return 0;   // replace  

}
    
//---------- eFile_RClose-----------------
// close the reading file
// Input: none
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_RClose(void){ // close the file for writing
	if(loaded_files[0].file_status!=FILE_ROPEN){
		return 1;
	}
	loaded_files[0].file_status=FILE_CLOSED;
  
  return 0;   // replace

}


//---------- eFile_Delete-----------------
// delete this file
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Delete( const char name[]){  // remove this file 
	  int root_directory_buffer[128];
		DRESULT	result = eDisk_ReadBlock((BYTE*)root_directory_buffer, 0);  //Directory 
	  if(result!=RES_OK){
			return 1;
		}
		int first_sector_of_file_to_be_deleted=0;
		int file_size_of_file_to_be_deleted = 0;
		int num_files = root_directory_buffer[0];
		
		// Reaarange the entries so that they are contiguous once a file entry has been deleted in directory
		for(int i=0; i<num_files; i++){
	    int current_file_offset = 4+(i*DIRECTORY_ENTRY_SIZE);
			if(strCmp((char*)((char*)(root_directory_buffer)+current_file_offset), name)==0){
				int* sector_ptr = (int*)((char*)(root_directory_buffer)+current_file_offset+MAX_FILE_NAME_SIZE);
				first_sector_of_file_to_be_deleted = sector_ptr[0];
				file_size_of_file_to_be_deleted = sector_ptr[1];
				int entries_below = num_files - i -1;
				char* rearrange_ptr = (char*)((char*)(root_directory_buffer)+current_file_offset);
				for(int j=0; j<entries_below; j++){
					for(int k=0; k<DIRECTORY_ENTRY_SIZE; k++){
					rearrange_ptr[(j*DIRECTORY_ENTRY_SIZE)+k] = rearrange_ptr[(j*DIRECTORY_ENTRY_SIZE)+DIRECTORY_ENTRY_SIZE+k];
					}
				}
				
			}
		}
		if(first_sector_of_file_to_be_deleted==0){
			return 1;
		}
		// Release the space held by the file and update the free space pointers.
		int current_first_free_sector = root_directory_buffer[127];
		int file_to_delete_buffer[128];
		  result = eDisk_ReadBlock((BYTE*)file_to_delete_buffer, first_sector_of_file_to_be_deleted);  //Directory 
	    if(result!=RES_OK){
			   return 1;
		  }
		int last_sector_of_file_to_be_deleted = first_sector_of_file_to_be_deleted;
		while(file_to_delete_buffer[0]!=0){
			last_sector_of_file_to_be_deleted = file_to_delete_buffer[0];
		  result = eDisk_ReadBlock((BYTE*)file_to_delete_buffer, last_sector_of_file_to_be_deleted);  //Directory 
	    if(result!=RES_OK){
			   return 1;
		  }
		}
		file_to_delete_buffer[0] = current_first_free_sector;
		root_directory_buffer[127] = first_sector_of_file_to_be_deleted;
		root_directory_buffer[0] -= 1;

		result = eDisk_WriteBlock((BYTE*)file_to_delete_buffer, last_sector_of_file_to_be_deleted);  //Directory 
	  if(result!=RES_OK){
			return 1;
		}			
		result = eDisk_WriteBlock((BYTE*)root_directory_buffer, 0);  //Directory 
	  if(result!=RES_OK){
			return 1;
		}	

  return 0;   // replace

}                             


//---------- eFile_DOpen-----------------
// Open a (sub)directory, read into RAM
// Input: directory name is an ASCII string up to seven characters
//        (empty/NULL for root directory)
// Output: 0 if successful and 1 on failure (e.g., trouble reading from flash)
int eFile_DOpen( const char name[]){ // open directory
	strcpy(current_directory_info.directory_name, name);
	current_directory_info.entry_ptr = 0;
	
	
	  int root_directory_buffer[128];
		DRESULT result = eDisk_ReadBlock((BYTE*)root_directory_buffer, 0);  //Directory 
	  if(result!=RES_OK){
			return 1;
		}	
		
		current_directory_info.num_entries = root_directory_buffer[0];
		for(int i=0; i<current_directory_info.num_entries; i++){
			strcpy(current_directory_info.files[i].file_name, (char*)((char*)root_directory_buffer+4+(DIRECTORY_ENTRY_SIZE*i)));
			current_directory_info.files[i].num_bytes = *((uint32_t*)((char*)((char*)root_directory_buffer+4+(DIRECTORY_ENTRY_SIZE*i)+12)));
		}
   
  return 0;   // replace
}
  
//---------- eFile_DirNext-----------------
// Retreive directory entry from open directory
// Input: none
// Output: return file name and size by reference
//         0 if successful and 1 on failure (e.g., end of directory)
int eFile_DirNext( char *name [], unsigned long *size){  // get next entry 
	if(current_directory_info.entry_ptr>=current_directory_info.num_entries)
		return 1;
	strcpy(*name, current_directory_info.files[current_directory_info.entry_ptr].file_name);
	*size = current_directory_info.files[current_directory_info.entry_ptr].num_bytes;
	current_directory_info.entry_ptr++;
   
  return 0;   // replace
}

//---------- eFile_DClose-----------------
// Close the directory
// Input: none
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_DClose(void){ // close the directory
   
  return 0;   // replace
}


//---------- eFile_Close-----------------
// Deactivate the file system
// Input: none
// Output: 0 if successful and 1 on failure (not currently open)
int eFile_Close(void){ 
   
  return 0;   // replace
}
