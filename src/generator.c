#include <math.h>
#include <time.h>
#include "shared.h"

#define SEM_WMUTEX_NAME "/11775823_sem_wmutex"

typedef struct graph {
    int num_edges;
    int num_vertices;
    char** adj_mat;
    int* vertices;
} graph_t;

static void init_graph(graph_t* const, const char**);
static void init_2D_mat(char*** const, int);
static void print_adj_mat(graph_t* const);
static void set_random_seed(void);
static void exit_error(const char*);
static void map_shared_mem(shm_t** const);
static void open_semaphores(sem_t** const, sem_t** const);

static const char* pgrm_name = NULL;

int main(int argc, const char** argv)
{
    //init 
    pgrm_name = argv[0];
    set_random_seed();

    graph_t g;
    rset_t rs;
    shm_t* shm;
    sem_t* sem_free;
    sem_t* sem_used;
    sem_t* sem_wmutex;

    memset(&g, 0, sizeof(g));
    memset(&rs, 0, sizeof(rs));

    init_graph(&g, argv + 1);
    print_adj_mat(&g);
    map_shared_mem(&shm);
    open_semaphores(&sem_free, &sem_used);

    if ((sem_wmutex = sem_open(SEM_WMUTEX_NAME, O_CREAT, PERM_OWNER_RW, 1)) == SEM_FAILED)
        exit_error("sem_open failed");

    //main loop
    char** adj_mat_buffer = NULL;
    while(shm->state == 0) {
        //assign random color to each vertex        
        for(size_t i = 0; i < g.num_vertices; i++)
            g.vertices[i] = rand() % 3;
        
        //init buffer adjacency matrix only once
        if(adj_mat_buffer == NULL)
            init_2D_mat(&adj_mat_buffer, g.num_vertices);
        
        //restore original state
        memcpy(*adj_mat_buffer, *g.adj_mat, g.num_vertices * g.num_vertices );
        memset(&rs.edges, 0, sizeof(rs.edges));
        rs.num_edges = 0;

        //iterate over adjacency matrix and check for same color of two connected vertices
        for(size_t i = 0; i < g.num_vertices; i++)
        {        
            for(size_t j = 0; j < g.num_vertices; j++)
            {
                //skip if there is no edge between these vertices
                if (adj_mat_buffer[i][j] == 0) continue;

               //if the two vertices have the same color, remove them from adj mat, add to result set
                if (g.vertices[i] == g.vertices[j]) {
                    adj_mat_buffer[i][j] = 0;

                    if (rs.num_edges < MAX_RESULT_EDGES)
                        rs.edges[rs.num_edges++] = ENCODE(i, j);
                }
            }            
        }

        //debug output
        // for(size_t i = 0; i < MAX_RESULT_EDGES; i++)
        // {
        //     if(rs.edges[i] == 0) break;
        //     printf("%d-%d\n", DECODE_U(rs.edges[i]), DECODE_V(rs.edges[i]));
        // }
        // printf("---------\n");

        //write to shared mem
        sem_wait(sem_wmutex);
        sem_wait(sem_free);
        if (shm->state == 0)
            shm->data[shm->write_pos] = rs;
        else break;

        sem_post(sem_used);
        shm->write_pos = (shm->write_pos + 1) % CIRCULAR_BUFFER_SIZE;
        sem_post(sem_wmutex);
    }

    if(munmap(shm, sizeof(shm_t)) < 0)
        exit_error("munmap failed");

    if (sem_close(sem_free) < 0)
        exit_error("sem_close failed");

    if (sem_close(sem_used) < 0)
        exit_error("sem_close failed");

    if (sem_close(sem_wmutex) < 0)
        exit_error("sem_close failed");

    errno = 0;
    if (sem_unlink(SEM_WMUTEX_NAME) < 0 && errno != ENOENT) //no such file or directory (already unlinked)
        exit_error("sem_unlink failed");


    free(g.vertices);
    free(g.adj_mat);
    free(g.adj_mat[0]);



    return EXIT_SUCCESS;
}

