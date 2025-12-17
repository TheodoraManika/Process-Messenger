#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <ncurses.h> // Η βιβλιοθήκη για το γραφικό περιβάλλον
#include <ctype.h>
#include "../include/defs.h"
#include "../include/ipc.h"

// --- Global Variables ---
SharedMemory* shm;
#ifdef __APPLE__
    sem_t *mutex;
#else
    // Στο Linux δείχνουμε απευθείας στη μνήμη
#endif

int my_dialogue_id = -1;
int dialogue_index = -1;
int running = 1;    /* app-level running flag */
int in_dialogue = 0; /* dialogue-level running flag (controls UI/receiver loop) */

// Ncurses Windows
WINDOW *chat_win;
WINDOW *input_win;
WINDOW *info_win;

// Mutex για να μην "μπερδεύεται" η οθόνη όταν γράφουν 2 threads ταυτόχρονα
pthread_mutex_t screen_mutex = PTHREAD_MUTEX_INITIALIZER;

// --- Helper: Safe Print ---
void write_to_chat(const char* username, const char* msg, int color_pair) {
    pthread_mutex_lock(&screen_mutex);
    
    wattron(chat_win, COLOR_PAIR(color_pair) | A_BOLD);
    wprintw(chat_win, "[%s]: ", username);
    wattroff(chat_win, COLOR_PAIR(color_pair) | A_BOLD);
    
    wprintw(chat_win, "%s\n", msg);
    wrefresh(chat_win); // Ενημέρωση παραθύρου chat
    
    // Επαναφορά κέρσορα στο input window
    wrefresh(input_win);
    
    pthread_mutex_unlock(&screen_mutex);
}

