
#ifndef SHARED_H
#define SHARED_H

#define SHM_NAME "/11775823_shm"
#define PERM_OWNER_RW 0600
#define PERM_OWNER_R 0400
#define MAX_EDGE_COUNT 64

typedef struct myshm {
unsigned int state;
unsigned int data[1000];
} myshm_t;

#endif // SHARED_H
