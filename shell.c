#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "shell.h"

int cmp(char arg_buf[MAX_ARGS][MAX_LINE], char* s){
	return(!strcmp(arg_buf[0], s));
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
	printf("\t\t\t\t%sUsage:\n\t%stouch%s <file1> <file2> ... \t Creates empty files\n", YELLOW, RED, RESET);
	printf("\t%sls%s <dir> \t\t\t Reads a directory\n", RED, RESET);
	printf("\t%sopen%s <file> \t\t\t Opens a file\n", RED, RESET);
	printf("\t%srm%s <file> \t\t\t Removes a file\n", RED, RESET);
	printf("\t%scd%s <dir> \t\t\t Changes current directory\n", RED, RESET);
	printf("\t%smkdir%s <dir> \t\t\t Creates a new directory\n", RED, RESET);
	printf("\t%spcd%s \t\t\t\t Prints current directory\n", RED, RESET);
	printf("\n\t%shelp (h)%s \t\t\t Prints command info\n", RED, RESET);
	printf("\t%squit (q)%s \t\t\t Quits the program\n\n", RED, RESET);
}

//usage of file commands
void print_file_info(void){
	printf("\t\t\t%sUsage:\n\t%swrite%s <data> \t\t Writes %sdata%s on file\n", YELLOW, RED, RESET, GREEN, RESET);
	printf("\t%sread%s <size> \t\t Reads %ssize%s bytes on file\n", RED, RESET, GREEN, RESET);
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
	}
	
	//write
	else if(cmp(arg_buf, "write")){
		if(arg_num==2){
			int size=strlen(arg_buf[1]);
			char* data=(char*)malloc(sizeof(char)*size);
			sprintf(data, "%s", arg_buf[1]);
			SimpleFS_write(f, data, size);
		}
		else printf("%sUsage:%s write <data>\n", YELLOW, RESET);
	}
	
	
	//read
	else if(cmp(arg_buf, "read")){
		if(arg_num==2){
			int size=atoi(arg_buf[1]);
			char* data=(char*)malloc(sizeof(char)*size);
			data[size]='\0';
			SimpleFS_read(f, data, size);
			printf("%s", data);
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
			printf("%sClosing file%s %s'%s'%s...%s", YELLOW, RESET, RED, f->fcb->fcb.name, YELLOW, RESET);
		}
		else printf("%sUsage:%sopen <file>\n", YELLOW, RESET);
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
