#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <math.h>
#include <time.h>


#include "shared.h"

typedef struct graph {
    int num_edges;
    int num_vertices;
    char** adj_mat;
    int* vertices;
} graph_t;

#define ENCODE(u, v) (((int16_t)u << 16) | v)
#define DECODE_U(val) (val >> 16)
#define DECODE_V(val) (val & 255)

static void init_graph(graph_t*, char**);
static void init_2D_mat(char***, int);
static void print_adj_mat(graph_t*);
static void set_random_seed(void);
static void exit_error(const char*);
static void map_shared_mem(myshm_t**);

static char* pgrm_name = NULL;

int main(int argc, char* argv[])
{
    pgrm_name = argv[0];
    graph_t g;
    memset(&g, 0, sizeof(g));

    init_graph(&g, argv + 1);
    // print_adj_mat(&g);

    set_random_seed();    

    myshm_t* shm;
    map_shared_mem(&shm);

    res_set_t rs;
    memset(&rs, 0, sizeof(rs));

    size_t count = 1000000000000000000;

    //main loop generator
    char** adj_mat_buffer = NULL;
    while(count-- > 0) {
        //assign random color to each vertex
        
        for(size_t i = 0; i < g.num_vertices; i++)
            g.vertices[i] = rand() % 3;
        
        //restore original adj matrix for new run
        // int adj_mat_buffer[g.num_vertices][g.num_vertices];
        // char** adj_mat_buffer;
        if(adj_mat_buffer == NULL)
            init_2D_mat(&adj_mat_buffer, g.num_vertices);
        
        memcpy(*adj_mat_buffer, *g.adj_mat, g.num_vertices * g.num_vertices );


        // printf("----\n");
        // graph_t gtest;
        // gtest.num_vertices = g.num_vertices;
        // gtest.adj_mat = adj_mat_buffer;
        // print_adj_mat(&gtest);


        for(size_t i = 0; i < g.num_vertices; i++)
        {        
            for(size_t j = 0; j < g.num_vertices; j++)
            {
                if (adj_mat_buffer[i][j] == 0) continue;

                //edge exists between vertex i and j
                if (g.vertices[i] == g.vertices[j]) {
                    //remove
                    adj_mat_buffer[i][j] = 0;
                    //store the result set

                    if(rs.num_edges < MAX_RESULT_EDGES)
                    rs.edges[rs.num_edges++] = ENCODE(i, j);
                }
            } 
            
        }

        //graph is acyclic, no edges need to be removed
        if(rs.num_edges == 0) {
            printf("graph is acyclic\n");
            break;
        }

        //debug output
        // for(size_t i = 0; i < MAX_RESULT_EDGES; i++)
        // {
        //     if(rs.edges[i] == 0) break;
        //     printf("%d-%d\n", DECODE_U(rs.edges[i]), DECODE_V(rs.edges[i]));
        // }
        // printf("---------\n");

        //write to shared mem
        memcpy(&(shm->data[0]), &rs, sizeof(res_set_t));
        //clean local buffer;
        memset(&rs.edges, 0, sizeof(rs.edges));
        rs.num_edges = 0;
    }



    // close(shmfd);
    munmap(shm, sizeof(myshm_t));
    // shm_unlink(SHM_NAME);


    free(g.vertices);
    free(g.adj_mat);
    free(g.adj_mat[0]);
    return EXIT_SUCCESS;
}

static void init_graph(graph_t* g, char** pedges) {
    //parse edges, get highest vertex index
    long int max_idx = 0;
    int num_edges = 0;

    char** buffer = pedges;
    // get highest vertex index
    while(*buffer != NULL) {
        char* end;
        long int u = strtol(*buffer, &end, 10);
        long int v = strtol(++end, NULL, 10);

        if(u > max_idx || v > max_idx)
            max_idx = u > v ? u : v;

        num_edges++;
        buffer++;
    }
    
    g->num_edges = num_edges;

    //init vertices array
    int num_vertices = max_idx + 1;
    g->num_vertices = num_vertices;

    g->vertices = malloc(num_vertices * sizeof(int));
    memset(g->vertices, 0, num_vertices);

    //init 2D adjacency matrix
    init_2D_mat(&g->adj_mat, num_vertices);

    //fill adjacency matrix
    while(*pedges != NULL) {
        char* end;
        long int u = strtol(*pedges, &end, 10);
        long int v = strtol(++end, NULL, 10);

        g->adj_mat[u][v] = 1;
        pedges++;
    }

}

static void print_adj_mat(graph_t* g) {    
    for(size_t i = 0; i < g->num_vertices; i++)
    {   
        for(size_t j = 0; j < g->num_vertices; j++)
        {
            printf("[%d] ", g->adj_mat[i][j]);
        }
        printf("\n");
    }
    
}

static void init_2D_mat(char*** pmat, int numrows) {
    *pmat = malloc(numrows * sizeof(char *));
    if(*pmat == NULL) exit_error("malloc failed");
	(*pmat)[0] = malloc(numrows * numrows );
    if((*pmat)[0] == NULL) exit_error("malloc failed");

    //zero allocation
    memset((*pmat)[0], 0, numrows * numrows );

	for(int i = 1; i < numrows; i++)
		(*pmat)[i] = (*pmat)[0] + i * numrows;
}

static void set_random_seed(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

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

static void map_shared_mem(myshm_t** pshm) {
    int shmfd = shm_open(SHM_NAME, O_RDWR, 0400);
    if(shmfd == -1) {
        strerror(errno);
        perror("error shm_open");
    }

    ftruncate(shmfd, sizeof(myshm_t));

    *pshm = mmap(NULL, sizeof(myshm_t), PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);

    if(*pshm == MAP_FAILED){
        strerror(errno);
        perror("error map");
    }

}

