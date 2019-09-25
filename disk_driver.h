#pragma once
#include "bitmap.h"

#define BLOCK_SIZE 512

//first block of the disk
typedef struct{
	int num_blocks; 
	int bitmap_blocks; //num of blocks in the bitmap
	int bitmap_entries; //bytes needed to store the bitmap
	
	int free_blocks; //free blocks
	int first_free_block; //first block index
} DiskHeader;

typedef struct{
	DiskHeader* header; //mmapped
	char* bitmap_data; //mmapped (bitmap)
	int fd; //file descriptor
} DiskDriver;

//This function opens or creates the file, 
//allocates its needed memory, calculates the size of the bitmap,
//if the file is new
//it compiles a disk header and set to 0 the entire bitmap (free space)
void DiskDriver_init(DiskDriver* disk, const char* filename, int num_blocks);

//reads the block in pos "block_num", returns -1 if the block is free
//0 otherwise
int DiskDriver_readBlock(DiskDriver* disk, void* dest, int block_num);

//writes a block in pos "block_num" and edits the bitmap, returns -1 
//if operation is not possible
int DiskDriver_writeBlock(DiskDriver* disk, void* src, int block_num);

//update a block in pos "block_num", DOES NOT edit the bitmap
//returns -1 if operation is not possible
int DiskDriver_updateBlock(DiskDriver* disk, void* src, int block_num);

//frees a block in pos "block_num", edits the bitmap
//returns -1 if operation is not possible
int DiskDriver_freeBlock(DiskDriver* disk, int block_num);

//returns the first free block in disk, starting from postion "start"
int DiskDriver_getFreeBlock(DiskDriver* disk, int start);

//writes data (flush)
int DiskDriver_flush(DiskDriver* disk);
