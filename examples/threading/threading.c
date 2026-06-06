#include "threading.h"
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

void* threadfunc(void* thread_param)
{
    struct thread_data *thread_func_args = (struct thread_data *) thread_param;

    thread_func_args->thread_complete_success = false;

    usleep(thread_func_args->wait_to_obtain_ms * 1000);

    if (pthread_mutex_lock(thread_func_args->mutex) != 0) {
        return thread_param;
    }

    usleep(thread_func_args->wait_to_release_ms * 1000);

    if (pthread_mutex_unlock(thread_func_args->mutex) != 0) {
        return thread_param;
    }

    thread_func_args->thread_complete_success = true;

    return thread_param;
}

bool start_thread_obtaining_mutex(pthread_t *thread, pthread_mutex_t *mutex, int wait_to_obtain_ms, int wait_to_release_ms)
{
    struct thread_data *thread_func_args = malloc(sizeof(struct thread_data));

    if (thread_func_args == NULL) {
        return false;
    }

    thread_func_args->mutex = mutex;
    thread_func_args->wait_to_obtain_ms = wait_to_obtain_ms;
    thread_func_args->wait_to_release_ms = wait_to_release_ms;
    thread_func_args->thread_complete_success = false;

    if (pthread_create(thread, NULL, threadfunc, thread_func_args) != 0) {
        free(thread_func_args);
        return false;
    }

    return true;
}
