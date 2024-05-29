#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <syslog.h>
#include <errno.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>


#define PORT        "9000"      //In parenthases because that is what getaddrinfo expects

// #define LOG_CONSOLE
#define BUFFER_SIZE 1024
#define RECEIVE_FLAGS 0

#define PATH_TO_FILE "/var/tmp/aesdsocketdata" 
#define WRITE_MODE  "a"
#define READ_MODE   "r"


#ifdef LOG_CONSOLE          //Logs exclusively to the console
#define DEBUG_LOG(msg,...) printf(msg "\n", ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf(" ERROR: " msg "\n" , ##__VA_ARGS__)
#else                       //Will send the logs to the logs folder
#define DEBUG_LOG(msg,...) syslog(LOG_DEBUG , msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) syslog(LOG_ERR , " ERROR: " msg "\n" , ##__VA_ARGS__)
#endif


//This function will do nothing if LOG_CONSOLE is defined, otherwise it will simply close the logs 
void associateIP(struct sockaddr* sock_addr, char* out_string);
void close_log();
void exitfunction();