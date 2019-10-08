CCOPTS=-g -std=gnu99 -Wstrict-prototypes
LIBS =
CC = gcc
AR = ar

HEADERS= bitmap.h\
				disk_driver.h\
				simplefs.h\
				shell.h
				
shell: shell.c bitmap.c disk_driver.c simplefs.c $(HEADERS)
		$(CC) $(CCOPTS) shell.c bitmap.c disk_driver.c simplefs.c -o shell
		
.phony: clean

clean: 
		rm -f *.o shell disk.txt test2.txt