static void init_graph(graph_t* const g, const char** pedges) {
    int max_idx = 0;
    int num_edges = 0;
    const char** buffer = pedges;

    // get highest vertex index
    while(*buffer != NULL) {
        char* end;
        //for error checking on strtol
        errno = 0;
        int u = strtol(*buffer, &end, 10);
        int v = strtol(++end, NULL, 10);
        if(errno != 0)
            exit_error("edge parsing error");

        if(u > max_idx || v > max_idx)
            max_idx = u > v ? u : v;

        num_edges++;
        buffer++;
    }

    int num_vertices = max_idx + 1;
    g->num_vertices = num_vertices;
    g->num_edges = num_edges;

    //init vertices for coloring
    if((g->vertices = malloc(num_vertices * sizeof(int))) == NULL)
        exit_error("malloc failed");
    
    memset(g->vertices, 0, num_vertices);

    //init 2D adjacency matrix
    init_2D_mat(&g->adj_mat, num_vertices);

    //fill adjacency matrix
    while(*pedges != NULL) {
        char* end;
        //for error checking on strtol
        errno = 0;
        int u = strtol(*pedges, &end, 10);
        int v = strtol(++end, NULL, 10);
        if(errno != 0)
            exit_error("edge parsing error");

        g->adj_mat[u][v] = 1;
        pedges++;
    }
}

static void print_adj_mat(graph_t* const g) {    
    for(size_t i = 0; i < g->num_vertices; i++)
    {   
        for(size_t j = 0; j < g->num_vertices; j++)
        {
            printf("[%d] ", g->adj_mat[i][j]);
        }
        printf("\n");
    }    
}

static void init_2D_mat(char*** const pmat, int numrows) {
    if((*pmat = malloc(numrows * sizeof(char *))) == NULL) {
        exit_error("malloc failed");
    }        

	if(((*pmat)[0] = malloc(numrows * numrows)) == NULL) {
        exit_error("malloc failed");
    }        
    
    memset((*pmat)[0], 0, numrows * numrows);

	for(int i = 1; i < numrows; i++)
		(*pmat)[i] = (*pmat)[0] + i * numrows;
}

static void set_random_seed(void) {
    struct timespec ts;
    if(clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
        exit_error("clock_gettime failed");

    /* using nano-seconds instead of seconds */
    srand((time_t)ts.tv_nsec);
}

/**
 * Prints an error message and exits with code 1
 * @brief This function prints the error message specified as argument, prints
 * it to stderr with additionally information if errno is set
 * @param[in]   s  error message to be printed
 */
static void exit_error(const char *s)
{
    if (errno == 0)
        fprintf(stderr, "[%s]: %s\n", pgrm_name, s);
    else
        fprintf(stderr, "[%s]: %s, Error: %s\n", pgrm_name, s, strerror(errno));

    exit(EXIT_FAILURE);
}

static void map_shared_mem(shm_t** const pshm) {
    int shmfd;
    if((shmfd = shm_open(SHM_NAME, O_RDWR, PERM_OWNER_R)) < 0)
        exit_error("shm_open failed");

    if(ftruncate(shmfd, sizeof(shm_t)) < 0)
        exit_error("ftruncated failed");

    if((*pshm = mmap(NULL, sizeof(shm_t), PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0)) == MAP_FAILED)
        exit_error("mmap failed");
    
    if(close(shmfd) < 0)
        exit_error("closing shmfd failed");
}

static void open_semaphores(sem_t** const sem_free, sem_t** const sem_used) {
    if ((*sem_free = sem_open(SEM_FREE_NAME, 0)) == SEM_FAILED)
        exit_error("sem_open failed");

    if ((*sem_used = sem_open(SEM_USED_NAME, 0)) == SEM_FAILED)
        exit_error("sem_open failed");
}
