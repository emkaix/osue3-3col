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
    int** adj_mat;
    int* vertices;
} graph_t;

#define ENCODE(u, v) ((int16_t)u << 16) | v
#define DECODE_U(val) val >> 16
#define DECODE_V(val) val & 255

static void init_graph(graph_t*, char**);
static void print_adj_mat(graph_t*);

int main(int argc, char* argv[])
{
    graph_t g;
    memset(&g, 0, sizeof(g));

    init_graph(&g, argv + 1);
    print_adj_mat(&g);


    //assing random color to each vertex
    srand((unsigned int)time(NULL));
    for(size_t i = 0; i < g.num_vertices; i++)
    {
        g.vertices[i] = rand() % 3;
    }
    



    res_set_t rs;
    memset(&rs, 0, sizeof(rs));

    
    for(size_t i = 0; i < g.num_edges; i++)
    {        
        for(size_t j = 0; j < g.num_edges; j++)
        {
            if (g.adj_mat[i][j] == 0) continue;

            //edge exists between vertex i and j
            if (g.vertices[i] == g.vertices[j]) {
                //remove
                g.adj_mat[i][j] = 0;
                //store the result set
                // rs.edges[rs.num_edges++] = (g.vertices[i] << 32) | g.vertices[j];
                rs.edges[rs.num_edges++] = ENCODE(3, 4);

                int t1 = DECODE_U(rs.edges[0]);
                int t2 = DECODE_V(rs.edges[0]);
                
                int a = 3;
            }
        } 
        
    }
    

    // int testrtttt = 2;

    // int shmfd = shm_open("/test_shm", O_RDWR, 0400);
    // if(shmfd == -1) {
    //     strerror(errno);
    //     perror("error shm_open");
    // }

    // ftruncate(shmfd, sizeof(myshm_t));

    // myshm_t* p_mshm = mmap(NULL, sizeof(*p_mshm), PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);

    // if(p_mshm == MAP_FAILED){
    //     strerror(errno);
    //     perror("error map");
    // }

    // close(shmfd);
    // munmap(p_mshm, sizeof(*p_mshm));
    // shm_unlink("/test_shm");



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

    g->vertices = malloc(num_vertices);
    memset(g->vertices, 0, num_vertices);

    //init 2D adjacency matrix
    g->adj_mat = malloc(num_vertices * sizeof(int *));
	g->adj_mat[0] = malloc(num_vertices * num_vertices * sizeof(int));

    //zero allocation
    memset(g->adj_mat[0], 0, num_vertices * num_vertices * sizeof(int));

	for(int i = 1; i < num_vertices; i++)
		g->adj_mat[i] = g->adj_mat[0] + i * num_vertices;

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





