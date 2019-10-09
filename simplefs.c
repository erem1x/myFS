#include <stdlib.h>
#include <stdio.h>
#include "simplefs.h"
#include "error.h"
#include "shell.h"

//directories need to have all 0s at first
DirectoryHandle* SimpleFS_init(SimpleFS* fs, DiskDriver* disk){
	if(fs==NULL || disk==NULL){
		ERROR_HELPER(-1, "Impossible to init: Bad Parameters\n");
	}
	
	fs->disk=disk;
	
	FirstDirectoryBlock* fdb=malloc(sizeof(FirstDirectoryBlock));
	
	//reading block 0 of the given disk for the root directory
	int ret=DiskDriver_readBlock(disk, fdb, 0);
	//if there's no root dir
	if(ret==-1){
		free(fdb);
		return NULL;
	}
	
	//handle to return
	DirectoryHandle* dh=(DirectoryHandle*) malloc(sizeof(DirectoryHandle));
	dh->sfs=fs;
	dh->dcb=fdb;
	dh->directory=fdb;
	dh->pos_in_block=0;
	
	//handler to root
	return dh;
}


void SimpleFS_format(SimpleFS* fs){
	if(fs==NULL){
		ERROR_HELPER(-1, "Impossible to format: Bad Parameters\n");
	}
	
	int bitmap_size=fs->disk->header->bitmap_entries;
	memset(fs->disk->bitmap_data, 0, bitmap_size); //setting 0s
	fs->disk->header->free_blocks=fs->disk->header->num_blocks; //clearing bitmap
	fs->disk->header->first_free_block=0; //starts at 0
	
	int ret=0;
	
	//creating block for root directory, set to 0, cleaning old data
	FirstDirectoryBlock root={0};
	root.header.block_in_file=0;
	root.header.previous_block=root.header.next_block=-1;
	
	root.fcb.directory_block=-1; //no parents
	root.fcb.block_in_disk=0;
	root.fcb.is_dir=1;
	root.fcb.size_in_blocks=1;
	strcpy(root.fcb.name, "/");
	
	
	DiskDriver_writeBlock(fs->disk, &root, 0); //writing root on block 0, DiskHeader offset already calculated
}

//checks if a file already exists, uses filename
static int checkFileExistence(DiskDriver* disk, int entries, int* file_blocks, const char* filename){
	FirstFileBlock check;
	int i;
	for(i=0; i<entries; i++){
		if(file_blocks[i]>0 && (DiskDriver_readBlock(disk, &check, file_blocks[i])) != -1){
			if(!strncmp(check.fcb.name, filename, MAX_NAME_LEN)){
				return i;
			}
		}
	}
	return -1;
}

