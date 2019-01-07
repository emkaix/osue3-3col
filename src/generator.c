#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <math.h>

#include "shared.h"

typedef struct graph {
    int num_edges;
    int num_vertices;
    int** adj_mat;
    int* vertices;
} graph_t;

static void init_graph(graph_t*, char**);

int main(int argc, char* argv[])
{
    int edges_cnt = argc - 1;
    char* edges[edges_cnt];
    memset(edges, 0, sizeof(char*) * edges_cnt);
    
    for(size_t i = 0; i < edges_cnt; i++)
    {
        edges[i] = argv[i + 1];
    }
    
    graph_t g;
    memset(&g, 0, sizeof(g));

    init_graph(&g, argv + 1);

    int t = 2;

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

    while(*pedges != NULL) {
        char* end;
        long int u = strtol(*pedges, &end, 10);
        long int v = strtol(++end, NULL, 10);

        if(u > max_idx || v > max_idx)
            max_idx = u > v ? u : v;

        num_edges++;
        pedges++;
    }

    g->num_edges = num_edges;

    //init vertices array
    g->num_vertices = max_idx + 1;
    g->vertices = malloc(g->num_vertices);

    //init 2D adjacency matrix
    g->adj_mat = malloc(max_idx * sizeof(int *));
	g->adj_mat[0] = malloc(max_idx * max_idx * sizeof(int));
	for(int i = 1; i < max_idx; i++)
		g->adj_mat[i] = g->adj_mat[0] + i * max_idx;

}





