#ifndef DEFS_H
#define DEFS_H

#include <semaphore.h>

#define SHM_NAME "/chat_shm"
#define SEM_NAME "/chat_sem"  // <--- ΑΥΤΗ Η ΓΡΑΜΜΗ ΕΛΕΙΠΕ
#define MAX_MSG_LEN 256
#define MAX_MESSAGES 100
#define MAX_DIALOGUES 10

typedef struct {
    int id;                 // Το μοναδικό ID του διαλόγου
    int participants;       // Πλήθος ενεργών διεργασιών στο διάλογο
    int active;             // 1 αν ο διάλογος είναι ενεργός, 0 αν όχι
} Dialogue;

typedef struct {
    int dialogue_id;        // Σε ποιον διάλογο ανήκει το μήνυμα
    int sender_pid;         // Ποιος το έστειλε
    char payload[MAX_MSG_LEN]; 
    int readers_count;      // Πόσοι το έχουν διαβάσει
    int is_free;            // 1 αν η θυρίδα είναι κενή
    long sequence_num;      // Αύξων αριθμός για να ξεχωρίζουμε τα μηνύματα
} Message;

typedef struct {
#ifdef __APPLE__
    // Στο MacOS ο semaphore είναι pointer και δεν αποθηκεύεται εδώ.
    // Βάζουμε ένα dummy int για να μην είναι κενό το struct ή αλλάξουν τα offsets.
    int dummy_padding; 
#else
    sem_t mutex;            // Στο Linux ο semaphore μπαίνει ΜΕΣΑ στη μνήμη
#endif
    Dialogue dialogues[MAX_DIALOGUES];
    Message messages[MAX_MESSAGES];
    int active_app_count;   // Πόσες διεργασίες τρέχουν συνολικά
    long global_seq_counter; // Μετρητής για sequence numbers
} SharedMemory;

#endif