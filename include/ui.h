#ifndef UI_H
#define UI_H

// Αρχικοποίηση γραφικού περιβάλλοντος
void init_ui(void);

// Τερματισμός γραφικού περιβάλλοντος
void cleanup_ncurses(void);

// Εγγραφή μηνύματος στο παράθυρο chat με χρώμα
void write_to_chat(const char* username, const char* msg, int color_pair);

// Καθαρισμός του παραθύρου εισαγωγής κειμένου
void clear_input_window(void);

// Διαβάζει είσοδο χρήστη. Επιστρέφει 1 αν πατήθηκε Enter, 0 αν έγινε timeout
int get_user_input(char* buffer);

#endif