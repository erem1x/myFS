#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include "disk_driver.h"
#include "error.h" //standard error handler

//Using mmap to map our file into memory, allowing multi-process access
//and using the same physical memory
void DiskDriver_init(DiskDriver* disk, const char* filename, int num_blocks){
	if(disk==NULL || filename==NULL || num_blocks<1){
		//checking parameters
		ERROR_HELPER(-1, "Impossible to init disk: Bad Parameters\n");
	}
	
	int bitmap_size=num_blocks / BIT_NUM; //bmp entries
	if(num_blocks % 8) ++bitmap_size; //round up
	
	int fd;
	int exists=access(filename, F_OK) == 0; //verify if file exists
	
	if(exists){
		fd=open(filename, O_RDWR, (mode_t)0666); //opens file
		ERROR_HELPER(fd, "Impossible to init disk: Error opening file\n");
	}
	else{
		//file doesn't exist, creating it
		fd=open(filename, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0666);
		ERROR_HELPER(fd, "Impossible to init disk: Error creating file\n");
		
		//allocating new disk file, setting his size
		if(posix_fallocate(fd, 0, sizeof(DiskHeader)+bitmap_size)>0){
			//File has size=0
			ERROR_HELPER(-1, "Impossible to init disk: Error in fallocate\n");
		}
	}
	//mmapping Disk Header (map shared) into disk
	DiskHeader* disk_mem = (DiskHeader*) mmap(0, sizeof(DiskHeader)+bitmap_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0); 
	if(disk_mem==MAP_FAILED){
		close(fd);
		ERROR_HELPER(-1, "Impossible to init disk: file mmap failed\n");
	}
	//saving pointers to mmap space
	disk->header=disk_mem;
	disk->bitmap_data=(char*)disk_mem + sizeof(DiskHeader); //header offset
	disk->fd=fd;
	
	if(!exists){
		//completing the header
		disk_mem->num_blocks=num_blocks;
		disk_mem->bitmap_blocks=num_blocks;
		disk_mem->bitmap_entries=bitmap_size;
		
		disk_mem->free_blocks=num_blocks; //all free
		disk_mem->first_free_block=0;
		memset(disk->bitmap_data, 0, bitmap_size); //setting 0's
	}
	
}

int DiskDriver_readBlock(DiskDriver* disk, void* dest, int block_num){
	//checking parameters
	if(block_num>disk->header->bitmap_blocks || block_num<0 || dest==NULL || disk==NULL){
		ERROR_HELPER(-1, "Impossile to read block: Bad Parameters\n"); 
	}
	//allocating bitmap structure, copying num blocks and data
	BitMap bmp;
	
	bmp.num_bit=disk->header->bitmap_blocks;
	bmp.entries=disk->bitmap_data;
	
	if(!BitMap_getBit(&bmp, block_num)) return -1; //checks if block is free
	int fd=disk->fd;
	//using pread (read with an offset)
	int ret=pread(fd, dest, BLOCK_SIZE, sizeof(DiskHeader)+disk->header->bitmap_entries+(block_num*BLOCK_SIZE));
	ERROR_HELPER(ret, "Impossible to read, read error\n");
	return 0;
}


int DiskDriver_writeBlock(DiskDriver* disk, void* src, int block_num){
	if(block_num>disk->header->bitmap_blocks || block_num<0 || src==NULL || disk==NULL){
		ERROR_HELPER(-1, "Impossible to write block: Bad Parameters\n");
	}
	BitMap bmp;
	
	bmp.num_bit=disk->header->bitmap_blocks;
	bmp.entries=disk->bitmap_data;
	if(BitMap_getBit(&bmp, block_num)) return -1; //block is full
	
	//updating first free block
	if(block_num==disk->header->first_free_block){
		disk->header->first_free_block=DiskDriver_getFreeBlock(disk, block_num+1);}
	//block full on bitmap
	if(BitMap_set(&bmp, block_num, 1)==-1) return -1;
	disk->header->free_blocks-=1;
		
	int fd=disk->fd;
	
	//using pwrite, offset write
	int ret=pwrite(fd, src, BLOCK_SIZE, sizeof(DiskHeader)+disk->header->bitmap_entries+(block_num*BLOCK_SIZE));
	ERROR_HELPER(ret, "Impossible to write data on block\n");
	return 0;
}

int DiskDriver_updateBlock(DiskDriver* disk, void* src, int block_num){
	if(block_num>disk->header->bitmap_blocks || block_num<0 || src==NULL || disk==NULL){
		ERROR_HELPER(-1, "Impossible to update block: Bad Parameters");
	}
	//skip updating bitmap
	int fd=disk->fd;
		
	int ret=pwrite(fd, src, BLOCK_SIZE, sizeof(DiskHeader)+disk->header->bitmap_entries+(block_num*BLOCK_SIZE));
	ERROR_HELPER(ret, "Impossible to update data on block\n");
	return 0;
}
		

int DiskDriver_freeBlock(DiskDriver* disk, int block_num){
	if(block_num>disk->header->bitmap_blocks || block_num < 0 || disk==NULL){
		ERROR_HELPER(-1, "Impossible to free block: Bad Parameters\n");
	}
	
	BitMap bmp;
	//same as read/write, updating bitmap
	bmp.num_bit=disk->header->bitmap_blocks;
	bmp.entries=disk->bitmap_data;
	
	if(!BitMap_getBit(&bmp, block_num)) return -1; //block already free
	
	//set bit to 0 on bitmap
	if(BitMap_set(&bmp, block_num, 0) < 0){
		ERROR_HELPER(-1, "Impossible to free block: Bitmap bit setting error\n");
	}
	
	//updating first free block
	if(block_num < disk->header->first_free_block || disk->header->first_free_block==-1) disk->header->first_free_block=block_num;
	
	//setting changes on disk structure
	disk->header->free_blocks+=1;
	
	return 0;
}

int DiskDriver_getFreeBlock(DiskDriver* disk, int start){
	if(start>disk->header->bitmap_blocks){
		ERROR_HELPER(-1, "Impossible to get a free block: Bad Parameters\n");
	}
	
	BitMap bmp;
	
	//same here
	bmp.num_bit=disk->header->bitmap_blocks;
	bmp.entries=disk->bitmap_data;
	
	//using BitMap_get to find the first free block
	return BitMap_get(&bmp, start, 0);
	
}


int DiskDriver_flush(DiskDriver* disk){
	int bitmap_size=disk->header->num_blocks / 8 +1;
	//flushing header and bitmap on file
	//msync flushes changes made on a mmapped file
	int ret=msync(disk->header, (size_t)sizeof(DiskHeader)+bitmap_size, MS_SYNC);
	ERROR_HELPER(ret, "Couldn't sync the file on disk\n");
	return 0;
}
