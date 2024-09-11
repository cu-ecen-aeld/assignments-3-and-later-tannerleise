//---------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//Article linked here for future reference: https://beej.us/guide/bgnet/html/#a-simple-stream-server
#include "aesdsocket.h"



int setup_signal_handlers();
int setup_listening_socket(int daemon_mode, struct addrinfo **servinfo);


SLIST_HEAD(thread_list_t, slist_data) head = SLIST_HEAD_INITIALIZER(head);
struct slist_data *ll_thread_data = NULL;
struct slist_data *temp = NULL;

static void signal_handler(int signo);
bool signal_caught = false;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
FILE *fp = NULL;


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
    
    if(setup_signal_handlers() == -1){
      return -1;
    }

    #ifndef LOG_CONSOLE
    openlog("tl-assignment6-socket", LOG_CONS, LOG_USER);
    #endif

    socklen_t client_addr_size;

    struct addrinfo *servinfo;
    int listen_sockfd = setup_listening_socket(daemon_mode, &servinfo);

    if(listen_sockfd == -1){
        freeaddrinfo(servinfo);
        close_log();
        return -1;
    }

    fp = fopen(PATH_TO_FILE, "a+");
    if(fp == NULL){
        ERROR_LOG("Opening File Error:%s\n", strerror(errno));
        return -1;
    }
    
    //Initialize our timer
    struct sigevent sev;
    timer_t timerid;
    memset(&sev, 0, sizeof(sev));
    sev.sigev_notify = SIGEV_THREAD;
    sev.sigev_notify_function = &TimerthreadRoutine;
    if(timer_create(CLOCK_MONOTONIC, &sev, &timerid) == -1) {
        syslog(LOG_ERR, "Cannot register for timer signal handler");
        exit(-1);
    }

    // Start our timer
    struct itimerspec its;
    its.it_value.tv_sec = 10;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = its.it_value.tv_sec;
    its.it_interval.tv_nsec = its.it_value.tv_nsec;
    if (timer_settime(timerid, 0, &its, NULL) == -1) {
        syslog(LOG_ERR, "Could not start timer.");
        exit(-1);
    }

    int uid = 0;
    while(!signal_caught){
    //accepting incoming connections------------------------------------------------------------------------
        pthread_t thread;
        struct thread_data *thread_args = malloc(sizeof(struct thread_data)); //Allocate Space for our arguments


        client_addr_size = sizeof(thread_args->addr);
        thread_args->sockfd = accept(listen_sockfd, (struct sockaddr *) &thread_args->addr, &client_addr_size);    //Accept makes a new socket_fd so the other one can continue to listen
        if(thread_args->sockfd == -1){
            if(signal_caught){
                continue;       //If a kill signal was caught, theres no error, we are just closing out the function
            }
            ERROR_LOG("acception error:%s\n", strerror(errno));
            close(listen_sockfd);
            close_log();
            continue;
        }


        //Hey we have accepted a connection, lets make our thread now!
        thread_args->mutex = &mutex;
        thread_args->thread_complete = false;
        thread_args->uid = uid;
        uid++;
        pthread_create(&thread, NULL, DataTransferthreadRoutine, (void *) thread_args);

        //Add the thread to our linked list
        ll_thread_data = malloc(sizeof(struct slist_data));
        ll_thread_data->thread = thread;
        ll_thread_data->tdata = thread_args;

        SLIST_INSERT_HEAD(&head, ll_thread_data, entries);

        struct slist_data *t1, *t2;
        t1 = SLIST_FIRST(&head);
        while(t1 != NULL) {
            t2 = t1;
            t1 = SLIST_NEXT(t1, entries);

            if(t2->tdata->thread_complete) {
                close(t2->tdata->sockfd);
                pthread_join(t2->thread, NULL);
                SLIST_REMOVE(&head, t2, slist_data, entries);
                free(t2->tdata);
                free(t2);
            }
        }
    }
    DEBUG_LOG("Caught Signal, exiting");
    close(listen_sockfd);      //close the listener socket
    close_log();
    fclose(fp);
    remove(PATH_TO_FILE);
    pthread_mutex_destroy(&mutex);
    exitfunction();
    exit(0);
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
//This is the write and receive logic for the socket server
void* DataTransferthreadRoutine(void *thread_param){
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    char client_ip[INET6_ADDRSTRLEN];

    //marking IP
    associateIP((struct sockaddr *) &thread_func_args->addr, client_ip);
    DEBUG_LOG("Accepted connection from %s\n", client_ip);

    //Sending and receiving data
    char receive_buffer[BUFFER_SIZE];
    int bytes_received, bytes_sent;

    //Open up our file to stream the data in
    pthread_mutex_lock(thread_func_args->mutex);
    //Sending and Receiving data------------------------------------------------------------------------
        while(!signal_caught){
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
        fflush(fp);
        rewind(fp);
        clearerr(fp);
        while(!signal_caught){
            bytes_received = fread(receive_buffer, 1, BUFFER_SIZE, fp);
            if(bytes_received == 0){        //End of File
                break;
            }
            bytes_sent = send(thread_func_args->sockfd, receive_buffer, bytes_received, RECEIVE_FLAGS);
            if(bytes_sent == -1){
                ERROR_LOG("Sending Error:%s\n", strerror(errno));
                close(thread_func_args->sockfd);
                close_log();
                pthread_mutex_unlock(thread_func_args->mutex);
                return thread_param;
            }
        }
        fflush(fp);
        pthread_mutex_unlock(thread_func_args->mutex);
        thread_func_args->thread_complete = true;
        pthread_exit(NULL);
        return thread_param;
}

//---------------------------------------------------------------------------------------------------------------------------------
//When we create our timer, this is the routine it will go through to post timestamps
static void TimerthreadRoutine(void *thread_param){
    if(pthread_mutex_lock(&mutex) != 0) {
        syslog(LOG_ERR, "Mutex lock failed");
        return;
    }

    char outstr[256] = "timestamp:";
    size_t outstr_target_len = sizeof(outstr) - strlen(outstr);
    char* outstr_target = outstr + strlen(outstr);

    time_t t;
    struct tm *tmp;

    t = time(NULL);
    tmp = localtime(&t);
    if(tmp == NULL) {
        syslog(LOG_ERR, "Getting time failed");
    } else if (strftime(outstr_target, outstr_target_len, "%a, %d %b %Y %T %z\n", tmp) == 0) {
        syslog(LOG_ERR, "Getting time failed, strftime");
    } else if(fputs(outstr, fp) < 0) {
        syslog(LOG_ERR, "Getting time failed, strftime");
    }

    fflush(fp);
    
    if(pthread_mutex_unlock(&mutex)) {
        syslog(LOG_ERR, "Mutex unlock failed");
    }
    return;
}
//---------------------------------------------------------------------------------------------------------------------------------
//This will handle closing out all the threads that we have
//Issue here is how do we access the head? Its declared outside of this scope and I have no idea what it's type is
void exitfunction(){
        struct slist_data *t1, *t2;
        t1 = SLIST_FIRST(&head);
        while(t1 != NULL) {
            t2 = t1;
            t1 = SLIST_NEXT(t1, entries);

            close(t2->tdata->sockfd);
            pthread_join(t2->thread, NULL);
            SLIST_REMOVE(&head, t2, slist_data, entries);
            free(t2->tdata);
            free(t2);
            }
}




int setup_signal_handlers(){
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
    return 0;
}


int setup_listening_socket(int daemon_mode, struct addrinfo **servinfo){
    //socket stuff-----------------------------------------------------------------------------
    int listen_sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if(listen_sockfd == -1){
        ERROR_LOG("socket error:%s\n", strerror(errno));
        return -1;
    }
    //Status and socket defines
    int status;

    //Socket addr info structs (used for most of the socket initialization and binding)
    struct addrinfo hints;  //This guy will give getaddrinfo() some "hints" as to how to fill out the servinfo struct
    //Prep work for socket-----------------------------------------------------------------------------
    memset(&hints, 0, sizeof(hints));    //Initialize all fields to 0 in hints
    hints.ai_family = AF_INET;                    //we can get addresses for either IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;               //We want to use a stream rather than a datagram
    hints.ai_flags = AI_PASSIVE;                    //We want the function to fill our address for us (allows us to use NULL with func node argument)

    status = getaddrinfo(NULL, PORT, &hints, servinfo);
    if(status != 0){
        ERROR_LOG("getaddrinfo error:%s\n", gai_strerror(status));
        return -1;
    }

    //Dealing with bind() failing after rerunning a server
    int enable = 1;
    if (setsockopt(listen_sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0) {
        ERROR_LOG("set socket option error:%s\n", strerror(errno));
        close(listen_sockfd);
        return -1;
    }

    //binding the socket-----------------------------------------------------------------------------
    status = bind(listen_sockfd, (*servinfo)->ai_addr, (*servinfo)->ai_addrlen);
    if(status != 0){
        ERROR_LOG("binding error:%s\n", strerror(errno));
        close(listen_sockfd);
        return -1;
    }

    //Forking according to assignment instructions if -d was specified
    if(daemon_mode){
            pid_t pid = fork();

            if(pid < 0)
            {
                printf("failed to fork\n"); 
                close(listen_sockfd);
                closelog();  
                exit(EXIT_FAILURE);      
            }   

            if(pid > 0 )
            {
                printf("Running as a deamon\n");
                exit(EXIT_SUCCESS);
            }

            umask(0);

            if(setsid() < 0)
            {
                printf("Failed to create SID for child\n");
                close(listen_sockfd);
                closelog(); 
                exit(EXIT_FAILURE);
            }

            if(chdir("/") < 0)
            {
                printf("Unable to change directory to root\n");
                close(listen_sockfd);
                closelog(); 
                exit(EXIT_FAILURE);
            }

            close(STDIN_FILENO);
            close(STDOUT_FILENO);
            close(STDERR_FILENO); 
            // signal(SIGALRM, signal_handler);
            // alarm(10);
    }

    //listening for connections-----------------------------------------------------------------------------
    status = listen(listen_sockfd, 20);
    if( status != 0){
        ERROR_LOG("listening error:%s\n", strerror(errno));
        close(listen_sockfd);
        close_log();
        return -1;
    }

    if(pthread_mutex_init(&mutex, NULL) != 0) {
        syslog(LOG_ERR, "Cannot create mutex");
        exit(-1);
    }

    //Free up servinfo, no longer needed now that the socket is bound
    freeaddrinfo(*servinfo);

    return listen_sockfd;
}





