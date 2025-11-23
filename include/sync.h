#ifndef SYNC_H
#define SYNC_H

/* Synchronization Functions */
int init_sync(void);
void lock_shared_memory(void);
void unlock_shared_memory(void);
int cleanup_sync(void);

#endif
