#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "shell.h"

//quick compare for the shell
int cmp(char arg_buf[MAX_ARGS][MAX_LINE], char* s){
	return(!strcmp(arg_buf[0], s));
}

//related to printBitMap
int findBit(unsigned char b, int n) {
    static unsigned char mask2[] = {128, 64, 32, 16, 8, 4, 2, 1};
    return ((b & mask2[n]) != 0);
}

//prints bitmap entries, for testing purposes
void printBitMap(BitMap* bmp, int a){
	int i, bit;
	for(i=0;i<8;i++){
		bit=findBit(bmp->entries[a], i);
		printf("%d", bit);
	}
}
		
	

void get_cmd_line(char cmd_line[MAX_LINE], char arg_buf[MAX_ARGS][MAX_LINE], unsigned* arg_num_ptr){
	unsigned arg_num=0; //number of token
	char buf[MAX_LINE]; //to store the integral cmd line
	strcpy(buf, cmd_line); //copying
	const char* delim=" \n\t";
	char* token=strtok(buf, delim); //tokenizes buffer
	while(token!=NULL){
		strcpy(arg_buf[arg_num], token);
		token=strtok(NULL, delim); //next token
		arg_num++;
	}
	
	*arg_num_ptr=arg_num; //exact tokens
}

//usage of fs commands
void print_cmd_info(void){
	printf("\t\t\t\t%sUsage:\n\t%stest (t)%s \t\t\t Runs a test of all functions \n\n", YELLOW, RED, RESET);
	printf("\t%stouch%s <file1> <file2> ... \t Creates empty files\n", RED, RESET);
	printf("\t%sls%s <dir> \t\t\t Reads a directory\n", RED, RESET);
	printf("\t%sopen%s <file> \t\t\t Opens a file\n", RED, RESET);
	printf("\t%srm%s <file1> ... <dir1> ... \t Removes a file\n", RED, RESET);
	printf("\t%scd%s <dir> \t\t\t Changes current directory\n", RED, RESET);
	printf("\t%smkdir%s <dir1> <dir2> ... \t Creates a new directory\n", RED, RESET);
	printf("\t%spcd%s \t\t\t\t Prints current directory\n", RED, RESET);
	printf("\n\t%shelp (h)%s \t\t\t Prints command info\n", RED, RESET);
	printf("\t%squit (q)%s \t\t\t Quits the program\n\n", RED, RESET);
}

//usage of file commands
void print_file_info(void){
	printf("\t\t\t%sUsage:\n\t%swrite%s <data> \t\t Writes %sdata%s on file\n", YELLOW, RED, RESET, GREEN, RESET);
	printf("\t%sread%s <size> \t\t Reads %ssize%s bytes on file\n", RED, RESET, GREEN, RESET);
	printf("\t%sformat%s \t\t\t Removes all the content on file\n", RED, RESET);
	printf("\t%sseek%s <pos> \t\t Moves file pointer at %spos%s\n", RED, RESET, GREEN, RESET);
	printf("\n\t%sinfo%s (i) \t\t Prints file info\n", RED, RESET);
	printf("\t%shelp%s (h) \t\t Prints command info\n", RED, RESET);
	printf("\t%sclose%s (c)\t\t Closes file\n", RED, RESET);
}


//recursive function to find the current directory path
void do_pcd(DiskDriver* disk, FirstDirectoryBlock* fdb, int block_in_disk){
	if(block_in_disk==-1) return;
	DiskDriver_readBlock(disk, fdb, block_in_disk);
	block_in_disk=fdb->fcb.directory_block;
	char dir[MAX_NAME_LEN];
	strncpy(dir, fdb->fcb.name, MAX_NAME_LEN);
	do_pcd(disk, fdb, block_in_disk);
	if(strcmp(dir, "/")==0) printf("%sroot%s", GREEN, RESET);
	else printf("/%s", dir);
}