// --- Receiver Thread ---
void* receiver_thread(void* arg) {
    (void)arg;
    long last_read_seq = shm->global_seq_counter;

        while (in_dialogue) {
        // Lock IPC Mutex
#ifdef __APPLE__
        sem_wait(mutex);
#else
        sem_wait(&shm->mutex);
#endif

        // Έλεγχος αν έκλεισε ο διάλογος
        if (dialogue_index != -1 && !shm->dialogues[dialogue_index].active) {
            write_to_chat("SYSTEM", "Dialogue ended by peer.", 3);
            in_dialogue = 0;
#ifdef __APPLE__
            sem_post(mutex);
#else
            sem_post(&shm->mutex);
#endif
            break;
        }

        for (int i = 0; i < MAX_MESSAGES; i++) {
            Message* msg = &shm->messages[i];

            if (!msg->is_free && 
                msg->dialogue_id == my_dialogue_id && 
                msg->sender_pid != getpid() &&
                msg->sequence_num > last_read_seq) {

                if (strcmp(msg->payload, "TERMINATE") == 0) {
                    write_to_chat("SYSTEM", "Received TERMINATE.", 3);
                    /* Mark dialogue inactive and clear related messages */
                    if (dialogue_index != -1 && shm->dialogues[dialogue_index].id == msg->dialogue_id) {
                        shm->dialogues[dialogue_index].active = 0;
                        shm->dialogues[dialogue_index].participants = 0;
                        shm->dialogues[dialogue_index].id = 0;
                        /* clear messages belonging to this dialogue */
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
                    // Χρώμα 2 = Cyan (για τους άλλους)
                    write_to_chat(sender_name, msg->payload, 2);
                }

                msg->readers_count++;
                last_read_seq = msg->sequence_num;

                if (msg->readers_count >= (shm->dialogues[dialogue_index].participants - 1)) {
                    msg->is_free = 1;
                }
            }
        }
        
        // Unlock IPC Mutex
#ifdef __APPLE__
        sem_post(mutex);
#else
        sem_post(&shm->mutex);
#endif
        usleep(100000); // 100ms delay
    }
    return NULL;
}

void cleanup_ncurses() {
    endwin(); // Κλείσιμο γραφικού περιβάλλοντος
}

void cleanup_and_exit() {
    // Καθαρισμός shared memory logic
#ifdef __APPLE__
    sem_wait(mutex);
#else
    sem_wait(&shm->mutex);
#endif
    
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

    shm->active_app_count--;
    int apps_left = shm->active_app_count;

#ifdef __APPLE__
    sem_post(mutex);
    sem_close(mutex);
    detach_shm(shm);
    if (apps_left <= 0) {
        sem_unlink(SEM_NAME);
        destroy_shm();
    }
#else
    sem_post(&shm->mutex);
    detach_shm(shm);
    if (apps_left <= 0) {
        sem_destroy(&shm->mutex);
        destroy_shm();
    }
#endif

    cleanup_ncurses();
    exit(0);
}

void init_ui() {
    initscr();              // Start ncurses
    cbreak();               // Line buffering disabled
    // noecho();            // Don't echo while we do getch (θα το χειριστούμε εμείς)
    start_color();          // Enable colors
    curs_set(1);            // Show cursor

    // Ορισμός χρωμάτων (Foreground, Background)
    init_pair(1, COLOR_GREEN, COLOR_BLACK);   // Εμείς
    init_pair(2, COLOR_CYAN, COLOR_BLACK);    // Άλλοι
    init_pair(3, COLOR_RED, COLOR_BLACK);     // System/Errors
    init_pair(4, COLOR_WHITE, COLOR_BLUE);    // Header/Borders

    // Υπολογισμός διαστάσεων
    int height, width;
    getmaxyx(stdscr, height, width);

    // 1. Info Window (Top)
    info_win = newwin(3, width, 0, 0);
    wbkgd(info_win, COLOR_PAIR(4));
    box(info_win, 0, 0);
    mvwprintw(info_win, 1, 2, "Chat App | PID: %d | Dialogue ID: %d", getpid(), my_dialogue_id);
    wrefresh(info_win);

    // 2. Chat Window (Middle)
    // Ύψος = Συνολικό - Header(3) - Input(3)
    chat_win = newwin(height - 6, width, 3, 0);
    scrollok(chat_win, TRUE); // Επιτρέπει το scrolling
    wrefresh(chat_win);

    // 3. Input Window (Bottom)
    input_win = newwin(3, width, height - 3, 0);
    box(input_win, 0, 0);
    mvwprintw(input_win, 1, 2, "> ");
    wrefresh(input_win);
}

int main() {
    // --- Setup Shared Memory ---
    shm = connect_to_shm();
    
#ifdef __APPLE__
    mutex = sem_open(SEM_NAME, O_CREAT, 0644, 1);
    if (mutex == SEM_FAILED) { perror("sem_open"); exit(1); }
#else
    if (shm->active_app_count == 0) {
        if (sem_init(&shm->mutex, 1, 1) < 0) { perror("sem_init"); exit(1); }
    }
#endif

    // --- Active App Count Increment ---
#ifdef __APPLE__
    sem_wait(mutex);
#else
    sem_wait(&shm->mutex);
#endif

    /* Sanitize dialogue slots: prevent negative participants and clear stale dialogues */
    for (int i = 0; i < MAX_DIALOGUES; i++) {
        int did = shm->dialogues[i].id;
        if (shm->dialogues[i].participants <= 0) {
            /* If slot had an id but zero/negative participants, free its messages */
            if (did != 0) {
                for (int m = 0; m < MAX_MESSAGES; m++) {
                    if (shm->messages[m].dialogue_id == did) {
                        shm->messages[m].is_free = 1;
                    }
                }
            }
            shm->dialogues[i].participants = 0;
            shm->dialogues[i].active = 0;
            shm->dialogues[i].id = 0;
        }
    }

    if (shm->active_app_count == 0) {
        /* First time init: ensure messages array marked free and seq counter reset */
        memset(shm->dialogues, 0, sizeof(shm->dialogues));
        for (int i = 0; i < MAX_MESSAGES; i++) shm->messages[i].is_free = 1;
        shm->global_seq_counter = 0;
    }

    shm->active_app_count++;

#ifdef __APPLE__
    sem_post(mutex);
#else
    sem_post(&shm->mutex);
#endif

    // --- Application loop: show menu, join/create dialogues, allow multiple dialogues ---
    int choice;
    while (running) {
        // Show simple menu (standard I/O) before entering ncurses for each dialogue
        printf("--- Messaging System (PID: %d) ---\n", getpid());
        printf("1. Start new dialogue\n2. Join existing dialogue\n3. Exit\nChoice: ");
        if (scanf("%d", &choice) != 1) {
            // clear invalid input
            int c; while ((c = getchar()) != '\n' && c != EOF);
            continue;
        }

        if (choice == 3) {
            /* Exit application */
            break;
        }

#ifdef __APPLE__
        sem_wait(mutex);
#else
        sem_wait(&shm->mutex);
#endif

        if (choice == 1) {
            dialogue_index = find_free_dialogue_slot(shm);
            if (dialogue_index == -1) {
                printf("No space.\n");
#ifdef __APPLE__
                sem_post(mutex);
#else
                sem_post(&shm->mutex);
#endif
                continue;
            }
            printf("Enter ID for new dialogue: ");
            if (scanf("%d", &my_dialogue_id) != 1) {
                int c; while ((c = getchar()) != '\n' && c != EOF);
#ifdef __APPLE__
                sem_post(mutex);
#else
                sem_post(&shm->mutex);
#endif
                continue;
            }

            if (find_dialogue_index(shm, my_dialogue_id) != -1) {
                 printf("ID exists!\n");
#ifdef __APPLE__
                sem_post(mutex);
#else
                sem_post(&shm->mutex);
#endif
                 continue;
            }

            shm->dialogues[dialogue_index].id = my_dialogue_id;
            shm->dialogues[dialogue_index].participants = 1;
            shm->dialogues[dialogue_index].active = 1;

        } else {
            printf("Enter Dialogue ID to join: ");
            if (scanf("%d", &my_dialogue_id) != 1) {
                int c; while ((c = getchar()) != '\n' && c != EOF);
#ifdef __APPLE__
                sem_post(mutex);
#else
                sem_post(&shm->mutex);
#endif
                continue;
            }
            dialogue_index = find_dialogue_index(shm, my_dialogue_id);
            if (dialogue_index == -1) {
                printf("Not found.\n");
#ifdef __APPLE__
                sem_post(mutex);
#else
                sem_post(&shm->mutex);
#endif
                continue;
            }
            shm->dialogues[dialogue_index].participants++;
        }

#ifdef __APPLE__
        sem_post(mutex);
#else
        sem_post(&shm->mutex);
#endif

        /* Enter dialogue UI */
        in_dialogue = 1;
        init_ui();

        /* Start Receiver Thread */
        pthread_t r_thread;
        pthread_create(&r_thread, NULL, receiver_thread, NULL);

        char buffer[MAX_MSG_LEN];

        while (in_dialogue) {
        // Καθαρισμός input box
        pthread_mutex_lock(&screen_mutex);
        werase(input_win);
        box(input_win, 0, 0);
        mvwprintw(input_win, 1, 2, "> ");
        wrefresh(input_win);
        pthread_mutex_unlock(&screen_mutex);

        // Λήψη εισόδου από χρήστη (non-blocking with timeout)
        // We use wgetch with wtimeout so the input loop can be interrupted
        // when `in_dialogue` is cleared by the receiver thread (no need to press Enter).
        wtimeout(input_win, 100); // 100 ms timeout
        int idx = 0;
        buffer[0] = '\0';
        int ch;
        while (in_dialogue) {
            pthread_mutex_lock(&screen_mutex);
            // redraw prompt and current buffer
            werase(input_win);
            box(input_win, 0, 0);
            mvwprintw(input_win, 1, 2, "> %s", buffer);
            wrefresh(input_win);
            pthread_mutex_unlock(&screen_mutex);

            ch = wgetch(input_win);
            if (ch == ERR) {
                // timeout, check in_dialogue again
                continue;
            }

            if (ch == '\n' || ch == '\r') {
                // Enter pressed — finish input
                break;
            } else if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
                if (idx > 0) {
                    idx--;
                    buffer[idx] = '\0';
                }
            } else if (isprint(ch) && idx < MAX_MSG_LEN - 1) {
                buffer[idx++] = (char)ch;
                buffer[idx] = '\0';
            }
        }

        // If dialogue was ended by receiver thread, skip processing this buffer
        if (!in_dialogue) {
            // clear input window one last time
            pthread_mutex_lock(&screen_mutex);
            werase(input_win);
            box(input_win, 0, 0);
            mvwprintw(input_win, 1, 2, "> ");
            wrefresh(input_win);
            pthread_mutex_unlock(&screen_mutex);
            continue;
        }

        if (strlen(buffer) == 0) continue;
        if (!running) break;

        // Εμφάνιση του δικού μας μηνύματος (Χρώμα 1 = Πράσινο)
        write_to_chat("ME", buffer, 1);

        // Αποστολή στη Shared Memory
#ifdef __APPLE__
        sem_wait(mutex);
#else
        sem_wait(&shm->mutex);
#endif
        
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
                /* End current dialogue only: mark dialogue inactive and clear messages */
                if (dialogue_index != -1) {
                    shm->dialogues[dialogue_index].active = 0;
                    shm->dialogues[dialogue_index].participants = 0;
                    shm->dialogues[dialogue_index].id = 0;
                    for (int m = 0; m < MAX_MESSAGES; m++) {
                        if (shm->messages[m].dialogue_id == my_dialogue_id) {
                            shm->messages[m].is_free = 1;
                        }
                    }
                }
                in_dialogue = 0;
            }
        }

#ifdef __APPLE__
        sem_post(mutex);
#else
        sem_post(&shm->mutex);
#endif
    }

        pthread_join(r_thread, NULL);
        /* Exit ncurses for this dialogue and return to menu */
        cleanup_ncurses();

        /* Leave dialogue: decrement participants and mark inactive if needed */
    #ifdef __APPLE__
        sem_wait(mutex);
    #else
        sem_wait(&shm->mutex);
    #endif

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

    #ifdef __APPLE__
        sem_post(mutex);
    #else
        sem_post(&shm->mutex);
    #endif

        /* Reset dialogue state and loop back to menu */
        dialogue_index = -1;
        my_dialogue_id = -1;
        in_dialogue = 0;
        }

        /* Application exiting: perform full cleanup */
        cleanup_and_exit();
        return 0;
    }