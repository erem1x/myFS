#pragma once
#include "simplefs.h"
#include "disk_driver.h"

#define MAX_LINE 512
#define MAX_ARGS 16
#define PROMPT "myFS> "

//colors
#define COL(x) "\033[" #x ";1m"
#define GREEN COL(32)
#define BLUE COL(34)
#define YELLOW COL(33)
#define RED COL(31)
#define RESET "\033[0m"


//function to extract the cmd line args
//uses the input string, an array to store the tokens and a buf pointer 
//to store the number of tokens
void get_cmd_line(char cmd_line[MAX_LINE], char arg_buf[MAX_ARGS][MAX_LINE], unsigned* arg_num_ptr);


//prints infos about the commands
void print_cmd_info(void);


//prints info about the file commands
void print_file_info(void);


//do_pwd


//runs a command, returns 1 (valid) or -1(quit)
int do_cmd(DirectoryHandle* dh, char arg_buf[MAX_ARGS][MAX_LINE], unsigned arg_num);


//runs a file command, returns 1(valid) or -1(closes the file)
int do_file_cmd(FileHandle* f, char arg_buf[MAX_ARGS][MAX_LINE], unsigned arg_num);


//main loop (reading/running commands)
void do_cmd_loop(DiskDriver* disk, SimpleFS* sfs);


//file cmd loop
void do_file_loop(FileHandle* f);