FileHandle* SimpleFS_createFile(DirectoryHandle* d, const char* filename){
	if(d==NULL || filename==NULL) ERROR_HELPER(-1, "Cannot create file: Bad Parameters\n");
	
	int ret=0;
	FirstDirectoryBlock* fdb=d->dcb;
	DiskDriver* disk=d->sfs->disk;
	
	if(fdb->num_entries>0){ //directory not empty
		//checks if file already exists, uses FirstDirectoryBlock
		if(checkFileExistence(disk, FDB_space, fdb->file_blocks, filename)!=-1){
			printf("%sCannot create file %s%s%s: file already exists\n%s", RED, RESET, filename, RED, RESET);
			return NULL;
		}
		
		int next=fdb->header.next_block;
		DirectoryBlock db;
		
		//checks the integrity of the written blocks
		while(next!=-1){
			ret=DiskDriver_readBlock(disk, &db, next);
			if(ret==-1){
				printf("Cannot create file: problems on readBlock\n");
				return NULL;
			}
			
			//checks if file is in the DirectoryBlock
			if(checkFileExistence(disk, DB_space, db.file_blocks, filename)!=-1){
				printf("%sCannot create file %s%s%s: file already exists\n%s", RED, RESET, filename, RED, RESET);
				return NULL;
			}
			next=db.header.next_block; //next DirectoryBlock
		}
	}
	
	//getting a free block
	int new_block=DiskDriver_getFreeBlock(disk, disk->header->first_free_block);
	if(new_block==-1){
		printf("Cannot create file: can't find any free blocks\n");
		return NULL;
	}
	
	//using calloc to set 0's in the file
	FirstFileBlock* new_file=calloc(1, sizeof(FirstFileBlock));
	new_file->header.block_in_file=0;
	new_file->header.previous_block=new_file->header.next_block=-1;
	
	new_file->fcb.directory_block=fdb->fcb.block_in_disk;
	new_file->fcb.block_in_disk=new_block;
	new_file->fcb.is_dir=0;
	new_file->fcb.written_bytes=0;
	new_file->fcb.size_in_blocks=1;
	strncpy(new_file->fcb.name, filename, MAX_NAME_LEN);
	
	//writing file on disk
	ret=DiskDriver_writeBlock(disk, new_file, new_block);
	if(ret==-1){
		printf("Cannot create file: problems on writeBlock\n");
		return NULL;
	}
	
	//saving now changes on directory
	int i = 0;
	int found = 0;	//checking free space in existing directory blocks																				
	int block_number=fdb->fcb.block_in_disk; // # of blocks in disk
	DirectoryBlock db_last; //in case there's no space in FirstDirectoryBlock
	
	int entry = 0;	//number of the entry in file_blocks of (first)DirectoryBlock
	int blockInFile = 0; //number of the block in the directory	
	int first_or_not = 0; //0 for fdb, 1 for db_last
    int where_to_save = 0; //0 for fdb, 1 for another block	

	//checking for free space in fdb
	if (fdb->num_entries < FDB_space){																
		int* blocks = fdb->file_blocks;
		//looping to find a free block
		for(i=0; i<FDB_space; i++){																	
			if (blocks[i] == 0){																	
				found = 1;
				entry = i;
				break;
			}
		}
	} else{																							
		where_to_save = 1; //no space in fdb
		int next = fdb->header.next_block;
		
		//finding position in another DirectoryBlock
		while (next != -1 && !found){																
			ret = DiskDriver_readBlock(disk, &db_last, next);										
			if (ret == -1){
				printf("Cannot create file: problem on readBlock to read directory and change status\n");
				//gotta free the written block, can't complete operations
				DiskDriver_freeBlock(disk, fdb->fcb.block_in_disk);									
				return NULL;
			}
			int* blocks = db_last.file_blocks;
			blockInFile++; //updating																			
			block_number = next;
			//same loop to find the position
			for(i=0; i<DB_space; i++){																
				if (blocks[i] == 0){
					found = 1;
					entry = i;
					break;
				}

			}
			first_or_not = 1;
			next = db_last.header.next_block;
		}
	}

	if (!found){																					
		DirectoryBlock new_db = {0};																
		new_db.header.next_block = -1;
		new_db.header.block_in_file = blockInFile;
		new_db.header.previous_block = block_number;											
		new_db.file_blocks[0] = new_block; //saving the fcb of the just created file														
		
		//searching for a free block for the db in the disk
		int new_dir_block = DiskDriver_getFreeBlock(disk, disk->header->first_free_block);			
		if (new_block == -1){
			printf("Cannot create file: impossible to find free block to create a new block for directory\n");
			//freeing the already written block, can't complete
			DiskDriver_freeBlock(disk, fdb->fcb.block_in_disk);										
			return NULL;
		}

		//writing on disk
		ret = DiskDriver_writeBlock(disk, &new_db, new_dir_block);									
		if (ret == -1){
				printf("Cannot create file: problem on writeBlock to write file on disk\n");
				//same as before, freeing block
				DiskDriver_freeBlock(disk, fdb->fcb.block_in_disk);
				return NULL;
		}
	
		if (first_or_not == 0){
			fdb->header.next_block = new_dir_block; //updating header of current block
		} else{
			db_last.header.next_block = new_dir_block;
		}
		db_last = new_db;
		block_number = new_dir_block;

	} 

	if (where_to_save == 0){ //space in fdb
		fdb->num_entries++;	
		fdb->file_blocks[entry] = new_block; //saving pos in fdb					
		DiskDriver_updateBlock(disk, fdb, fdb->fcb.block_in_disk);
	} else{
		fdb->num_entries++;	
		DiskDriver_updateBlock(disk, fdb, fdb->fcb.block_in_disk);
		db_last.file_blocks[entry] = new_block;
		DiskDriver_updateBlock(disk, &db_last, block_number);
	}
	
	FileHandle* fh = malloc(sizeof(FileHandle));
	fh->sfs = d->sfs;
	fh->fcb = new_file;
	fh->directory = fdb;
	fh->pos_in_file = 0;
	
	return fh;
}
			
			
int SimpleFS_readDir(char** names, DirectoryHandle* d){
	if (d == NULL || names == NULL) ERROR_HELPER(-1, "Cannot read directory: Bad Parameters\n");

	int ret = 0, num_tot = 0;
	FirstDirectoryBlock *fdb = d->dcb;
	DiskDriver* disk = d->sfs->disk;

	if (fdb->num_entries > 0){ //directory not empty
		int i;
		FirstFileBlock to_check; 
		
		int* blocks=fdb->file_blocks;
		for(i=0;i<FDB_space;i++){ //checks every block entry in fdb
			//checking the name
			if(blocks[i]>0 && DiskDriver_readBlock(disk, &to_check, blocks[i])!=-1){
				//cut the string name if too long
				names[num_tot]=strndup(to_check.fcb.name, MAX_NAME_LEN);
				num_tot++;
			}
		}
		
		DirectoryBlock db;
		
		//checks if there are more entries in other blocks of the directory
		if(i<fdb->num_entries){ 
			int idx=fdb->header.next_block;
			//Checking other files/directories in other blocks of the dir
			while(idx!=-1){
				DiskDriver_readBlock(disk, &db, idx);
				int* blocks=db.file_blocks;
				//checks every block indicator in fdb directory
				for(i=0;i<DB_space;i++){
					//reades the FirstFileBlock of the file to check its name
					if(blocks[i]>0 && DiskDriver_readBlock(disk, &to_check, blocks[i])!=-1){ 
						names[num_tot]=strndup(to_check.fcb.name, MAX_NAME_LEN); //MAX_NAME_LEN=most size
						num_tot++;
					}
				}
				
				idx=db.header.next_block; //next directoryBlock
			}
		}
	}
	return 0;
}

