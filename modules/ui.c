#include <ncurses.h>
#include <pthread.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "../include/ui.h"
#include "../include/global.h"

// Στατικές μεταβλητές για τα παράθυρα (ορατές μόνο σε αυτό το αρχείο)
static WINDOW *chat_win;
static WINDOW *input_win;
static WINDOW *info_win;

void init_ui() {
    initscr();              // Εκκίνηση ncurses
    cbreak();               // Απενεργοποίηση buffering γραμμής
    start_color();          // Ενεργοποίηση χρωμάτων
    curs_set(1);            // Εμφάνιση κέρσορα

    // Ορισμός ζευγαριών χρωμάτων
    init_pair(1, COLOR_GREEN, COLOR_BLACK);   // Εμείς (Πράσινο)
    init_pair(2, COLOR_CYAN, COLOR_BLACK);    // Άλλοι (Γαλάζιο)
    init_pair(3, COLOR_RED, COLOR_BLACK);     // Σύστημα (Κόκκινο)
    init_pair(4, COLOR_WHITE, COLOR_BLUE);    // Header (Λευκό σε Μπλε)

    int height, width;
    getmaxyx(stdscr, height, width);

    // 1. Παράθυρο Πληροφοριών (Πάνω)
    info_win = newwin(3, width, 0, 0);
    wbkgd(info_win, COLOR_PAIR(4)); // Μπλε φόντο
    box(info_win, 0, 0);
    mvwprintw(info_win, 1, 2, "Chat App | PID: %d | Dialogue ID: %d", getpid(), my_dialogue_id);
    wrefresh(info_win);

    // 2. Παράθυρο Chat (Μέση)
    chat_win = newwin(height - 6, width, 3, 0);
    scrollok(chat_win, TRUE); // Επιτρέπει το scrolling όταν γεμίσει
    wrefresh(chat_win);

    // 3. Παράθυρο Εισαγωγής (Κάτω)
    input_win = newwin(3, width, height - 3, 0);
    box(input_win, 0, 0);
    mvwprintw(input_win, 1, 2, "> ");
    wrefresh(input_win);
}

void cleanup_ncurses() {
    endwin(); // Επαναφορά τερματικού
}

void write_to_chat(const char* username, const char* msg, int color_pair) {
    // Κλειδώνουμε το mutex οθόνης για να μην γράφουν ταυτόχρονα το thread και το main
    pthread_mutex_lock(&screen_mutex);
    
    wattron(chat_win, COLOR_PAIR(color_pair) | A_BOLD);
    wprintw(chat_win, "[%s]: ", username);
    wattroff(chat_win, COLOR_PAIR(color_pair) | A_BOLD);
    
    wprintw(chat_win, "%s\n", msg);
    wrefresh(chat_win); 
    
    wrefresh(input_win); // Επαναφορά κέρσορα κάτω
    
    pthread_mutex_unlock(&screen_mutex);
}

void clear_input_window() {
    pthread_mutex_lock(&screen_mutex);
    werase(input_win);
    box(input_win, 0, 0);
    mvwprintw(input_win, 1, 2, "> ");
    wrefresh(input_win);
    pthread_mutex_unlock(&screen_mutex);
}

// Λογική εισαγωγής χαρακτήρων
int get_user_input(char* buffer) {
    // Timeout 100ms: Αν ο χρήστης δεν πατήσει τίποτα, η wgetch επιστρέφει ERR
    // Αυτό επιτρέπει στο loop να ελέγχει αν ήρθε TERMINATE (in_dialogue == 0)
    wtimeout(input_win, 100); 
    int idx = 0;
    buffer[0] = '\0';
    int ch;
    
    while (in_dialogue) {
        // Σχεδίαση του τρέχοντος buffer
        pthread_mutex_lock(&screen_mutex);
        werase(input_win);
        box(input_win, 0, 0);
        mvwprintw(input_win, 1, 2, "> %s", buffer);
        wrefresh(input_win);
        pthread_mutex_unlock(&screen_mutex);

        ch = wgetch(input_win);
        if (ch == ERR) continue; // Timeout, ξαναέλεγξε το loop

        if (ch == '\n' || ch == '\r') {
            return 1; // Πατήθηκε Enter -> Μήνυμα έτοιμο
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
    return 0; // Ο διάλογος τερματίστηκε
}