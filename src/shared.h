
#ifndef SHARED_H
#define SHARED_H

#define SHM_NAME "/11775823_shm"
#define PERM_OWNER_RW 0600
#define PERM_OWNER_R 0400
#define MAX_EDGE_COUNT 64
#define MAX_RESULT_EDGES 8

typedef struct res_set {
    int num_edges;
    int32_t edges[MAX_RESULT_EDGES];
} res_set_t;

typedef struct myshm {
    unsigned int state;
    res_set_t data[1000];
} myshm_t;



#endif // SHARED_H