FileHandle* SimpleFS_openFile(DirectoryHandle* d, const char* filename){
	if(d==NULL || filename==NULL) ERROR_HELPER(-1, "Cannot open file: Bad Parameters\n");
	
	int ret=0;
	FirstDirectoryBlock* fdb=d->dcb;
	DiskDriver* disk=d->sfs->disk;
	
	if(fdb->num_entries>0){ //dir not empty
		FileHandle* fh=malloc(sizeof(FileHandle)); //handle to return
		fh->sfs=d->sfs;
		fh->directory=fdb;
		fh->pos_in_file=0;
		
		int found=0; //file exists
		FirstFileBlock* to_check=malloc(sizeof(FirstFileBlock));
		
		//checking if file is in fdb
		int pos=checkFileExistence(disk, FDB_space, fdb->file_blocks, filename);
		if(pos>=0){
			found=1;
			//reading again, completing the handler
			DiskDriver_readBlock(disk, to_check, fdb->file_blocks[pos]);
			fh->fcb=to_check;
		}
		
		int next=fdb->header.next_block;
		DirectoryBlock db;
		
		//checking if file is in another block of the dir
		while(next!=-1 && !found){
			ret=DiskDriver_readBlock(disk, &db, next);
			//checking if file is in db
			pos=checkFileExistence(disk, DB_space, db.file_blocks, filename);
			if(pos>=0){
				found=1;
				DiskDriver_readBlock(disk, to_check, db.file_blocks[pos]);
				fh->fcb=to_check;
			}
			
			next=db.header.next_block;
			
		}
		
		if(found){
			return fh;
		}
		else{
			//printf("Cannot open file: file doesn't exist\n"); already in shell
			free(fh);
			return NULL;
		}
	}
	else{
	//if 0 entries=>directory empty
		printf("%sCannot open file: directory is empty\n%s", RED, RESET);
		return NULL;
	}
}

int SimpleFS_close(FileHandle* f){
	if(f==NULL) return -1;
	free(f->fcb);
	free(f);
	return 0;
}


