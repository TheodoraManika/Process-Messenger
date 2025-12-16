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
int running = 1;

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

    while (running) {
        // Lock IPC Mutex
#ifdef __APPLE__
        sem_wait(mutex);
#else
        sem_wait(&shm->mutex);
#endif

        // Έλεγχος αν έκλεισε ο διάλογος
        if (dialogue_index != -1 && !shm->dialogues[dialogue_index].active) {
            write_to_chat("SYSTEM", "Dialogue ended by peer. Press ENTER to exit.", 3);
            running = 0;
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
                    write_to_chat("SYSTEM", "Received TERMINATE. Press ENTER to exit.", 3);
                    running = 0;
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
        shm->dialogues[dialogue_index].participants--;
        if (shm->dialogues[dialogue_index].participants <= 0) {
            shm->dialogues[dialogue_index].active = 0;
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

    if (shm->active_app_count == 0) {
        memset(shm->dialogues, 0, sizeof(shm->dialogues));
        for(int i=0; i<MAX_MESSAGES; i++) shm->messages[i].is_free = 1;
        shm->global_seq_counter = 0;
    }
    shm->active_app_count++;

#ifdef __APPLE__
    sem_post(mutex);
#else
    sem_post(&shm->mutex);
#endif

    // --- Simple Setup (Standard IO before Ncurses) ---
    // Κάνουμε τα scanf ΠΡΙΝ μπούμε σε Ncurses mode για να μην μπλέξουμε
    printf("--- Messaging System (PID: %d) ---\n", getpid());
    printf("1. Start new dialogue\n2. Join existing dialogue\nChoice: ");
    int choice;
    if (scanf("%d", &choice) != 1) exit(1);

#ifdef __APPLE__
    sem_wait(mutex);
#else
    sem_wait(&shm->mutex);
#endif

    if (choice == 1) {
        dialogue_index = find_free_dialogue_slot(shm);
        if (dialogue_index == -1) {
            printf("No space.\n");
            exit(0); // Πρέπει να καθαρίσουμε, αλλά για συντομία exit
        }
        printf("Enter ID for new dialogue: ");
        scanf("%d", &my_dialogue_id);
        
        if (find_dialogue_index(shm, my_dialogue_id) != -1) {
             printf("ID exists!\n");
             exit(0);
        }

        shm->dialogues[dialogue_index].id = my_dialogue_id;
        shm->dialogues[dialogue_index].participants = 1;
        shm->dialogues[dialogue_index].active = 1;

    } else {
        printf("Enter Dialogue ID to join: ");
        scanf("%d", &my_dialogue_id);
        dialogue_index = find_dialogue_index(shm, my_dialogue_id);
        if (dialogue_index == -1) {
            printf("Not found.\n");
            exit(0);
        }
        shm->dialogues[dialogue_index].participants++;
    }

#ifdef __APPLE__
    sem_post(mutex);
#else
    sem_post(&shm->mutex);
#endif

    // --- START NCURSES UI ---
    init_ui();

    // Start Receiver Thread
    pthread_t r_thread;
    pthread_create(&r_thread, NULL, receiver_thread, NULL);

    char buffer[MAX_MSG_LEN];

    while (running) {
        // Καθαρισμός input box
        pthread_mutex_lock(&screen_mutex);
        werase(input_win);
        box(input_win, 0, 0);
        mvwprintw(input_win, 1, 2, "> ");
        wrefresh(input_win);
        pthread_mutex_unlock(&screen_mutex);

        // Λήψη εισόδου από χρήστη (Blocking)
        // wgetnstr διαβάζει μέχρι το Enter
        mvwgetnstr(input_win, 1, 4, buffer, MAX_MSG_LEN - 1);

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
                running = 0;
            }
        }

#ifdef __APPLE__
        sem_post(mutex);
#else
        sem_post(&shm->mutex);
#endif
    }

    pthread_join(r_thread, NULL);
    cleanup_and_exit();
    return 0;
}