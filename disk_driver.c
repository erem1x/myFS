#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <assert.h>
#include <unistd.h>
#include <sys/mman.h>
#include "disk_driver.h"
#include "error.h"

//Using a standard error handler
void DiskDriver_init(DiskDriver* disk, const char* filename, int num_blocks){
	if(disk==NULL || filename==NULL || num_blocks<1){
		//checking parameters
		ERROR_HELPER(-1, "Impossible to init disk: Bad Parameters\n");
	}
	
	int bitmap_size=num_blocks / BIT_NUM; 
	if(num_blocks % 8) bitmap_size+=1;
	
	int fd;
	int is_file=access(filename, F_OK) == 0; //verify if the file exists
	
	if(is_file){
		fd=open(filename, O_RDWR, (mode_t)0666); //opens file
		ERROR_HELPER(fd, "Impossible to init disk: Error opening file\n");
		
		//mmapping Disk Header (map shared) into disk
		DiskHeader* disk_mem = (DiskHeader*) mmap(0, sizeof(DiskHeader)+bitmap_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0); 
		if(disk_mem==MAP_FAILED){
			close(fd);
			ERROR_HELPER(-1, "Impossible to init disk: file mmap failed\n");
		}
		//saving pointers to mmap space
		disk->header=disk_mem;
		disk->bitmap_data=(char*)disk_mem + sizeof(DiskHeader); //header offset
	}
	else{
		//file doesn't exist, creating it
		fd=open(filename, O_RDWR | O_CREAT | O_TRUNC, (mode_t)0666);
		ERROR_HELPER(fd, "Impossible to init disk: Error creating file\n");
		//fallocate allocs the necessary space to mmap, referring to fd
		if(posix_fallocate(fd, 0, sizeof(DiskHeader)+bitmap_size)>0){
			//File has size=0
			ERROR_HELPER(-1, "Impossible to init disk: Error in fallocate\n");
		}
		//mmap header
		DiskHeader* disk_mem = (DiskHeader*) mmap(0, sizeof(DiskHeader)+bitmap_size
			, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);  		
		if (disk_mem == MAP_FAILED){
			close(fd);
			ERROR_HELPER(-1, "Impossible to init disk: Error mmapping the file\n");
		}
		
		//saving pointers to mmap space
		disk->header=disk_mem;
		disk->bitmap_data=(char*)disk_mem+sizeof(DiskHeader); //pointer to bitmap calculated by header offset
		disk_mem->num_blocks=num_blocks;
		//completing the header
		disk_mem->bitmap_blocks=num_blocks;
		disk_mem->bitmap_entries=bitmap_size;
		disk_mem->free_blocks=num_blocks; //all free
		disk_mem->first_free_block=0;
		memset(disk->bitmap_data, 0, bitmap_size); //setting 0's
	}
	
	disk->fd=fd;
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
	//setting pointer of fd on the block to read
	off_t offset=lseek(fd, sizeof(DiskHeader)+disk->header->bitmap_entries+(block_num*BLOCK_SIZE), SEEK_SET);
	ERROR_HELPER(offset, "Impossible to read block: lseek error\n");
	
	int ret, bytes_read=0;
	//loop to check errors
	while(bytes_read<BLOCK_SIZE){
		ret=read(fd, dest+bytes_read, BLOCK_SIZE-bytes_read);
		if(ret==-1 && errno==EINTR) continue;
		ERROR_HELPER(ret, "Impossible to read block: error in loop\n");
		
		bytes_read+=ret;
	}
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
	BitMap_set(&bmp, block_num, 1);
	disk->header->free_blocks-=1;
		
	int fd=disk->fd;
	off_t offset=lseek(fd, sizeof(DiskHeader)+disk->header->bitmap_entries+(block_num*BLOCK_SIZE), SEEK_SET); //set pointer of fd on th block to write (just like read)
	ERROR_HELPER(offset, "Impossible to write block: lseek error\n");
		
	int ret, bytes_written=0;
	//loop to check errors
	while(bytes_written<BLOCK_SIZE){
		ret=write(fd, src + bytes_written, BLOCK_SIZE - bytes_written);
		if(ret==-1 && errno==EINTR) continue;
		ERROR_HELPER(ret, "Impossible to write block: error in loop\n");
		bytes_written+=ret;
	}
	return 0;
}

int DiskDriver_updateBlock(DiskDriver* disk, void* src, int block_num){
	if(block_num>disk->header->bitmap_blocks || block_num<0 || src==NULL || disk==NULL){
		ERROR_HELPER(-1, "Impossible to update block: Bad Parameters\n");
	}
	
	int fd=disk->fd;
	off_t offset=lseek(fd, sizeof(DiskHeader)+disk->header->bitmap_entries+(block_num*BLOCK_SIZE), SEEK_SET); //pointer of fd on block to update
	ERROR_HELPER(offset, "Impossible to update block: lseek error\n");
	
	int ret, bytes_written=0;
	//loop to check errors
	while(bytes_written<BLOCK_SIZE){
		ret=write(fd, src+bytes_written, BLOCK_SIZE - bytes_written);
		if(ret==-1 && errno==EINTR) continue;
		
		ERROR_HELPER(ret, "Impossible to write block: error in loop\n");
		
		bytes_written+=ret;
	}
	return 0;
}
		

int DiskDriver_freeBlock(DiskDriver* disk, int block_num){
	if(block_num>disk->header->bitmap_blocks || block_num < 0 || disk==NULL){
		ERROR_HELPER(-1, "Impossible to free block: Bad Parameters\n");
	}
	
	BitMap bmp;
	//same as read/write
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
	int free_block=BitMap_get(&bmp, start, 0);
	
	return free_block;
}


int DiskDriver_flush(DiskDriver* disk){
	int bitmap_size=disk->header->num_blocks / 8 +1;
	//flushing header and bitmap on file
	//msync flushes changes made on a mmapped file
	int ret=msync(disk->header, (size_t)sizeof(DiskHeader)+bitmap_size, MS_SYNC);
	ERROR_HELPER(ret, "Couldn't sync the file on disk\n");
	return 0;
}