int SimpleFS_formatFile(FileHandle* f){
	FirstFileBlock* ffb=f->fcb; //using an aux structure
	int blocks=ffb->fcb.size_in_blocks;
	if(blocks==1){ //if data is only stored in ffb
		memset(ffb->data, 0, FFB_space); //setting memory to 0 
		ffb->fcb.written_bytes=0;
		DiskDriver_updateBlock(f->sfs->disk, ffb, ffb->fcb.block_in_disk);
		f->pos_in_file=0; //setting cursor to the start
		return 0;
	}
	
	else{ //more blocks
		FileBlock tmp;
		int block_in_disk=ffb->fcb.block_in_disk;
		int next_block=ffb->header.next_block;
		int block_in_file=ffb->header.block_in_file;
		while(next_block!=-1){ //while there are next blocks, keep cycling
			ffb->fcb.size_in_blocks--;
			DiskDriver_readBlock(f->sfs->disk, &tmp, next_block);
			memset(tmp.data, 0, FB_space);
			DiskDriver_freeBlock(f->sfs->disk, next_block); //freeing blocks
			next_block=tmp.header.next_block;
		}
		
		memset(ffb->data, 0, FFB_space); //ffb at last
		ffb->fcb.written_bytes=0;
		DiskDriver_updateBlock(f->sfs->disk, ffb, ffb->fcb.block_in_disk);
		f->pos_in_file=0;
		
		return 0;
	}
			
			
	
	return -1;
}
	
	


int SimpleFS_write(FileHandle* f, void* data, int size){
	 FirstFileBlock* ffb=f->fcb;
	 int written_bytes=0;
	 int to_write=size;
	 int off=f->pos_in_file; //cursor
	 if(off==0 && f->fcb->fcb.written_bytes==0) printf("%sPointer set to 0.\nRemember to use %sseek%s, otherwise other %swrite%s calls will overwrite file's content\n%s", BLUE, YELLOW, BLUE, YELLOW,BLUE, RESET);
	 else if(off==0 && f->fcb->fcb.written_bytes!=0) printf("%sPointer set to 0.\nOverwriting data\n%s", RED, RESET);
	 //if bytes to be written are smaller or equal to available space
	 if(off<FFB_space && to_write<=FFB_space-off){
		 //writing bytes
		 memcpy(ffb->data+off, (char*)data, to_write);
		 written_bytes+=to_write;
		 if(f->pos_in_file+written_bytes > ffb->fcb.written_bytes){
			 ffb->fcb.written_bytes=f->pos_in_file+written_bytes;
		 }
		 DiskDriver_updateBlock(f->sfs->disk, ffb, ffb->fcb.block_in_disk);
		 printf("\n%s%d%s bytes wrote", YELLOW, written_bytes, RESET);
		 return written_bytes;
	 }
	 
	 else if(off<FFB_space && to_write>FFB_space-off){
		 memcpy(ffb->data+off, (char*)data, FFB_space-off);
		 written_bytes+=FFB_space-off;
		 to_write=size-written_bytes;
		 DiskDriver_updateBlock(f->sfs->disk, ffb, ffb->fcb.block_in_disk);
		 off=0;
	 }
	 else off-=FFB_space; //next_block is not empty
	 
	 //id on disk current block
	 int block_in_disk=ffb->fcb.block_in_disk;
	 //id on disk next block
	 int next_block=ffb->header.next_block;
	 //id on file current block
	 int block_in_file=ffb->header.block_in_file;
	 FileBlock tmp;
	 int one_block=0;
	 if(next_block==-1) one_block=1;
	 
	 while(written_bytes<size){
		 if(next_block==-1){
			 ffb->fcb.size_in_blocks+=1;
			 //a new block if space for data is over
			 FileBlock new={0};
			 new.header.block_in_file=block_in_file+1;
			 new.header.next_block=-1;
			 new.header.previous_block=block_in_disk;
			 
			 //updating next block's id
			 next_block=DiskDriver_getFreeBlock(f->sfs->disk, block_in_disk);
			 if(one_block==1){ //No next block allocated
				 ffb->header.next_block=next_block;
				 DiskDriver_updateBlock(f->sfs->disk, ffb, ffb->fcb.block_in_disk);
				 one_block=0;
			 }
			 else{
				 //updating next_block's id of previous fileblock
				 tmp.header.next_block=next_block;
				 DiskDriver_updateBlock(f->sfs->disk, &tmp, block_in_disk);
			 }
			 
			 //writing changes
			 DiskDriver_writeBlock(f->sfs->disk, &new, next_block);
			 
			 tmp=new;
			 
		 }
		 
		 else{
			 //reading FileBlock from disk
			 if(DiskDriver_readBlock(f->sfs->disk, &tmp, next_block)==-1) return -1;
		 }
		 
		 //if bytes to write are smaller than available space
		 if(off<FB_space && to_write <= FB_space-off){
			 //writing the last bytes then exiting the cycle
			 memcpy(tmp.data+off, (char*)data+written_bytes, to_write);
			 written_bytes+=to_write;
			 if(f->pos_in_file+written_bytes>ffb->fcb.written_bytes){
				 ffb->fcb.written_bytes=f->pos_in_file+written_bytes;
			 }
			 DiskDriver_updateBlock(f->sfs->disk, ffb, ffb->fcb.block_in_disk);
			 DiskDriver_updateBlock(f->sfs->disk, &tmp, next_block);
			 printf("\n%s%d%s bytes wrote", YELLOW, written_bytes, RESET);
			 return written_bytes;
		 }
		 
		 else if(off<FB_space && to_write>FB_space-off){
			 //writing last bytes (same as before), then cycling again with next block
			 memcpy(tmp.data+off, (char*)data+written_bytes, FB_space-off);
			 written_bytes+=FB_space-off;
			 to_write=size-written_bytes;
			 DiskDriver_updateBlock(f->sfs->disk, &tmp, next_block);
			 off=0;
		 }
		 
		 else off-=FB_space;
		 
		 block_in_disk=next_block; //id current block
		 next_block=tmp.header.next_block;
		 block_in_file=tmp.header.block_in_file; //id next block
	 }
	 printf("\n%s%d%s bytes wrote", YELLOW, written_bytes, RESET);
	 return written_bytes;
 }
	
	