int do_file_cmd(FileHandle* f, char arg_buf[MAX_ARGS][MAX_LINE], unsigned arg_num){
	//close
	if(cmp(arg_buf, "close") || cmp(arg_buf, "c")) return -1;
	
	
	//help
	else if(cmp(arg_buf, "help") || cmp(arg_buf, "h")) print_file_info();
	
	
	//info
	else if(cmp(arg_buf, "info") || cmp(arg_buf, "i")){
		printf("\t%sFILE '%s'--->\n%s", YELLOW, f->fcb->fcb.name, RESET);
		printf("\n\t\t%sPosition on file:%s %d\n", RED, RESET, f->pos_in_file);
		printf("\t\t%sBytes written: %s%d\n", RED, RESET, f->fcb->fcb.written_bytes);
		printf("\t\t%sMax current readable size: %s%d\n", RED, RESET, f->fcb->fcb.written_bytes - f->pos_in_file);
		printf("\t\t%sSize in blocks: %s%d\n", RED, RESET, f->fcb->fcb.size_in_blocks);
	}
	
	
	//format
	else if(cmp(arg_buf, "format")){
		if(arg_num==1){
			if(SimpleFS_formatFile(f)==0) printf("%sContent successfully removed%s", YELLOW, RESET);
		}
		else printf("%sUsage:%s format\n", YELLOW, RESET);
	}
	
	
	//write
	else if(cmp(arg_buf, "write")){
		if(arg_num==2){
			int size=strlen(arg_buf[1]);
			//char* data=(char*)malloc(sizeof(char)*size);
			//strncpy(data, arg_buf[1], size);
			//sprintf(data, "%s", arg_buf[1]);
			SimpleFS_write(f, arg_buf[1], size);
		}
		else printf("%sUsage:%s write <data>\n", YELLOW, RESET);
	}
	
	
	//read
	else if(cmp(arg_buf, "read")){
		if(arg_num==2){
			int size=atoi(arg_buf[1]);
			char* data=(char*)malloc(sizeof(char)*size+1);
			data[size]='\0'; //string terminator
			SimpleFS_read(f, data, size);
			printf("%s", data);
			free(data);
		}
		else printf("%sUsage:%s read <size>\n", YELLOW, RESET);
	}
	
	//seek
	else if(cmp(arg_buf, "seek")){
		if(arg_num>1){
			int pos=atoi(arg_buf[1]);
			SimpleFS_seek(f, pos);
		}
		else printf("%sUsage:%s seek <pos>\n", YELLOW, RESET);
	}
	else{
		printf("%sCommand not found, please retry.%s Press 'h' or 'help' for usage", RED, RESET);
	} 
		printf("\n");
		return 0;
}



