#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "messaging.h"
#include "shm_init.h"
#include "dialogue.h"
#include "sync.h"

/* Find dialogue by ID */
int find_dialogue(int dialogue_id) {
    SharedMemory *shm_ptr = get_shared_memory();
    if (shm_ptr == NULL) return -1;

    for (int i = 0; i < shm_ptr->num_dialogues; i++) {
        if (shm_ptr->dialogues[i].dialogue_id == dialogue_id &&
            shm_ptr->dialogues[i].active) {
            return i;
        }
    }
    return -1;
}

/* Add participant to a dialogue */
int add_participant_to_dialogue(int dialogue_idx, pid_t pid) {
    SharedMemory *shm_ptr = get_shared_memory();
    if (shm_ptr == NULL) return -1;
    
    if (dialogue_idx < 0 || dialogue_idx >= shm_ptr->num_dialogues) {
        return -1;
    }

    Dialogue *d = &shm_ptr->dialogues[dialogue_idx];
    
    /* Check if participant already exists */
    for (int i = 0; i < d->num_participants; i++) {
        if (d->participants[i] == pid) {
            return 0;  /* Already a participant */
        }
    }

    if (d->num_participants >= MAX_PROCESSES_PER_DIALOGUE) {
        fprintf(stderr, "Max participants reached for dialogue\n");
        return -1;
    }

    d->participants[d->num_participants++] = pid;
    return 0;
}

/* Create or join a dialogue */
int create_or_join_dialogue(int dialogue_id) {
    SharedMemory *shm_ptr = get_shared_memory();
    if (shm_ptr == NULL) {
        fprintf(stderr, "Shared memory not initialized\n");
        return -1;
    }

    pid_t my_pid = getpid();

    lock_shared_memory();

    int idx = find_dialogue(dialogue_id);

    if (idx == -1) {
        /* Create new dialogue */
        if (shm_ptr->num_dialogues >= MAX_DIALOGUES) {
            fprintf(stderr, "Max dialogues reached\n");
            unlock_shared_memory();
            return -1;
        }

        idx = shm_ptr->num_dialogues++;
        Dialogue *d = &shm_ptr->dialogues[idx];
        
        memset(d, 0, sizeof(Dialogue));
        d->dialogue_id = dialogue_id;
        d->active = 1;
        d->message_head = 0;
        d->message_count = 0;

        if (add_participant_to_dialogue(idx, my_pid) == -1) {
            shm_ptr->num_dialogues--;
            unlock_shared_memory();
            return -1;
        }

        printf("Created new dialogue with ID: %d\n", dialogue_id);
    } else {
        /* Join existing dialogue */
        if (add_participant_to_dialogue(idx, my_pid) == -1) {
            unlock_shared_memory();
            return -1;
        }
        printf("Joined dialogue with ID: %d\n", dialogue_id);
    }

    unlock_shared_memory();
    return idx;
}
