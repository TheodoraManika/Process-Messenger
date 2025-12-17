#include <pthread.h>
#include <semaphore.h>
#include "../include/defs.h"

SharedMemory* shm = NULL;

#ifdef __APPLE__
    sem_t *mutex = NULL;
#endif

int my_dialogue_id = -1;
int dialogue_index = -1;
int running = 1;
int in_dialogue = 0;

// Αρχικοποίηση του mutex για την οθόνη (απαραίτητο για ncurses + threads)
pthread_mutex_t screen_mutex = PTHREAD_MUTEX_INITIALIZER;