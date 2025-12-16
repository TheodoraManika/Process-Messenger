#ifndef IPC_H
#define IPC_H

#include "defs.h"

// Αρχικοποίηση ή σύνδεση στη διαμοιραζόμενη μνήμη
SharedMemory* connect_to_shm();

// Αποσύνδεση
void detach_shm(SharedMemory* shm);

// Καθαρισμός και διαγραφή μνήμης (μόνο στο τέλος)
void destroy_shm();

// Βοηθητικές
int find_free_dialogue_slot(SharedMemory* shm);
int find_dialogue_index(SharedMemory* shm, int dialogue_id);

#endif