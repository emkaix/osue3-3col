
#ifndef SHARED_H
#define SHARED_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <semaphore.h>

#define SHM_NAME "/11775823_shm"
#define SEM_FREE_NAME "/11775823_sem_free"
#define SEM_USED_NAME "/11775823_sem_used"

#define PERM_OWNER_RW (0600)
#define PERM_OWNER_R (0400)
#define MAX_EDGE_COUNT (64)
#define MAX_RESULT_EDGES (8)
#define CIRCULAR_BUFFER_SIZE (100)

#define DECODE_U(val) (val >> 16)
#define DECODE_V(val) (val & 255)
#define ENCODE(u, v) (((int16_t)u << 16) | v)

typedef struct rset {
    int num_edges;
    int32_t edges[MAX_RESULT_EDGES];
} rset_t;

typedef struct shm {
    unsigned int state;
    rset_t data[CIRCULAR_BUFFER_SIZE];
} shm_t;



#endif // SHARED_H
