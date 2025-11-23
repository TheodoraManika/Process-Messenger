#ifndef DIALOGUE_H
#define DIALOGUE_H

#include <sys/types.h>

/* Dialogue Management Functions */
int create_or_join_dialogue(int dialogue_id);
int find_dialogue(int dialogue_id);
int add_participant_to_dialogue(int dialogue_idx, pid_t pid);

#endif
