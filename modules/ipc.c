#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "../include/ipc.h"

SharedMemory* connect_to_shm() {
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open failed");
        exit(1);
    }

    // Ορισμός μεγέθους μνήμης
    struct stat mapstat;
    if (fstat(shm_fd, &mapstat) != -1 && mapstat.st_size == 0) {
        if (ftruncate(shm_fd, sizeof(SharedMemory)) == -1) {
            perror("ftruncate failed");
            exit(1);
        }
    }

    SharedMemory* shm = mmap(NULL, sizeof(SharedMemory), 
                             PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED) {
        perror("mmap failed");
        exit(1);
    }
    
    return shm;
}

void detach_shm(SharedMemory* shm) {
    munmap(shm, sizeof(SharedMemory));
}

void destroy_shm() {
    shm_unlink(SHM_NAME);
}

int find_free_dialogue_slot(SharedMemory* shm) {
    for (int i = 0; i < MAX_DIALOGUES; i++) {
        if (!shm->dialogues[i].active) {
            return i;
        }
    }
    return -1;
}

int find_dialogue_index(SharedMemory* shm, int dialogue_id) {
    for (int i = 0; i < MAX_DIALOGUES; i++) {
        if (shm->dialogues[i].active && shm->dialogues[i].id == dialogue_id) {
            return i;
        }
    }
    return -1;
}