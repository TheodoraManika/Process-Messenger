#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include "../include/defs.h"
#include "../include/ipc.h"

SharedMemory* shm;
int my_dialogue_id = -1;
int dialogue_index = -1;
int running = 1;

// Thread function για λήψη μηνυμάτων
void* receiver_thread(void* arg) {
    long last_read_seq = shm->global_seq_counter; // Ξεκινάμε διαβάζοντας μόνο νέα μηνύματα

    while (running) {
        sem_wait(&shm->mutex);
        
        // Έλεγχος αν ο διάλογος είναι ακόμα ενεργός
        if (!shm->dialogues[dialogue_index].active) {
            printf("\n[System]: Dialogue ended by peer.\n");
            running = 0;
            sem_post(&shm->mutex);
            break;
        }

        for (int i = 0; i < MAX_MESSAGES; i++) {
            Message* msg = &shm->messages[i];

            // Αν υπάρχει μήνυμα, είναι για τον διάλογό μας, ΔΕΝ το στείλαμε εμείς, και είναι νεότερο
            if (!msg->is_free && 
                msg->dialogue_id == my_dialogue_id && 
                msg->sender_pid != getpid() &&
                msg->sequence_num > last_read_seq) {

                // Έλεγχος TERMINATE
                if (strcmp(msg->payload, "TERMINATE") == 0) {
                    printf("\n[System]: Received TERMINATE signal. Closing...\n");
                    running = 0;
                } else {
                    printf("\n[Peer %d]: %s\nInput: ", msg->sender_pid, msg->payload);
                    fflush(stdout);
                }

                // Ενημέρωση κατάστασης μηνύματος
                msg->readers_count++;
                last_read_seq = msg->sequence_num; // Ενημέρωση τοπικού δείκτη

                // Αν το έχουν διαβάσει όλοι (Participants - 1 Sender), διαγράφεται
                if (msg->readers_count >= (shm->dialogues[dialogue_index].participants - 1)) {
                    msg->is_free = 1;
                }
            }
        }
        sem_post(&shm->mutex);
        usleep(200000); // Μικρή καθυστέρηση για να μην καίει CPU (polling)
    }
    return NULL;
}

void cleanup_and_exit() {
    sem_wait(&shm->mutex);
    
    // Αποχώρηση από τον διάλογο
    if (dialogue_index != -1) {
        shm->dialogues[dialogue_index].participants--;
        if (shm->dialogues[dialogue_index].participants <= 0) {
            shm->dialogues[dialogue_index].active = 0;
            printf("[System]: Dialogue closed (no participants left).\n");
        }
    }

    shm->active_app_count--;
    int apps_left = shm->active_app_count;
    sem_post(&shm->mutex);

    detach_shm(shm);

    // Αν είμαστε η τελευταία εφαρμογή γενικώς, σβήνουμε τη μνήμη
    if (apps_left <= 0) {
        printf("[System]: Last process exiting. Cleaning up Shared Memory.\n");
        destroy_shm();
    }

    exit(0);
}

int main() {
    shm = connect_to_shm();
    
    // Αρχικοποίηση Semaphore αν είναι η πρώτη φορά (Active apps = 0)
    // Προσοχή: Εδώ γίνεται απλοϊκά. Σε real-world θέλει atomic check.
    if (shm->active_app_count == 0) {
        sem_init(&shm->mutex, 1, 1);
        memset(shm->dialogues, 0, sizeof(shm->dialogues));
        for(int i=0; i<MAX_MESSAGES; i++) shm->messages[i].is_free = 1;
        shm->global_seq_counter = 0;
    }

    sem_wait(&shm->mutex);
    shm->active_app_count++;
    sem_post(&shm->mutex);

    printf("--- Messaging System (PID: %d) ---\n", getpid());
    printf("1. Start new dialogue\n2. Join existing dialogue\nChoice: ");
    int choice;
    scanf("%d", &choice);

    sem_wait(&shm->mutex);
    if (choice == 1) {
        dialogue_index = find_free_dialogue_slot(shm);
        if (dialogue_index == -1) {
            printf("No space for new dialogues.\n");
            sem_post(&shm->mutex);
            cleanup_and_exit();
        }
        printf("Enter ID for new dialogue: ");
        scanf("%d", &my_dialogue_id);
        
        // Έλεγχος αν υπάρχει ήδη
        if (find_dialogue_index(shm, my_dialogue_id) != -1) {
             printf("Dialogue ID already exists!\n");
             sem_post(&shm->mutex);
             cleanup_and_exit();
        }

        shm->dialogues[dialogue_index].id = my_dialogue_id;
        shm->dialogues[dialogue_index].participants = 1;
        shm->dialogues[dialogue_index].active = 1;
        printf("Dialogue %d created.\n", my_dialogue_id);

    } else {
        printf("Enter Dialogue ID to join: ");
        scanf("%d", &my_dialogue_id);
        dialogue_index = find_dialogue_index(shm, my_dialogue_id);
        if (dialogue_index == -1) {
            printf("Dialogue not found.\n");
            sem_post(&shm->mutex);
            cleanup_and_exit();
        }
        shm->dialogues[dialogue_index].participants++;
        printf("Joined dialogue %d. Total users: %d\n", my_dialogue_id, shm->dialogues[dialogue_index].participants);
    }
    sem_post(&shm->mutex);

    // Εκκίνηση Receiver Thread
    pthread_t r_thread;
    if (pthread_create(&r_thread, NULL, receiver_thread, NULL) != 0) {
        perror("Failed to create thread");
        cleanup_and_exit();
    }

    // Main Loop - Sending
    char buffer[MAX_MSG_LEN];
    // Καθαρισμός buffer εισόδου
    int c; while ((c = getchar()) != '\n' && c != EOF); 

    while (running) {
        printf("Input: ");
        if (fgets(buffer, MAX_MSG_LEN, stdin) == NULL) break;
        
        // Αφαίρεση newline
        buffer[strcspn(buffer, "\n")] = 0;

        if (strlen(buffer) == 0) continue;

        sem_wait(&shm->mutex);
        
        // Εύρεση κενής θέσης μηνύματος
        int msg_slot = -1;
        for (int i = 0; i < MAX_MESSAGES; i++) {
            if (shm->messages[i].is_free) {
                msg_slot = i;
                break;
            }
        }

        if (msg_slot != -1) {
            shm->global_seq_counter++;
            shm->messages[msg_slot].is_free = 0;
            shm->messages[msg_slot].dialogue_id = my_dialogue_id;
            shm->messages[msg_slot].sender_pid = getpid();
            strncpy(shm->messages[msg_slot].payload, buffer, MAX_MSG_LEN);
            shm->messages[msg_slot].readers_count = 0;
            shm->messages[msg_slot].sequence_num = shm->global_seq_counter;
            
            if (strcmp(buffer, "TERMINATE") == 0) {
                running = 0; // Τερματισμός τοπικά
            }
        } else {
            printf("[System]: Message buffer full. Try again later.\n");
        }

        sem_post(&shm->mutex);
        
        if (!running) break;
    }

    pthread_join(r_thread, NULL);
    cleanup_and_exit();
    return 0;
}