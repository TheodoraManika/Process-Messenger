#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "messaging.h"
#include "shm_init.h"
#include "dialogue.h"
#include "message.h"
#include "sync.h"

static int current_dialogue = -1;
static pid_t my_pid;
static int running = 1;

void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        running = 0;
        printf("\nReceived termination signal. Exiting...\n");
    }
}

void print_menu(void) {
    printf("\n========== Process Messenger ==========\n");
    printf("1. Create/Join Dialogue\n");
    printf("2. Send Message\n");
    printf("3. Check Messages\n");
    printf("4. Terminate Dialogue\n");
    printf("5. Exit\n");
    printf("======================================\n");
    printf("Choose an option: ");
}

int main(void) {
    int choice;
    char input[MAX_MESSAGE_SIZE];
    int dialogue_id;
    Message msg;

    my_pid = getpid();

    /* Register signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    /* Initialize shared memory */
    if (init_shared_memory() == -1) {
        fprintf(stderr, "Failed to initialize shared memory\n");
        exit(EXIT_FAILURE);
    }

    printf("Process %d started\n", my_pid);

    /* Main loop */
    while (running) {
        print_menu();

        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }

        choice = atoi(input);

        switch (choice) {
            case 1:  /* Create/Join Dialogue */
                printf("Enter dialogue ID (positive integer): ");
                if (fgets(input, sizeof(input), stdin) == NULL) break;
                dialogue_id = atoi(input);

                if (dialogue_id <= 0) {
                    printf("Invalid dialogue ID\n");
                    break;
                }

                if (create_or_join_dialogue(dialogue_id) == -1) {
                    printf("Failed to create/join dialogue\n");
                } else {
                    current_dialogue = dialogue_id;
                    printf("Dialogue %d is now active\n", dialogue_id);
                }
                break;

            case 2:  /* Send Message */
                if (current_dialogue == -1) {
                    printf("No active dialogue. Join a dialogue first.\n");
                    break;
                }

                printf("Enter message (max %d chars): ", MAX_MESSAGE_SIZE - 1);
                if (fgets(input, sizeof(input), stdin) == NULL) break;

                /* Remove newline */
                size_t len = strlen(input);
                if (len > 0 && input[len - 1] == '\n') {
                    input[len - 1] = '\0';
                }

                if (send_message(current_dialogue, my_pid, input) == 0) {
                    printf("Message sent: %s\n", input);
                    
                    /* Check if this is a TERMINATE message */
                    if (strcmp(input, "TERMINATE") == 0) {
                        printf("Terminating dialogue...\n");
                        terminate_dialogue(current_dialogue, my_pid);
                        current_dialogue = -1;
                        printf("Dialogue terminated. Exiting...\n");
                        running = 0;
                    }
                } else {
                    printf("Failed to send message\n");
                }
                break;

            case 3:  /* Check Messages */
                if (current_dialogue == -1) {
                    printf("No active dialogue.\n");
                    break;
                }

                printf("Checking for new messages in dialogue %d...\n", current_dialogue);
                
                int found = 0;
                while (receive_message(current_dialogue, my_pid, &msg) == 1) {
                    found = 1;
                    printf("Message from PID %d: %s\n", msg.sender_pid, msg.payload);
                    
                    /* Check if this is a TERMINATE message */
                    if (strcmp(msg.payload, "TERMINATE") == 0) {
                        printf("Dialogue terminated by another process.\n");
                        terminate_dialogue(current_dialogue, my_pid);
                        current_dialogue = -1;
                        printf("Exiting...\n");
                        running = 0;
                        break;
                    }
                }

                if (!found) {
                    printf("No new messages.\n");
                }
                break;

            case 4:  /* Terminate Dialogue */
                if (current_dialogue == -1) {
                    printf("No active dialogue.\n");
                    break;
                }

                printf("Sending TERMINATE message...\n");
                if (send_message(current_dialogue, my_pid, "TERMINATE") == 0) {
                    terminate_dialogue(current_dialogue, my_pid);
                    current_dialogue = -1;
                    printf("Dialogue terminated. Exiting...\n");
                    running = 0;
                } else {
                    printf("Failed to send TERMINATE message\n");
                }
                break;

            case 5:  /* Exit */
                if (current_dialogue != -1) {
                    printf("Please terminate the active dialogue first.\n");
                } else {
                    running = 0;
                }
                break;

            default:
                printf("Invalid choice. Try again.\n");
        }
    }

    /* Cleanup */
    cleanup_shared_memory();
    printf("Process %d exiting\n", my_pid);

    return EXIT_SUCCESS;
}
