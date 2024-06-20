SHELL:= /bin/sh
CC := $(CROSS_COMPILE)gcc

#When I type make all, it will run the steps to compile writer
all: aesdsocket


#The $(var_name) essentially says hey, if CROSS_COMPILE is define when calling make, put it there
#-g specifies we want debugging information
#-Wall specifies we want all the warning messages that GCC can produce
#-c specifies that we want to generate a .o file
#-o specifies the name of the output file
# -I/ specifies the directories to be searched
#-pthread specifies we want to link the pthread library with our program. Ensures code is thread safe and there are no race conditions

#This line is telling us that how to build writer. But  in order to do that it needs writer.o (which is generated below)
writer: writer.o
	$(CC) -g -Wall -I/ aesdsocket.o -o aesdsocket -lpthread

#This line is telling us that how to build writer.o. But  in order to do that it needs writer.c (which we have written)
aesdsocket.o: aesdsocket.c
	$(CC) -g -Wall -c -o aesdsocket.o aesdsocket.c -lpthread

# -f specifies that we want to run the remove command without asking for confirmation from the user
# This command is removing any .o files, .elf files, .map files, and the writer file
clean:
	rm -f *.o *.elf *.map aesdsocket
	
