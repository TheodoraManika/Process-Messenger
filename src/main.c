#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h> 
#include "../include/defs.h"
#include "../include/ipc.h"
#include "../include/global.h"
#include "../include/ui.h"
#include "../include/chat.h"

// Macros για συντομία στο κλείδωμα
#ifdef __APPLE__
    #define SEM_LOCK() sem_wait(mutex)
    #define SEM_UNLOCK() sem_post(mutex)
#else
    #define SEM_LOCK() sem_wait(&shm->mutex)
    #define SEM_UNLOCK() sem_post(&shm->mutex)
#endif

// Συνάρτηση για καθαρό τερματισμό και αποδέσμευση πόρων
void cleanup_and_exit() {
    SEM_LOCK();
    
    // Αφαίρεση συμμετέχοντα από τον διάλογο (αν υπάρχει)
    if (dialogue_index != -1) {
        if (shm->dialogues[dialogue_index].participants > 0) {
            shm->dialogues[dialogue_index].participants--;
        }
        // Αν δεν έμεινε κανείς, κλείσε τον διάλογο
        if (shm->dialogues[dialogue_index].participants <= 0) {
            shm->dialogues[dialogue_index].active = 0;
            shm->dialogues[dialogue_index].id = 0;
            shm->dialogues[dialogue_index].participants = 0;
        }
    }
    shm->active_app_count--;
    int apps_left = shm->active_app_count;

#ifdef __APPLE__
    SEM_UNLOCK();
    sem_close(mutex);
    detach_shm(shm);
    // Αν είμαστε η τελευταία εφαρμογή, διαγράφουμε τα πάντα (unlink)
    if (apps_left <= 0) {
        sem_unlink(SEM_NAME);
        destroy_shm();
    }
#else
    SEM_UNLOCK();
    detach_shm(shm);
    if (apps_left <= 0) {
        sem_destroy(&shm->mutex);
        destroy_shm();
    }
#endif
    cleanup_ncurses();
    exit(0);
}

int main() {
    // 1. Σύνδεση στη μνήμη
    shm = connect_to_shm();
    
    // 2. Αρχικοποίηση Σηματοφόρου
#ifdef __APPLE__
    mutex = sem_open(SEM_NAME, O_CREAT, 0644, 1);
    if (mutex == SEM_FAILED) { perror("sem_open"); exit(1); }
#else
    if (shm->active_app_count == 0) {
        if (sem_init(&shm->mutex, 1, 1) < 0) { perror("sem_init"); exit(1); }
    }
#endif

    SEM_LOCK();
    // 3. Εκκαθάριση "ορφανών" διαλόγων (αν το πρόγραμμα κράσαρε παλιά)
    for (int i = 0; i < MAX_DIALOGUES; i++) {
        int did = shm->dialogues[i].id;
        if (shm->dialogues[i].participants <= 0) {
            if (did != 0) {
                // Καθαρισμός μηνυμάτων ορφανού διαλόγου
                for (int m = 0; m < MAX_MESSAGES; m++) {
                    if (shm->messages[m].dialogue_id == did) shm->messages[m].is_free = 1;
                }
            }
            shm->dialogues[i].participants = 0;
            shm->dialogues[i].active = 0;
            shm->dialogues[i].id = 0;
        }
    }

    // Αν είμαστε η 1η εφαρμογή, μηδενίζουμε τα πάντα
    if (shm->active_app_count == 0) {
        memset(shm->dialogues, 0, sizeof(shm->dialogues));
        for (int i = 0; i < MAX_MESSAGES; i++) shm->messages[i].is_free = 1;
        shm->global_seq_counter = 0;
    }
    shm->active_app_count++;
    SEM_UNLOCK();

    int choice;
    // 4. Main Menu Loop
    while (running) {
        printf("--- Messaging System (PID: %d) ---\n", getpid());
        printf("1. Start new dialogue\n2. Join existing dialogue\n3. Exit\nChoice: ");
        if (scanf("%d", &choice) != 1) {
            int c; while ((c = getchar()) != '\n' && c != EOF); // Flush stdin
            continue;
        }

        if (choice == 3) break; // Exit

        SEM_LOCK();
        if (choice == 1) {
            // Δημιουργία νέου διαλόγου
            dialogue_index = find_free_dialogue_slot(shm);
            if (dialogue_index == -1) {
                printf("Δεν υπάρχει χώρος για νέο διάλογο.\n");
                SEM_UNLOCK();
                continue;
            }
            printf("Δώσε ID για τον νέο διάλογο: ");
            if (scanf("%d", &my_dialogue_id) != 1) {
                int c; while ((c = getchar()) != '\n' && c != EOF);
                SEM_UNLOCK();
                continue;
            }
            if (find_dialogue_index(shm, my_dialogue_id) != -1) {
                 printf("Το ID υπάρχει ήδη!\n");
                 SEM_UNLOCK();
                 continue;
            }
            shm->dialogues[dialogue_index].id = my_dialogue_id;
            shm->dialogues[dialogue_index].participants = 1;
            shm->dialogues[dialogue_index].active = 1;
        } else {
            // Συμμετοχή σε υπάρχοντα
            printf("Δώσε το ID του διαλόγου: ");
            if (scanf("%d", &my_dialogue_id) != 1) {
                int c; while ((c = getchar()) != '\n' && c != EOF);
                SEM_UNLOCK();
                continue;
            }
            dialogue_index = find_dialogue_index(shm, my_dialogue_id);
            if (dialogue_index == -1) {
                printf("Ο διάλογος δεν βρέθηκε.\n");
                SEM_UNLOCK();
                continue;
            }
            shm->dialogues[dialogue_index].participants++;
        }
        SEM_UNLOCK();

        // 5. Έναρξη UI και Chat Loop
        in_dialogue = 1;
        init_ui();

        // Εκκίνηση του thread λήψης
        pthread_t r_thread;
        pthread_create(&r_thread, NULL, receiver_thread, NULL);

        char buffer[MAX_MSG_LEN];
        
        // Loop εισαγωγής κειμένου
        while (in_dialogue) {
            clear_input_window();
            
            // Περιμένουμε για είσοδο χρήστη
            int input_status = get_user_input(buffer);
            
            // Ελέγχουμε αν το thread έκλεισε τον διάλογο (π.χ. ήρθε TERMINATE)
            if (!in_dialogue) break; 
            
            if (input_status == 1 && strlen(buffer) > 0) {
                write_to_chat("ME", buffer, 1);
                send_message_to_shm(buffer);
            }
        }

        pthread_join(r_thread, NULL); // Αναμονή τερματισμού thread
        cleanup_ncurses();            // Κλείσιμο γραφικών

        // 6. Έξοδος από τον διάλογο - Ενημέρωση μνήμης
        SEM_LOCK();
        if (dialogue_index != -1) {
            if (shm->dialogues[dialogue_index].participants > 0) {
                shm->dialogues[dialogue_index].participants--;
            }
            if (shm->dialogues[dialogue_index].participants <= 0) {
                shm->dialogues[dialogue_index].active = 0;
                shm->dialogues[dialogue_index].id = 0;
                shm->dialogues[dialogue_index].participants = 0;
            }
        }
        SEM_UNLOCK();

        // Reset μεταβλητών για επιστροφή στο μενού
        dialogue_index = -1;
        my_dialogue_id = -1;
        in_dialogue = 0;
    }

    cleanup_and_exit();
    return 0;
}