#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include "../include/chat.h"
#include "../include/ui.h"
#include "../include/global.h"
#include "../include/ipc.h"

// Βοηθητικές συναρτήσεις για κλείδωμα/ξεκλείδωμα (χειρίζονται Mac vs Linux)
static void sem_lock() {
#ifdef __APPLE__
    sem_wait(mutex);
#else
    sem_wait(&shm->mutex);
#endif
}

static void sem_unlock() {
#ifdef __APPLE__
    sem_post(mutex);
#else
    sem_post(&shm->mutex);
#endif
}

// Thread που ακούει για νέα μηνύματα
void* receiver_thread(void* arg) {
    (void)arg;
    // Αρχίζουμε διαβάζοντας μόνο τα νέα μηνύματα (από το τρέχον seq counter και μετά)
    long last_read_seq = shm->global_seq_counter;

    while (in_dialogue) {
        sem_lock(); // Κρίσιμο τμήμα: Ανάγνωση μνήμης

        // Έλεγχος αν ο διάλογος απενεργοποιήθηκε από άλλον
        if (dialogue_index != -1 && !shm->dialogues[dialogue_index].active) {
            write_to_chat("SYSTEM", "Ο διάλογος τερματίστηκε από συνομιλητή.", 3);
            in_dialogue = 0;
            sem_unlock();
            break;
        }

        // Σάρωση όλων των μηνυμάτων
        for (int i = 0; i < MAX_MESSAGES; i++) {
            Message* msg = &shm->messages[i];

            // Κριτήρια εμφάνισης:
            // 1. Δεν είναι κενό
            // 2. Είναι για το δικό μας διάλογο
            // 3. ΔΕΝ το στείλαμε εμείς
            // 4. Είναι νεότερο από το τελευταίο που διαβάσαμε
            if (!msg->is_free && 
                msg->dialogue_id == my_dialogue_id && 
                msg->sender_pid != getpid() &&
                msg->sequence_num > last_read_seq) {

                if (strcmp(msg->payload, "TERMINATE") == 0) {
                    write_to_chat("SYSTEM", "Ελήφθη εντολή TERMINATE.", 3);
                    
                    // Καθαρισμός του διαλόγου
                    if (dialogue_index != -1 && shm->dialogues[dialogue_index].id == msg->dialogue_id) {
                        shm->dialogues[dialogue_index].active = 0;
                        shm->dialogues[dialogue_index].participants = 0;
                        shm->dialogues[dialogue_index].id = 0;
                        // Διαγραφή όλων των μηνυμάτων αυτού του διαλόγου
                        for (int m = 0; m < MAX_MESSAGES; m++) {
                            if (shm->messages[m].dialogue_id == msg->dialogue_id) {
                                shm->messages[m].is_free = 1;
                            }
                        }
                    }
                    in_dialogue = 0;
                } else {
                    char sender_name[32];
                    snprintf(sender_name, 32, "User %d", msg->sender_pid);
                    write_to_chat(sender_name, msg->payload, 2);
                }

                // Ενημέρωση μετρητών
                msg->readers_count++;
                last_read_seq = msg->sequence_num;

                // Αν το διάβασαν όλοι οι συμμετέχοντες (εκτός του αποστολέα), διαγράφεται
                if (msg->readers_count >= (shm->dialogues[dialogue_index].participants - 1)) {
                    msg->is_free = 1;
                }
            }
        }
        
        sem_unlock();
        usleep(100000); // Μικρή παύση 100ms για να μην υπερφορτώνουμε τη CPU
    }
    return NULL;
}

void send_message_to_shm(const char* payload) {
    sem_lock();
        
    // Εύρεση κενής θέσης μηνύματος
    int msg_slot = -1;
    for (int i = 0; i < MAX_MESSAGES; i++) {
        if (shm->messages[i].is_free) {
            msg_slot = i;
            break;
        }
    }

    if (msg_slot != -1) {
        // Εγγραφή μηνύματος
        shm->global_seq_counter++;
        shm->messages[msg_slot].is_free = 0;
        shm->messages[msg_slot].dialogue_id = my_dialogue_id;
        shm->messages[msg_slot].sender_pid = getpid();
        strncpy(shm->messages[msg_slot].payload, payload, MAX_MSG_LEN);
        shm->messages[msg_slot].readers_count = 0;
        shm->messages[msg_slot].sequence_num = shm->global_seq_counter;
        
        // Ειδικός χειρισμός για το TERMINATE
        if (strcmp(payload, "TERMINATE") == 0) {
            if (dialogue_index != -1) {
                shm->dialogues[dialogue_index].active = 0;
                shm->dialogues[dialogue_index].participants = 0;
                shm->dialogues[dialogue_index].id = 0;
                // Καθαρισμός μηνυμάτων
                for (int m = 0; m < MAX_MESSAGES; m++) {
                    if (shm->messages[m].dialogue_id == my_dialogue_id) {
                        shm->messages[m].is_free = 1;
                    }
                }
            }
            in_dialogue = 0;
        }
    }
    sem_unlock();
}