#ifndef CHAT_H
#define CHAT_H

// Η συνάρτηση που τρέχει στο thread λήψης μηνυμάτων
void* receiver_thread(void* arg);

// Στέλνει ένα μήνυμα στη διαμοιραζόμενη μνήμη
void send_message_to_shm(const char* payload);

#endif