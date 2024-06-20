//Article linked here for future reference: https://beej.us/guide/bgnet/html/#a-simple-stream-server
#include "aesdsocket.h"
#include <pthread.h>

/*#TODO:
- Complete the exit function
- Make usre there are no apparent locations of memory leaks. IE make sure that we are closing all of our connections properly and what not

*/
SLIST_HEAD(slisthead, slist_data) head;
struct slist_data *ll_thread_data = NULL;
struct slist_data *temp = NULL;

static void signal_handler(int signo);
bool signal_caught = false;
pthread_mutex_t mutex;


int main(int argc, char* argv[]){

    //Checking -d argument for running as daemon 
    bool daemon_mode = false;
    if(argc > 1 && strcmp(argv[1],"-d") == 0){
        DEBUG_LOG("Starting server in Daemon mode");
        daemon_mode = true;
    }
    else{
        DEBUG_LOG("Starting server in Normal mode");
    }


    //Initialize the linked list
    SLIST_INIT(&head);


    //Initialize our timer
    pthread_t timer_thread;
    struct thread_data *timer_thread_args = (struct thread_data *) malloc(sizeof(struct thread_data)); //Allocate Space for our arguments
    timer_thread_args->mutex = &mutex;
    timer_thread_args->sockfd = 0;
    timer_thread_args->thread_complete = false;
    pthread_create(&timer_thread, NULL, TimerthreadRoutine, (void *) timer_thread_args);    //Starts the timer for us

    //Lets add our timer thread to the linked list to keep track of him
    ll_thread_data = malloc(sizeof(ll_thread_data));
    ll_thread_data->thread = timer_thread;
    ll_thread_data->tdata = timer_thread_args;      //May be an issue here

    SLIST_INSERT_HEAD(&head, ll_thread_data, entries);



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


    //Prep work for socket-----------------------------------------------------------------------------
    memset(&hints, 0, sizeof(struct addrinfo));    //Initialize all fields to 0 in hints
    hints.ai_family = AF_UNSPEC;                    //we can get addresses for either IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;               //We want to use a stream rather than a datagram
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

    //Forking according to assignment instructions if -d was specified
    if(daemon_mode){
        pid_t pid;
        pid = fork();
        //Child process
        if(!pid){
            //Let the child process run, no further input needed
    	}
    //Parent Process
    else if(pid > 0){
        //Tidy up and close down the parent, let the child go
        freeaddrinfo(servinfo);
        close(listen_sockfd);
        close_log();
        return 0;
    }
    //Any number that is negative, indicating an error
    else{
    	ERROR_LOG("fork error: failed to fork:%s\n", strerror(errno));
        freeaddrinfo(servinfo);
        close(listen_sockfd);
        close_log();
        return -1;

    }
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
        //Before we accept any connections, let's see if any of our threads have completed
        //------------------------------------------------------------------------------------
        ll_thread_data = SLIST_FIRST(&head);
        while(ll_thread_data != NULL){
            if(ll_thread_data->tdata->thread_complete){
                SLIST_REMOVE(&head, ll_thread_data, slist_data, entries);
                pthread_join(ll_thread_data->thread, NULL);      //May need to switch to using a pointer for that data point, idk if it will join the right thread or not
                temp = ll_thread_data;
                ll_thread_data = SLIST_NEXT(ll_thread_data, entries);
                free(temp);
            }
            else{
                ll_thread_data = SLIST_NEXT(ll_thread_data, entries);
            }
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

        //Hey we have accepted a connection, lets make our thread now!
        pthread_t thread;
        struct thread_data *thread_args = (struct thread_data *) malloc(sizeof(struct thread_data)); //Allocate Space for our arguments
        thread_args->mutex = &mutex;
        thread_args->sockfd = sendrec_sockfd;
        thread_args->thread_complete = false;
        pthread_create(&thread, NULL, DataTransferthreadRoutine, (void *) thread_args);

        //Add the thread to our linked list
        ll_thread_data = malloc(sizeof(ll_thread_data));
        ll_thread_data->thread = thread;
        ll_thread_data->tdata = thread_args;      //May be an issue here

        SLIST_INSERT_HEAD(&head, ll_thread_data, entries);
    }
    DEBUG_LOG("Caught Signal, exiting");
    // fclose(fp);
    close(listen_sockfd);      //close the listener socket
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
//This is essentially the routine the thread will run through on its creation
//This is the write and creceive logic for the socket server
void* DataTransferthreadRoutine(void *thread_param){
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    FILE *fp;
    //Sending and receiving data
    char receive_buffer[BUFFER_SIZE];
    int bytes_received, bytes_sent;


    //Open up our file to stream the data in
    pthread_mutex_lock(thread_func_args->mutex);
    if((fp = fopen(PATH_TO_FILE, WRITE_MODE)) == NULL){
        ERROR_LOG("Opening File Error:%s\n", strerror(errno));
        close(thread_func_args->sockfd);
        close_log();
        return;
    }

    //Sending and Receiving data------------------------------------------------------------------------
        while(1){
            if(signal_caught){
            break;
            }
            bytes_received = recv(thread_func_args->sockfd, receive_buffer, BUFFER_SIZE - 1, RECEIVE_FLAGS);
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


        //Writing the data back to the client-----------------------------------------------------------
        if((fp = fopen(PATH_TO_FILE, READ_MODE)) == NULL){
            ERROR_LOG("Opening File to Read Error:%s\n", strerror(errno));
            close(thread_func_args->sockfd);
            close_log();
            pthread_mutex_unlock(thread_func_args->mutex);
            return;
        }

        while(1){
            if(signal_caught){
            break;
        }
            bytes_received = fread(receive_buffer, 1, BUFFER_SIZE - 1, fp);
            if(bytes_received == 0){        //End of File
                break;
            }
            bytes_sent = send(thread_func_args->sockfd, receive_buffer, bytes_received, RECEIVE_FLAGS);
            if(bytes_sent == -1){
                ERROR_LOG("Sending Error:%s\n", strerror(errno));
                close(thread_func_args->sockfd);
                close_log();
                pthread_mutex_unlock(thread_func_args->mutex);
                return -1;
            }
        }
        fclose(fp);
        pthread_mutex_unlock(thread_func_args->mutex);
        thread_func_args->thread_complete = true;
        return thread_param;
}
//---------------------------------------------------------------------------------------------------------------------------------
//When we create our timer, this is the routine it will go through to post timestamps
void* TimerthreadRoutine(void *thread_param){
    //Time is givien in seconds since last epoch
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    time_t start_time = time(NULL);
    time_t check_time = time(NULL);
    struct tm *local_time;
    char write_buffer[BUFFER_SIZE];
    FILE *fp;
    while(1){
        if(signal_caught){
        break;
        }
        //Check the time
        check_time = time(NULL);
        //See if it has been 10 seconds yet or not
        if(difftime(check_time,start_time) < TIME_INTERVAL){
            //if not, lets go to the next iteration of our loop
            continue;
        }
        //10 seconds has elapsed
        start_time = check_time;    //set our time for next iteration

        local_time = localtime(&check_time); //Get it in local time format
        strftime(write_buffer, BUFFER_SIZE, "timestamp: %a, %d %b %Y %T %z\n", local_time); //Fill a buffer with that time info
        pthread_mutex_lock(thread_func_args->mutex);

        if((fp = fopen(PATH_TO_FILE, WRITE_MODE)) == NULL){
        ERROR_LOG("Opening File Error:%s\n", strerror(errno));
        pthread_mutex_unlock(thread_func_args->mutex);
        close_log();
        return;
        }
        fwrite(write_buffer, 1, strlen(write_buffer), fp);
        fclose(fp);
        pthread_mutex_unlock(thread_func_args->mutex);
    }

}
//---------------------------------------------------------------------------------------------------------------------------------
//This will handle closing out all the threads that we have
//Issue here is how do we access the head? Its declared outside of this scope and I have no idea what it's type is
void exitfunction(){
    struct slist_data *datap = NULL;
    while (!SLIST_EMPTY(&head)) {
        datap = SLIST_FIRST(&head);
        SLIST_REMOVE_HEAD(&head, entries);
        free(datap);
    }
}