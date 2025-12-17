#ifndef GLOBALS_H
#define GLOBALS_H

#include "defs.h"
#include <pthread.h>
#include <semaphore.h>

// Δείκτης προς τη διαμοιραζόμενη μνήμη
extern SharedMemory* shm;

#ifdef __APPLE__
    // Δείκτης για τον named semaphore (μόνο για macOS)
    extern sem_t *mutex;
#endif

// Μεταβλητές κατάστασης της εφαρμογής
extern int my_dialogue_id;  // Το ID του τρέχοντος διαλόγου
extern int dialogue_index;  // Η θέση του διαλόγου στον πίνακα (array index)
extern int running;         // Flag για το αν τρέχει η εφαρμογή (Main Loop)
extern int in_dialogue;     // Flag για το αν είμαστε μέσα σε chat room
extern pthread_mutex_t screen_mutex; // Mutex για τον συγχρονισμό της οθόνης (ncurses)

#endif