int SimpleFS_read(FileHandle* f, void* data, int size){
	FirstFileBlock* ffb=f->fcb;
	
	int off=f->pos_in_file;
	int written_bytes=ffb->fcb.written_bytes;
	
	if(size+off>written_bytes){
		printf("Invalid size: size too big\n");
		memset(data, 0, size);
		return -1;
	}
	
	int bytes_read=0;
	int to_read=size;
	
	//here we have 2 main cases: data to read is all stored in FFB or
	//is also stored in next FB's
	if(off<FFB_space && to_read<=FFB_space-off){ //bytes to read<=available space
		memcpy(data, ffb->data+off, to_read);
		bytes_read+=to_read;
		to_read=size-bytes_read;
		f->pos_in_file+=bytes_read; //updating pointer
		return bytes_read;
	}
	//else, we continue (more than one block)
	else if(off<FFB_space && to_read>FFB_space-off){
		memcpy(data, ffb->data+off, FFB_space-off);
		bytes_read+=FFB_space-off;
		to_read=size-bytes_read;
		off=0;
	}
	
	else off-=FFB_space;
	
	int next_block=ffb->header.next_block; //id of next block
	FileBlock tmp;
	
	//cycling 'till all bytes are read 
	while(bytes_read<size && next_block!=-1){
		DiskDriver_readBlock(f->sfs->disk, &tmp, next_block);
		//bytes<=available space
		if(off<FB_space && to_read<=FB_space-off){
			memcpy(data+bytes_read, tmp.data+off, to_read);
			bytes_read+=to_read;
			to_read=size-bytes_read;
			f->pos_in_file+=bytes_read;
			return bytes_read;
		}
		//else we keep cycling
		else if(off<FB_space && to_read > FB_space-off){
			memcpy(data+bytes_read, tmp.data+off, FB_space-off);
			bytes_read+=FB_space-off;
			to_read=size-bytes_read;
			off=0;
		}
		
		else off-=FB_space;
		
		next_block=tmp.header.next_block;
	}
	
	return bytes_read;
}

int SimpleFS_seek(FileHandle* f, int pos){
	if(pos<0) ERROR_HELPER(-1, "Cannot seek: wrong position\n"); //checking if valid pos
	//using an aux structure
	FirstFileBlock* ffb=f->fcb;
	
	if(pos>ffb->fcb.written_bytes){
		printf("%sInvalid position\n%s", RED, RESET);
		return -1;
	}
	
	f->pos_in_file=pos;
	return pos;
}

//A simple function that checks if a directory 
//with the same name already exists,
//returns "i" if file is found
static int checkDirExistence(DiskDriver* disk, int entries, int* file_blocks, const char* filename){
	FirstDirectoryBlock to_check;
	int i;
	
	for(i=0;i<entries; i++){
		if(file_blocks[i]>0 && DiskDriver_readBlock(disk, &to_check, file_blocks[i])!=-1){
			if(!strncmp(to_check.fcb.name, filename, MAX_NAME_LEN)){
				return i;
			}
		}
	}
	return -1;
}


