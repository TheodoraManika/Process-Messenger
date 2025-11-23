#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "messaging.h"
#include "shm_init.h"
#include "dialogue.h"
#include "message.h"
#include "sync.h"

/* Send a message to a dialogue */
int send_message(int dialogue_id, pid_t sender_pid, const char *payload) {
    SharedMemory *shm_ptr = get_shared_memory();
    if (shm_ptr == NULL) {
        fprintf(stderr, "Shared memory not initialized\n");
        return -1;
    }

    lock_shared_memory();

    int idx = find_dialogue(dialogue_id);
    if (idx == -1) {
        fprintf(stderr, "Dialogue not found\n");
        unlock_shared_memory();
        return -1;
    }

    Dialogue *d = &shm_ptr->dialogues[idx];

    if (d->message_count >= MAX_MESSAGES_PER_DIALOGUE) {
        fprintf(stderr, "Message buffer full\n");
        unlock_shared_memory();
        return -1;
    }

    /* Add new message */
    int msg_idx = (d->message_head + d->message_count) % MAX_MESSAGES_PER_DIALOGUE;
    Message *msg = &d->messages[msg_idx];

    memset(msg, 0, sizeof(Message));
    msg->dialogue_id = dialogue_id;
    msg->sender_pid = sender_pid;
    strncpy(msg->payload, payload, MAX_MESSAGE_SIZE - 1);
    msg->payload[MAX_MESSAGE_SIZE - 1] = '\0';
    msg->active = 1;
    msg->num_received_by = 0;

    d->message_count++;

    unlock_shared_memory();
    return 0;
}

/* Receive a message from a dialogue */
int receive_message(int dialogue_id, pid_t receiver_pid, Message *msg) {
    SharedMemory *shm_ptr = get_shared_memory();
    if (shm_ptr == NULL) {
        fprintf(stderr, "Shared memory not initialized\n");
        return -1;
    }

    lock_shared_memory();

    int idx = find_dialogue(dialogue_id);
    if (idx == -1) {
        fprintf(stderr, "Dialogue not found\n");
        unlock_shared_memory();
        return -1;
    }

    Dialogue *d = &shm_ptr->dialogues[idx];

    if (d->message_count == 0) {
        unlock_shared_memory();
        return 0;  /* No messages available */
    }

    Message *front_msg = &d->messages[d->message_head];

    /* Check if this process has already received this message */
    for (int i = 0; i < front_msg->num_received_by; i++) {
        if (front_msg->received_by[i] == receiver_pid) {
            unlock_shared_memory();
            return 0;  /* Already received */
        }
    }

    /* Mark message as received by this process */
    front_msg->received_by[front_msg->num_received_by++] = receiver_pid;

    /* Copy message */
    memcpy(msg, front_msg, sizeof(Message));

    /* Check if all participants have received this message */
    if (front_msg->num_received_by >= d->num_participants) {
        /* Remove message from queue */
        d->message_head = (d->message_head + 1) % MAX_MESSAGES_PER_DIALOGUE;
        d->message_count--;
    }

    unlock_shared_memory();
    return 1;  /* Message received */
}

/* Terminate a dialogue */
int terminate_dialogue(int dialogue_id, pid_t sender_pid) {
    (void)sender_pid;  /* Unused parameter */
    
    SharedMemory *shm_ptr = get_shared_memory();
    if (shm_ptr == NULL) {
        fprintf(stderr, "Shared memory not initialized\n");
        return -1;
    }

    lock_shared_memory();

    int idx = find_dialogue(dialogue_id);
    if (idx == -1) {
        fprintf(stderr, "Dialogue not found\n");
        unlock_shared_memory();
        return -1;
    }

    Dialogue *d = &shm_ptr->dialogues[idx];
    d->active = 0;

    unlock_shared_memory();
    return 0;
}