//testing commands
void do_test_cmd(void){
	int ret;
	printf("%sGeneral info:%s\n", YELLOW, RESET);
	printf("Block size %ld\n", sizeof(FirstFileBlock));
    printf("FDB_space = %ld, DB_space = %ld, FFB_space = %ld, FB_space = %ld\n", FDB_space, DB_space, FFB_space, FB_space);
	printf("\nWe'll now run some tests...\n\n");
	
	//***************
	//Bitmap test
	printf("%sBITMAP%s\n", RED, RESET);
	printf("Creating bitmap with 2 entries, '150' and '240'...");
	BitMap* bmp=(BitMap*)malloc(sizeof(BitMap));
	bmp->num_bit=16;
	bmp->entries=(char*)malloc(sizeof(char)*2);
	bmp->entries[0]=150;
	bmp->entries[1]=240;
	printf("success\n");
	printf("10010110  ");
	printf("11110000\n\n");
	
	printf("Getting the first bit with status 0, starting from pos 4\n");
	printf("Result: %d [CORRECT=5]\n\n", BitMap_get(bmp, 4, 0));
	
	printf("Setting this bit to 1...\n");
	BitMap_set(bmp, 5, 1);
	//binary printing
	printBitMap(bmp, 0);
	printf(" [CORRECT=10110110]\n\n");
	
	printf("Getting the first bit with status 1, starting from pos 8\n");
	printf("Result: %d [CORRECT=12]\n", BitMap_get(bmp, 10, 1));	
	free(bmp->entries);
	free(bmp);
	
	//***************
	//DiskDriver TEST
	printf("\n%sDISKDRIVER%s\n", RED, RESET);
	printf("Creating a new disk with 4 blocks, called 'test.disk'...");
	DiskDriver disk;
	DiskDriver_init(&disk, "test.disk", 4);
	printf("success\n");
	DiskDriver_flush(&disk);
	printf("free:%d\n", disk.header->free_blocks);
	
	printf("Creating some blocks to write on disk...");
	
	BlockHeader h;
	h.previous_block=2;
	h.next_block=2;
	h.block_in_file=2;
	
	FileBlock* fb1=(FileBlock*)malloc(sizeof(FileBlock));
	fb1->header=h;
	char data1[BLOCK_SIZE-sizeof(BlockHeader)];
	for(int j=0; j<BLOCK_SIZE-sizeof(BlockHeader);j++){
		data1[j]='8';
	}
	//string terminator for strcpy
	data1[BLOCK_SIZE-sizeof(BlockHeader)-1]='\0';
	strcpy(fb1->data, data1);
	
	FileBlock* fb2=(FileBlock*)malloc(sizeof(FileBlock));
	fb2->header=h;
	char data2[BLOCK_SIZE-sizeof(BlockHeader)];
	for(int j=0; j<BLOCK_SIZE-sizeof(BlockHeader); j++){
		data2[j]='7';
	}
	data2[BLOCK_SIZE-sizeof(BlockHeader)-1]='\0'; 
	strcpy(fb2->data, data2);
	
	FileBlock* fb3=(FileBlock*)malloc(sizeof(FileBlock));
	fb3->header=h;
	char data3[BLOCK_SIZE-sizeof(BlockHeader)];
	for(int j=0; j<BLOCK_SIZE-sizeof(BlockHeader); j++){
		data3[j]='6';
	}
	data3[BLOCK_SIZE-sizeof(BlockHeader)-1]='\0';
	strcpy(fb3->data, data3);
	
	FileBlock* fb4=(FileBlock*)malloc(sizeof(FileBlock));
	fb4->header=h;
	char data4[BLOCK_SIZE-sizeof(BlockHeader)];
	for(int j=0; j<BLOCK_SIZE-sizeof(BlockHeader); j++){
		data4[j]='5';
	}
	data4[BLOCK_SIZE-sizeof(BlockHeader)-1]='\0';
	strcpy(fb4->data, data4);
	printf("success\n");
	
	printf("Writing now block 0\n");
	DiskDriver_writeBlock(&disk, fb1, 0);
	DiskDriver_flush(&disk);
	printf("Done. Free blocks: %d\nFirst free block: %d\n\n", disk.header->free_blocks, disk.header->first_free_block);
	
	printf("Writing now block 1\n");
	DiskDriver_writeBlock(&disk, fb2, 1);
	DiskDriver_flush(&disk);
	printf("Done. Free blocks: %d\nFirst free block: %d\n\n", disk.header->free_blocks, disk.header->first_free_block);
	
	printf("Writing now block 2\n");
	DiskDriver_writeBlock(&disk, fb3, 2);
	DiskDriver_flush(&disk);
	printf("Done. Free blocks: %d\nFirst free block: %d\n\n", disk.header->free_blocks, disk.header->first_free_block);
	
	printf("Writing now block 3\n");
	DiskDriver_writeBlock(&disk, fb4, 3);
	DiskDriver_flush(&disk);
	printf("Done. Free blocks: %d\nFirst free block: %d\n\n", disk.header->free_blocks, disk.header->first_free_block);
	
	
	printf("Reading them now...\n");
	FileBlock* read_b=(FileBlock*)malloc(sizeof(FileBlock));
	DiskDriver_readBlock(&disk, read_b, 0);
	printf("Block 0: %s\n\n", read_b->data);
	DiskDriver_readBlock(&disk, read_b, 1);
	printf("Block 1: %s\n\n", read_b->data);
	DiskDriver_readBlock(&disk, read_b, 2);
	printf("Block 2: %s\n\n", read_b->data);
	DiskDriver_readBlock(&disk, read_b, 3);
	printf("Block 3: %s\n\n", read_b->data);
	
	
	
	printf("\nFreeing now all the blocks and deleting disk...\n");
	DiskDriver_freeBlock(&disk, 0);
	DiskDriver_freeBlock(&disk, 1);
	DiskDriver_freeBlock(&disk, 2);
	DiskDriver_freeBlock(&disk, 3);
	printf("Free blocks: %d\n", disk.header->free_blocks);
	free(fb1);
	free(fb2);
	free(fb3);
	free(fb4);
	free(read_b);
	printf("Removing disk...\n");
	ret=remove("test.disk");
	ERROR_HELPER(ret, "Critical error");
	printf("Disk removed\n");
	
	//***************
	//simplefs
	printf("\n%sSIMPLEFS%s\n", RED, RESET);
	printf("Creating a new disk (128 blocks) and a new SimpleFS...");
	DiskDriver disk2;
	SimpleFS sfs;
	DiskDriver_init(&disk2, "test2.txt", 128);
	DiskDriver_flush(&disk2);
	DirectoryHandle* dh=SimpleFS_init(&sfs, &disk2);
	if(dh==NULL){
		SimpleFS_format(&sfs);
		dh=SimpleFS_init(&sfs, &disk2);
		if(dh==NULL) exit(EXIT_FAILURE);
	}
	printf("success\n\n");
	
	printf("Creating a file...");
	FileHandle* fh=SimpleFS_createFile(dh, "prova.txt");
	if(fh==NULL) exit(EXIT_FAILURE);
	else{
		printf("success\n\n");
		SimpleFS_close(fh);
	}
	
	printf("Creating a directory named 'testdir'...");
	SimpleFS_mkDir(dh, "testdir");
	if(dh==NULL) exit(EXIT_FAILURE);
	printf("success\n\n");
	
	printf("Changing dir to 'testdir'...");
	SimpleFS_changeDir(dh, "testdir");
	printf("success\n\n");
	printf("Path: \n");
	FirstDirectoryBlock* tmp=(FirstDirectoryBlock*)malloc(sizeof(FirstDirectoryBlock));
	do_pcd(dh->sfs->disk, tmp, dh->dcb->fcb.block_in_disk);
	free(tmp);
	
	printf("\n\nCreating 20 files inside this directory...");
	char filename[10];
	FileHandle* multi=NULL;
	int i;
	for(i=0; i<20; i++){
		sprintf(filename, "%d", i);
		multi=SimpleFS_createFile(dh, filename);
	}
	if(multi!=NULL) SimpleFS_close(multi);
	printf("success\n\n");
	
	printf("Reading 'testdir'...\n");
	char** files = (char**) malloc(sizeof(char*) * dh->dcb->num_entries);
	ret = SimpleFS_readDir(files, dh);
	for (i=0; i<dh->dcb->num_entries; i++){
		printf("%s ", files[i]);
	}
	for (i=0; i<dh->dcb->num_entries; i++){
		free(files[i]);
	}
	free(files);
	
	printf("\n\nRemoving file '11'...");
	SimpleFS_remove(dh, "11");
	printf("success\n\n");
	
	char** files2 = (char**) malloc(sizeof(char*) * dh->dcb->num_entries);
	ret = SimpleFS_readDir(files2, dh);
	for (i=0; i<dh->dcb->num_entries; i++){
		printf("%s ", files2[i]);
	}
	for (i=0; i<dh->dcb->num_entries; i++){
		free(files2[i]);
	}
	free(files2);
	
	printf("\n\nGetting back to root...");
	SimpleFS_changeDir(dh, "..");
	SimpleFS_remove(dh, "testdir");
	SimpleFS_format(&sfs);
	
	
	printf("\n\n%sUse the shell to test by yourself all the functions of the project%s\n", YELLOW, RESET);
}

