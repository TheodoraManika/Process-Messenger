#include <sys/sem.h>
#include <sys/ipc.h>
#include <unistd.h>
#include "sync.h"

#define SEM_KEY 0x5678DCBA

static int sem_id = -1;

/* Semaphore operations */
struct sembuf sem_lock = {0, -1, 0};    /* Lock */
struct sembuf sem_unlock = {0, 1, 0};   /* Unlock */

/* Initialize synchronization */
int init_sync(void) {
    /* Create/get semaphore */
    sem_id = semget(SEM_KEY, 1, IPC_CREAT | 0666);
    if (sem_id == -1) {
        return -1;
    }
    return 0;
}

/* Lock shared memory */
void lock_shared_memory(void) {
    if (sem_id != -1) {
        semop(sem_id, &sem_lock, 1);
    }
}

/* Unlock shared memory */
void unlock_shared_memory(void) {
    if (sem_id != -1) {
        semop(sem_id, &sem_unlock, 1);
    }
}

/* Cleanup synchronization */
int cleanup_sync(void) {
    if (sem_id != -1) {
        semctl(sem_id, 0, IPC_RMID, 0);
        sem_id = -1;
    }
    return 0;
}
