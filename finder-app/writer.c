#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>

void UserSysLog(int priority, char* error_message);

int main(int argc, char* argv[]){
	FILE* fptr;
	char error_message[128];

	if(argc != 3){
	//Sprintf will store our message in error_message for syslog. Then printf will print to console
		sprintf(error_message, "User entered incorrect argument count of %d\n", argc);
		printf("%s", error_message);
		UserSysLog(LOG_ERR, error_message);
		return 1;
	}
	//Open the file at the given directory in write mode
	fptr = fopen(argv[1], "w");
	if(fptr == NULL){
		sprintf(error_message, "Incorrect Path, Provided Path: %s\n", argv[1]);
		printf("%s", error_message);
		UserSysLog(LOG_ERR, error_message);
		return 1;
	}
	sprintf(error_message, "Writing %s to %s\n", argv[2], argv[1]);
	//In this case not an error message, bad naming convention 
	fprintf(fptr, "%s", error_message);
	fclose(fptr);
	UserSysLog(LOG_DEBUG, error_message);
	return 0;
}

void UserSysLog(int priority, char* error_message){
	openlog("tl-assignment2-writer", LOG_CONS, LOG_USER);
	syslog(LOG_USER | priority,"%s", error_message);
	closelog();
	return;
}
