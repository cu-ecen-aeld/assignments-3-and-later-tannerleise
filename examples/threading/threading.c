#include "threading.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>

// Optional: use these functions to add debug or error prints to your application
#define DEBUG_LOG(msg,...)
//#define DEBUG_LOG(msg,...) printf("threading: " msg "\n" , ##__VA_ARGS__)
#define ERROR_LOG(msg,...) printf("threading ERROR: " msg "\n" , ##__VA_ARGS__)

void* threadfunc(void* thread_param)
{

    // TODO: wait, obtain mutex, wait, release mutex as described by thread_data structure
    // hint: use a cast like the one below to obtain thread arguments from your parameter
    //struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    struct thread_data* thread_func_args = (struct thread_data *) thread_param;
    int wait_to_obatain_us = thread_func_args->wait_to_obtain_ms * 1000;
    int wait_to_release_us = thread_func_args->wait_to_release_ms * 1000;
    
    //wait
    usleep(wait_to_obatain_us);
    //Grab the mutex
    int mutex_ret = pthread_mutex_lock(thread_func_args->mutex); 
    if(mutex_ret != 0){
	    //error case unable to grab the mutex
	    ERROR_LOG("We were unable to grab the mutex for locking");
    }
    else{
    	    //We good
	    DEBUG_LOG("Mutex grabbed, waiting for specified time");
	    usleep(wait_to_release_us);
	    DEBUG_LOG("Unlocking mutex");
	    
	    mutex_ret = pthread_mutex_unlock(thread_func_args->mutex);
	    if(mutex_ret != 0){
	    	//error case unable to grab the mutex
	    	ERROR_LOG("We were unable to grab the mutex for unlocking");
	    	thread_func_args->thread_complete_success = false;
	    }
	    else{
	    //We good
	    DEBUG_LOG("Mutex unlocked, success!");
	    thread_func_args->thread_complete_success = true;
	    } 
    }
    return thread_param;
}


bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex,int wait_to_obtain_ms, int wait_to_release_ms)
{
    /**
     * TODO: allocate memory for thread_data, setup mutex and wait arguments, pass thread_data to created thread
     * using threadfunc() as entry point.
     *
     * return true if successful.
     *
     * See implementation details in threading.h file comment block
     */
     //Allocating memory
     struct thread_data *thread_args = (struct thread_data *) malloc(sizeof(struct thread_data));
     //Setup struct
     thread_args->mutex = mutex;
     thread_args->wait_to_obtain_ms = wait_to_obtain_ms;
     thread_args->wait_to_release_ms = wait_to_release_ms;
     
     //create our thread
     int thread_ret = pthread_create(thread, NULL, threadfunc, thread_args);
     
     if(thread_ret != 0){
     ERROR_LOG("The thread exited with the error code: %d\n", thread_ret);
     return false;
     }
     DEBUG_LOG("Thread was created successfully!1");
     return true;
}