int SimpleFS_changeDir(DirectoryHandle* d, char* dirname){
	if(d==NULL || dirname==NULL) ERROR_HELPER(-1, "Cannot change directory: Bad Parameters\n");
	
	int ret=0;
	//checking if it's the "goto parent directory" command
	if(!strncmp(dirname, "..", 2)){
		//checks if root
		if(d->dcb->fcb.block_in_disk==0){
			printf("%sCannot read parent directory, this is root dir\n%s", RED, RESET);
			return -1;
		}
		
		d->pos_in_block=0; //resets directory
		d->dcb=d->directory; //it's now the parent directory
		//saving its first block
		int parent_block=d->dcb->fcb.directory_block;
		if(parent_block==-1){ //it's root
			d->directory=NULL;
			return 0;
		}
		
		FirstDirectoryBlock* parent=malloc(sizeof(FirstDirectoryBlock));
		//reading the parent directory
		ret=DiskDriver_readBlock(d->sfs->disk, parent, parent_block);
		if(ret==-1){
			printf("Cannot read parent directory while going back\n");
			d->directory=NULL;
		} else{
			d->directory=parent; //saving correct parent
		}
		return 0;
	} else if(d->dcb->num_entries<0){
		printf("%sCannot change directory: directory is empty\n%s", RED, RESET);
		return -1;
	}
	
	//other cases now
	FirstDirectoryBlock* fdb=d->dcb;
	DiskDriver* disk=d->sfs->disk;
	
	FirstDirectoryBlock* to_check=malloc(sizeof(FirstDirectoryBlock));
	
	//checking if it's in the FirstDirectoryBlock
	int pos=checkDirExistence(disk, FDB_space, fdb->file_blocks, dirname);
	if(pos>=0){
		DiskDriver_readBlock(disk, to_check, fdb->file_blocks[pos]);
		d->pos_in_block=0;
		d->directory=fdb;
		d->dcb=to_check;
		return 0;
	}
	
	int next=fdb->header.next_block;
	DirectoryBlock db;
	
	//if it's in any other blocks in the directory
	while(next!=-1){
		ret=DiskDriver_readBlock(disk, &db, next);
		if(ret==-1){
			printf("Cannot read all the directories\n");
			return -1;
		}
		//checking function
		pos=checkDirExistence(disk, DB_space, db.file_blocks, dirname);
		if(pos>=0){
			DiskDriver_readBlock(disk, to_check, db.file_blocks[pos]);
			d->pos_in_block;
			d->directory=fdb;
			d->dcb=to_check;
			return 0;
		}
		next=db.header.next_block; //going next block, in loop
	}
	
	printf("%sCannot change directory, it doesn't exist\n%s", RED, RESET);
	return -1;
}

		
int SimpleFS_mkDir(DirectoryHandle* d, char* dirname){
	if(d==NULL || dirname==NULL) ERROR_HELPER(-1, "Cannot create directory, bad parameters\n");
	
	int ret=0;
	//no side effect
	FirstDirectoryBlock* fdb=d->dcb;
	DiskDriver* disk=d->sfs->disk;
	
	if(fdb->num_entries>0){
		//checking if a same name dir already exists in FirstDirectoryBlock
		if(checkDirExistence(disk, FDB_space, fdb->file_blocks, dirname)!=-1){
			printf("Cannot create directory: directory already exists\n");
			return -1;
		}
		
		int next=fdb->header.next_block;
		DirectoryBlock db;
		//checking now in others directory blocks
		while(next!=-1){
			ret=DiskDriver_readBlock(disk, &db, next); //reading db from disk
			if(ret==-1){
				printf("Cannot create directory\n");
				return -1;
			}
			
			//checking in DirectoryBlock
			if(checkDirExistence(disk, DB_space, db.file_blocks, dirname)!=-1){
				printf("Cannot create directory: directory already exists\n");
				return -1;
			}
			next=db.header.next_block; //going next
		}
	}
	
	//searching now for a free block in the disk
	int new_block=DiskDriver_getFreeBlock(disk, disk->header->first_free_block);
	if(new_block==-1){
		printf("Cannot create directory: cannot find any free block\n");
		return -1;
	}
	
	FirstDirectoryBlock* new_dir=calloc(1, sizeof(FirstFileBlock)); //setting 0's
	new_dir->header.block_in_file=0;
	new_dir->header.next_block=-1;
	new_dir->header.previous_block=-1;
	//filling FileControlBlock
	new_dir->fcb.directory_block=fdb->fcb.block_in_disk;
	new_dir->fcb.block_in_disk=new_block;
	new_dir->fcb.is_dir=1;
	strcpy(new_dir->fcb.name, dirname);
	
	//writing dir on disk
	ret=DiskDriver_writeBlock(disk, new_dir, new_block);
	if(ret==-1){
		printf("Cannot create directory: writeBlock error\n");
		return -1;
	}
	free(new_dir);
	
	int i=0;
	int found=0;
	int block_num=fdb->fcb.block_in_disk; //# of the block in disk
	DirectoryBlock db_last; //in case of no space in FirstDirectoryBlock
	
	int entry=0; //number of entry in file_blocks of db/fdb
	int blockInFile=0; //number of block in file
	int first=0; //if 0, saves it in next block after fdb, if 1 after db_last
	int where_to_save=0; //if 0, space available in fdb, if 1, otherwise
	
	//checking for free space in fdb
	if(fdb->num_entries<FDB_space){
		int* blocks=fdb->file_blocks;
		for(i=0; i<FDB_space; i++){
			if(blocks[i]==0){ //free space condition
				found=1;
				entry=i;
				break;
			}
		}
	}
	else{ //no space in fdb
		where_to_save=1;
		int next=fdb->header.next_block;
		
		while(next!=-1 && !found){ //finding position in other DirectoryBlocks
			ret=DiskDriver_readBlock(disk, &db_last, next);
			if(ret==-1){
				printf("Cannot create directory: readBlock problem\n");
				return -1;
			}
			int* blocks=db_last.file_blocks;
			blockInFile++;
			block_num=next;
			for(i=0; i<DB_space; i++){ //pos inside the dir block
				if(blocks[i]==0){
					found=1;
					entry=i;
					break;
				}
			}
			
			first=1;
			next=db_last.header.next_block;
		}
	}
	
	if(!found){ //if all Dir Blocks are full
		DirectoryBlock new={0};
		new.header.next_block=-1;
		new.header.block_in_file=blockInFile;
		new.header.previous_block=block_num;
		new.file_blocks[0]=new_block;
		
		//looking for a free block inside the disk
		int new_dir_block=DiskDriver_getFreeBlock(disk, disk->header->first_free_block);
		if(new_dir_block==-1){
			printf("Cannot create directory: no free blocks\n");
			//freeing what we already wrote
			DiskDriver_freeBlock(disk, fdb->fcb.block_in_disk);
			return -1;
		}
		//writing block on disk
		ret=DiskDriver_writeBlock(disk, &new, new_dir_block);
		if(ret==-1){
			printf("Cannot create directory: writeBlock problem\n");
			DiskDriver_freeBlock(disk, fdb->fcb.block_in_disk);
			return -1;
		}	
		
		//updating header both cases
		if(first==0){
			fdb->header.next_block=new_dir_block;
		} else{
			db_last.header.next_block=new_dir_block;
		}
		db_last=new;
		block_num=new_dir_block;
		
	}
	
	if(where_to_save==0){ //fdb case
		fdb->num_entries++;
		fdb->file_blocks[entry]=new_block;
		DiskDriver_updateBlock(disk, fdb, fdb->fcb.block_in_disk);
	}
	else{ //db case
		fdb->num_entries++;
		DiskDriver_updateBlock(disk, fdb, fdb->fcb.block_in_disk);
		db_last.file_blocks[entry]=new_block;
		DiskDriver_updateBlock(disk, &db_last, block_num);
	}
	
	return 0;
}
	

