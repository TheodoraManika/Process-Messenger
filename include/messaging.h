#ifndef MESSAGING_H
#define MESSAGING_H

#include <sys/types.h>
#include <time.h>

#define MAX_MESSAGE_SIZE 256
#define MAX_PROCESSES_PER_DIALOGUE 32
#define MAX_MESSAGES_PER_DIALOGUE 128
#define MAX_DIALOGUES 64
#define SHM_SIZE 65536

/* Message structure */
typedef struct {
    int dialogue_id;
    pid_t sender_pid;
    char payload[MAX_MESSAGE_SIZE];
    pid_t received_by[MAX_PROCESSES_PER_DIALOGUE];
    int num_received_by;
    int active;  /* 1 if message is active, 0 if should be removed */
} Message;

/* Dialogue structure */
typedef struct {
    int dialogue_id;
    pid_t participants[MAX_PROCESSES_PER_DIALOGUE];
    int num_participants;
    Message messages[MAX_MESSAGES_PER_DIALOGUE];
    int message_head;
    int message_count;
    int active;  /* 1 if dialogue is active, 0 if terminated */
} Dialogue;

/* Shared memory structure */
typedef struct {
    Dialogue dialogues[MAX_DIALOGUES];
    int num_dialogues;
    int next_dialogue_id;
} SharedMemory;

#endif
