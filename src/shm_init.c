#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/types.h>
#include <errno.h>
#include "messaging.h"
#include "shm_init.h"
#include "sync.h"

#define IPC_KEY 0x1234ABCD

static int shm_id = -1;
static SharedMemory *shm_ptr = NULL;

/* Initialize shared memory and semaphores */
int init_shared_memory(void) {
    /* Initialize synchronization first */
    if (init_sync() == -1) {
        fprintf(stderr, "Failed to initialize synchronization\n");
        return -1;
    }

    /* Create/get shared memory */
    shm_id = shmget(IPC_KEY, SHM_SIZE, IPC_CREAT | 0666);
    if (shm_id == -1) {
        perror("shmget");
        return -1;
    }

    shm_ptr = (SharedMemory *)shmat(shm_id, NULL, 0);
    if (shm_ptr == (void *)-1) {
        perror("shmat");
        return -1;
    }

    /* Initialize shared memory on first process */
    lock_shared_memory();
    
    if (shm_ptr->num_dialogues == 0) {
        /* First time initialization */
        memset(shm_ptr, 0, sizeof(SharedMemory));
        shm_ptr->next_dialogue_id = 1;
    }
    
    unlock_shared_memory();
    return 0;
}

/* Cleanup shared memory and semaphores */
int cleanup_shared_memory(void) {
    if (shm_ptr != NULL) {
        shmdt(shm_ptr);
        shm_ptr = NULL;
    }

    if (shm_id != -1) {
        shmctl(shm_id, IPC_RMID, NULL);
        shm_id = -1;
    }

    cleanup_sync();
    return 0;
}

/* Get pointer to shared memory */
SharedMemory *get_shared_memory(void) {
    return shm_ptr;
}
