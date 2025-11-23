#ifndef SHM_INIT_H
#define SHM_INIT_H

/* Shared Memory Initialization Functions */
int init_shared_memory(void);
int cleanup_shared_memory(void);
SharedMemory *get_shared_memory(void);

#endif
