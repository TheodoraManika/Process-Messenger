#ifndef MESSAGE_H
#define MESSAGE_H

#include <sys/types.h>
#include "messaging.h"

/* Message Functions */
int send_message(int dialogue_id, pid_t sender_pid, const char *payload);
int receive_message(int dialogue_id, pid_t receiver_pid, Message *msg);
int terminate_dialogue(int dialogue_id, pid_t sender_pid);

#endif
