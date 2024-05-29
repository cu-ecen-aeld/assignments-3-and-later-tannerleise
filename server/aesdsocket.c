//Article linked here for future reference: https://beej.us/guide/bgnet/html/#a-simple-stream-server
#include "aesdsocket.h"


static void signal_handler(int signo);
bool signal_caught = false;

int main(int argc, char* argv[]){
    //Signal Declarations and setups
    struct sigaction new_action;                        //Naming convention is because this is replacing the "old action"
    memset(&new_action, 0, sizeof(struct sigaction));   //Set struct to all zeros
    new_action.sa_handler = signal_handler;             //Point the new action to our signal handler function

    if((sigaction(SIGTERM, &new_action, NULL)) != 0){
        ERROR_LOG("Registering SIGTERM Error:%s\n", strerror(errno));
        return -1;
    }
    if((sigaction(SIGINT, &new_action, NULL)) != 0){
        ERROR_LOG("Registering SIGINT Error:%s\n", strerror(errno));
        return -1;
    }


    #ifndef LOG_CONSOLE
    openlog("tl-assignment5-socket", LOG_CONS, LOG_USER);
    #endif


    //Status and socket defines
    int status;
    int listen_sockfd;
    int sendrec_sockfd;

    //Socket addr info structs (used for most of the socket initialization and binding)
    struct addrinfo hints;  //This guy will give getaddrinfo() some "hints" as to how to fill out the servinfo struct
    struct addrinfo *servinfo;

    //Info for the device connecting to our host (used to accept, send, and receive)
    struct sockaddr_storage client_addr;     //sockaddr_storage can be easily cast to addrinfo. nicer struct to work with (has info broken out)
    socklen_t client_addr_size;
    char client_ip[INET6_ADDRSTRLEN];

    //Sending and receiving data
    char receive_buffer[BUFFER_SIZE];
    int bytes_received, bytes_sent;
    FILE *fp;


    //Prep work for socket-----------------------------------------------------------------------------
    memset(&hints, 0, sizeof(struct addrinfo));    //Initialize all fields to 0 in hints
    hints.ai_family = AF_UNSPEC;                    //we can get addresses for either IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;               //We want to use a stream tather than a datagram
    hints.ai_flags = AI_PASSIVE;                    //We want the function to fill our address for us (allows us to use NULL with func node argument)

    status = getaddrinfo(NULL, PORT, &hints, &servinfo);

    //error handling
    if(status != 0){
        ERROR_LOG("getaddrinfo error:%s\n", gai_strerror(status));
        freeaddrinfo(servinfo);
        close_log();
        return -1;
    }

    //socket stuff-----------------------------------------------------------------------------
    listen_sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

    //error handling
    if(listen_sockfd == -1){
        ERROR_LOG("socket error:%s\n", strerror(errno));
        freeaddrinfo(servinfo);
        close_log();
        return -1;
    }

    //Dealing with bind() failing after rerunning a server
    if (setsockopt(listen_sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) == -1) {
        ERROR_LOG("set socket option error:%s\n", strerror(errno));
        freeaddrinfo(servinfo);
        close(listen_sockfd);
        close_log();
        return -1;
    }

    //binding the socket-----------------------------------------------------------------------------
    status = bind(listen_sockfd, servinfo->ai_addr, servinfo->ai_addrlen);

    //error handling
    if(status != 0){
        ERROR_LOG("binding error:%s\n", strerror(errno));
        freeaddrinfo(servinfo);
        close(listen_sockfd);
        close_log();
        return -1;
    }

    //Free up servinfo, no longer needed now that the socket is bound
    freeaddrinfo(servinfo);

    //listening for connections-----------------------------------------------------------------------------
    status = listen(listen_sockfd, 20);
    if(status != 0){
        ERROR_LOG("listening error:%s\n", strerror(errno));
        close(listen_sockfd);
        close_log();
        return -1;
    }



    while(1){
        if(signal_caught){
            break;
        }
    //accepting incoming connections------------------------------------------------------------------------
        client_addr_size = sizeof(client_addr);
        sendrec_sockfd = accept(listen_sockfd, (struct sockaddr *) &client_addr, &client_addr_size);    //Accept makes a new socket_fd so the other one can continue to listen
        if(status == -1){
            ERROR_LOG("acception error:%s\n", strerror(errno));
            close(listen_sockfd);
            close_log();
            continue;
        }

        //marking IP
        associateIP((struct sockaddr *) &client_addr, client_ip);
        DEBUG_LOG("Accepted connection from %s\n", client_ip);

        //Open up our file to stream the data in
        if((fp = fopen(PATH_TO_FILE, WRITE_MODE)) == NULL){
            ERROR_LOG("Opening File Error:%s\n", strerror(errno));
            close(listen_sockfd);
            close(sendrec_sockfd);
            close_log();
            return -1;
        }

    //Sending and Receiving data------------------------------------------------------------------------
        while(1){
            if(signal_caught){
            break;
            }
            bytes_received = recv(sendrec_sockfd, receive_buffer, BUFFER_SIZE - 1, RECEIVE_FLAGS);
            if (bytes_received == -1) {
                ERROR_LOG("Receive error:%s\n", strerror(errno));
                break;
            }
            if(bytes_received == 0){        //Client has closed the connection!
                break;
            }

            //Write data to the file
            fwrite(receive_buffer, 1, bytes_received, fp);
            if(memchr(receive_buffer, '\n', bytes_received) != NULL){   //New line detected, we are finished with the packet
                break;
            }
        }
        fclose(fp);

        //Writing the data back to the client-----------------------------------------------------------
        if((fp = fopen(PATH_TO_FILE, READ_MODE)) == NULL){
            ERROR_LOG("Opening File to Read Error:%s\n", strerror(errno));
            close(listen_sockfd);
            close(sendrec_sockfd);
            close_log();
            return -1;
        }

        while(1){
            if(signal_caught){
            break;
        }
            bytes_received = fread(receive_buffer, 1, BUFFER_SIZE - 1, fp);
            if(bytes_received == 0){        //End of File
                break;
            }
            bytes_sent = send(sendrec_sockfd, receive_buffer, bytes_received, RECEIVE_FLAGS);
            if(bytes_sent == -1){
                ERROR_LOG("Sending Error:%s\n", strerror(errno));
                close(listen_sockfd);
                close(sendrec_sockfd);
                close_log();
                return -1;
            }
        }
    }
    DEBUG_LOG("Caught Signal, exiting");
    fclose(fp);
    close(listen_sockfd);     //close the listener socket
    close(sendrec_sockfd);     //close the readwrite socket
    close_log();
    remove(PATH_TO_FILE);
    return 0;
}

//---------------------------------------------------------------------------------------------------------------------------------
static void signal_handler(int signo){
    if(signo == SIGINT || signo == SIGTERM){
        signal_caught = true;
    }
}

//---------------------------------------------------------------------------------------------------------------------------------
//Since we have no way of knowing which IP we will have, this function will find it and get it presentable for us
void associateIP(struct sockaddr* sock_addr, char* out_string){
    if(sock_addr->sa_family == AF_INET){
        inet_ntop(AF_INET, (struct in_addr*) &sock_addr, out_string, INET_ADDRSTRLEN);
    }
    else if(sock_addr->sa_family == AF_INET6){
        inet_ntop(AF_INET6,(struct in6_addr*) &sock_addr, out_string, INET6_ADDRSTRLEN);
    }
    else{
        ERROR_LOG("IP Conversion error:%s\n", strerror(errno));
    }
}
//---------------------------------------------------------------------------------------------------------------------------------
void close_log(){
    #ifndef LOG_CONSOLE
    closelog();
    #else
        return;
    #endif
}
//---------------------------------------------------------------------------------------------------------------------------------