void do_file_loop(FileHandle* f){
	char cmd_line[MAX_LINE];
	char arg_buf[MAX_ARGS][MAX_LINE];
	unsigned arg_num;
	
	while(1){
		printf("%smyFS:%s> %s", GREEN, f->fcb->fcb.name, RESET);
		if(fgets(cmd_line, MAX_LINE, stdin)==NULL) break;
		get_cmd_line(cmd_line, arg_buf, &arg_num);
		if(arg_num==0) continue;
		int res=do_file_cmd(f, arg_buf, arg_num);
		if(res==-1) break;
	}
	return;
}

int do_cmd(DirectoryHandle* dh, char arg_buf[MAX_ARGS][MAX_LINE], unsigned arg_num){
	//quit
	if(cmp(arg_buf, "quit") || cmp(arg_buf, "q")){
		free(dh);
		return -1;
	}
	
	//help
	else if(cmp(arg_buf, "help") || cmp(arg_buf, "h")){
		print_cmd_info();
	}
	
	//read directory
	else if(cmp(arg_buf, "ls")){
		char name[MAX_NAME_LEN];
		char** names=(char**)malloc(sizeof(char*)*dh->dcb->num_entries);
		SimpleFS_readDir(names, dh);
		for(int i=0; i<dh->dcb->num_entries; i++){
			sprintf(name, "%s", names[i]);
			assert(!strcmp(names[i], name));
			printf("%s  ", names[i]);
		}
		free(names);
	}
	
	//change directory
	else if(cmp(arg_buf, "cd")){
		if(arg_num==2){
			char dirname[MAX_NAME_LEN];
			if(strncmp(arg_buf[1], "..", 2)==0){ //goback
				SimpleFS_changeDir(dh, arg_buf[1]);
			}else{
				sprintf(dirname, "%s%s%s", GREEN, arg_buf[1], RESET);
				SimpleFS_changeDir(dh, dirname);
			}
		}
		else printf("%sUsage:%s cd <dir>\n", YELLOW, RESET);
	}
	
	//create file
	else if(cmp(arg_buf, "touch")){
		char name[MAX_NAME_LEN];
		if(arg_num>1){
			for(int i=1; i<arg_num; i++){
				sprintf(name, "%s", arg_buf[i]);
				SimpleFS_createFile(dh, name);
			}
		}
		else printf("%sUsage:%s touch <file1> <file2> ...\n", YELLOW, RESET);
	}
	
	
	//create directory
	else if(cmp(arg_buf, "mkdir")){
		char name[MAX_NAME_LEN];
		if(arg_num>1){
			for(int i=1; i<arg_num; i++){
				sprintf(name, "%s%s%s", GREEN, arg_buf[i], RESET);
				SimpleFS_mkDir(dh, name);
			}
		}
		else printf("%sUsage:%s mkdir <dir1> <dir2> ...\n", YELLOW, RESET);
	}
	
	
	//open file
	else if(cmp(arg_buf, "open")){
		if(arg_num==2){
			char name[MAX_NAME_LEN];
			sprintf(name, "%s", arg_buf[1]);
			FileHandle* f=SimpleFS_openFile(dh, name);
			if(f==NULL){
				printf("%sError: file doesn't exist\n%s", RED, RESET);
				return 0;
			}
			do_file_loop(f);
			SimpleFS_close(f);
			printf("%sClosing file...%s", YELLOW, RESET);
		}
		else printf("%sUsage:%sopen <file>\n", YELLOW, RESET);
	}
	
	
	//test
	else if(cmp(arg_buf, "test") || cmp(arg_buf, "t")){
		if(arg_num==1){
			do_test_cmd();
		}
		else printf("%sUsage: test (t)\n%s", YELLOW, RESET);
	}
	
	
	//remove file
	else if(cmp(arg_buf, "rm")){
		int ret=0;
		if(arg_num>1){
			int c1=0;
			int c2=0;
			char name[MAX_NAME_LEN];
			for(int i=1; i<arg_num; ++i){
				sprintf(name, "%s", arg_buf[i]);
				ret=SimpleFS_remove(dh, name);
				if(ret==-1){
					sprintf(name, "%s%s%s", GREEN, arg_buf[i], RESET);
					ret=SimpleFS_remove(dh, name);
					if(ret==-1){
						printf("%sCannot remove file: it doesn't exist%s\n", RED, RESET);
						return 0;
					}
					else c2++;
				}else c1++;
			}
			printf("%sSuccessfully removed %s%d%s directory(ies) and %s%d%s file(s)", YELLOW, GREEN, c2, RESET, RED, c1, RESET); 
		}
		else printf("%sUsage:%srm <file1> <file2>...\n", YELLOW, RESET);
	}
	
	//print current directory
	else if(cmp(arg_buf, "pcd")){
		int this=dh->dcb->fcb.block_in_disk;
		int block_in_disk=dh->dcb->fcb.directory_block;
		FirstDirectoryBlock* tmp=(FirstDirectoryBlock*)malloc(sizeof(FirstDirectoryBlock));
		do_pcd(dh->sfs->disk, tmp, this);
		printf("%s", tmp->fcb.name);
		free(tmp);
	}
	
	//any other case
	else printf("%sCommand not found, please retry\n%s", RED, RESET);
	printf("\n");
	return 0;
}


void do_cmd_loop(DiskDriver* disk, SimpleFS* sfs){
	char cmd_line[MAX_LINE];
	char arg_buf[MAX_ARGS][MAX_LINE];
	unsigned arg_num;
	DirectoryHandle* dh=SimpleFS_init(sfs, disk);
	if(dh==NULL){
		SimpleFS_format(sfs);
		dh=SimpleFS_init(sfs, disk);
	}
	
	while(1){
		printf("%s%s%s", BLUE, PROMPT, RESET);
		if(fgets(cmd_line, MAX_LINE, stdin)==NULL) break;
		get_cmd_line(cmd_line, arg_buf, &arg_num);
		if(arg_num==0) continue;
		int res=do_cmd(dh, arg_buf, arg_num);
		if(res==-1) break;
	}
}



int main(int argc, char** argv){
	DiskDriver disk;
	SimpleFS sfs;
	//init disk
	DiskDriver_init(&disk, "disk.txt", 2048);
	printf("\t\t%s\tmyFS: a simple FileSystem\n%s", YELLOW, RESET);
	printf("'Help' or 'h' for help\n\n");
	do_cmd_loop(&disk, &sfs); //main program loop
	printf("%sQuitting...\n%s", YELLOW, RESET);
	return 0;
}