int SimpleFS_remove(DirectoryHandle* d, char* filename){
	if(d==NULL || filename==NULL) ERROR_HELPER(-1, "Cannot remove directory, bad parameters\n");
	FirstDirectoryBlock* fdb=d->dcb;
	
	int id=checkFileExistence(d->sfs->disk, FDB_space, fdb->file_blocks, filename);
	int first=1;
	
	//using an aux structure
	DirectoryBlock* db_tmp=(DirectoryBlock*)malloc(sizeof(DirectoryBlock));
	int next_block=fdb->header.next_block;
	int block_in_disk=fdb->fcb.block_in_disk;
	
	//if file isn't in fdb, continue
	while(id==-1){
		if(next_block!=-1){ //other blocks, same dir
			first=0;
			if(DiskDriver_readBlock(d->sfs->disk, db_tmp, next_block)==-1){
				printf("%sCannot remove file: readBlock problem\n%s", RED, RESET);
				return -1;
			}
			id=checkFileExistence(d->sfs->disk, DB_space, db_tmp->file_blocks, filename);
			block_in_disk=next_block;
			next_block=db_tmp->header.next_block;
		}
		
		else{ //blocks over
			//printf("%sCannot remove file: filename not valid\n%s", RED, RESET);
			return -1;
		}
	}
	
	int idf, ret;
	if(first==0) idf=db_tmp->file_blocks[id];
	else idf=fdb->file_blocks[id];
	
	FirstFileBlock ffb_rm;
	if(DiskDriver_readBlock(d->sfs->disk, &ffb_rm, idf)==-1){
		printf("%sCannot delete file: readBlock error\n%s", RED, RESET);
		return -1;
	}
	if(ffb_rm.fcb.is_dir==0){ //checking if it's a directory
		FileBlock fb_tmp;
		int next=ffb_rm.header.next_block;
		int block_in_disk=idf;
		while(next!=-1){
			if(DiskDriver_readBlock(d->sfs->disk, &fb_tmp, next)==-1){
				printf("%sCannot delete file, readBlock problem\n%s", RED, RESET);
				return -1;
			}
			block_in_disk=next;
			next=fb_tmp.header.next_block;
			DiskDriver_freeBlock(d->sfs->disk, block_in_disk);
		}
		DiskDriver_freeBlock(d->sfs->disk, idf);
		d->dcb=fdb;
		ret=0;
	}
	else{ //directory case, recursive remove
		FirstDirectoryBlock fdb_rm;
		if(DiskDriver_readBlock(d->sfs->disk, &fdb_rm, idf)==-1){
			printf("%sCannot delete file, readBlock problem\n%s", RED, RESET);
			return -1;
		}
		if(fdb_rm.num_entries>0){
			if(SimpleFS_changeDir(d, fdb_rm.fcb.name)==-1){
				printf("%sCannot delete directory\n%s", RED, RESET);
				return -1;
			}
			int i;
			//deleting each file inside the directory, fdb case
			for(i=0; i<FDB_space; i++){
				FirstFileBlock ffb;
				if(fdb_rm.file_blocks[i]>0 && DiskDriver_readBlock(d->sfs->disk, &ffb, fdb_rm.file_blocks[i])!=-1){
					SimpleFS_remove(d, ffb.fcb.name); //recursion
				}
			}
			int next=fdb_rm.header.next_block;
			int block_in_disk=idf;
			DirectoryBlock db_tmp;
			while(next!=-1){
				if(DiskDriver_readBlock(d->sfs->disk, &db_tmp, next)==-1){
					printf("Cannot delete directory: readBlock problem\n");
					return -1;
				}
				int j;
				//db case
				for(j=0; j<DB_space; j++){
					FirstFileBlock ffb;
					if(DiskDriver_readBlock(d->sfs->disk, &ffb, db_tmp.file_blocks[j])==-1){
						printf("Cannot remove directory: readBlock problem\n");
						return -1;
					}
					SimpleFS_remove(d, ffb.fcb.name); //recursion
				}
				block_in_disk=next;
				next=db_tmp.header.next_block;
				DiskDriver_freeBlock(d->sfs->disk, block_in_disk);
			}
			DiskDriver_freeBlock(d->sfs->disk, idf);
			d->dcb=fdb;
			ret=0;
		}
		else{
			DiskDriver_freeBlock(d->sfs->disk, idf);
			d->dcb=fdb;
			ret=0;
		}
	}
	
	if(first==0){
		db_tmp->file_blocks[id]=-1;
		fdb->num_entries=-1;
		DiskDriver_updateBlock(d->sfs->disk, db_tmp, block_in_disk);
		DiskDriver_updateBlock(d->sfs->disk, fdb, fdb->fcb.block_in_disk);
		free(db_tmp);
		return ret;
	}
	
	else{
		fdb->file_blocks[id]=-1;
		fdb->num_entries-=1;
		DiskDriver_updateBlock(d->sfs->disk, fdb, fdb->fcb.block_in_disk);
		free(db_tmp);
		return ret;
	}
	
	return -1;
}
