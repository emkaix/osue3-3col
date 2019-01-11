/**
 * @file supervisor.c
 * @author Klaus Hahnenkamp <e11775823@student.tuwien.ac.at>
 * @date 10.01.2019
 *
 * @brief Generator program module.
 * 
 * The generator program takes a graph as input. The program repeatedly generates a random solution
 * to the problem as described on the first page and writes its result to the circular buffer. It repeats this
 * procedure until it is notified by the supervisor to terminate.
 *
 **/

#include <math.h>
#include <time.h>
#include "shared.h"

typedef struct graph
{
    int num_edges;                  /*!< number of edges */
    int num_vertices;               /*!< number of vertices */
    char **adj_mat;                 /*!< adjacency matrix */
    int *vertices;                  /*!< pointer to the vertices */
} graph_t;                          /*!< representing the input graph */

static graph_t g;                    /*!< stores the data of the input graph */
static rset_t rs;                    /*!< stores the data of the result set */
static shm_t *shm = NULL;            /*!< pointer to the shared memory */
static sem_t *sem_free = NULL;       /*!< pointer to the semaphore tracking the free space in the ring buffer */
static sem_t *sem_used = NULL;       /*!< pointer to the semaphore tracking the used space in the ring buffer */
static sem_t *sem_wmutex = NULL;     /*!< pointer to the semaphore used for mutex-access to the write end of the ring buffer */
static char **adj_mat_buffer = NULL; /*!< stores a per-iteration copy of the adjacency matrix */
static const char *pgrm_name = NULL; /*!< the program name, set in early stage of execution */

static void init_graph(graph_t *const, const char **);
static void init_2D_mat(char ***const, int);
static void print_adj_mat(graph_t *const);
static void set_random_seed(void);
static void exit_error(const char *);
static void map_shared_mem(shm_t **const);
static void open_semaphores(sem_t **const, sem_t **const, sem_t **const);
static void free_resources(void);

/**
 * Main entry point of the generator program
 * @brief This function sets up the input graph, performs
 * the monte-carlo-algorithm for findinf a 3-colorable solution
 * for the input graph and writes the result sets in the ring buffer
 * @param[in]  argc     argument count
 * @param[in]  argv     argument vector
 * @returns returns     EXIT_SUCCESS
 * @details global variables: g
 * @details global variables: rs
 * @details global variables: shm
 * @details global variables: sem_free
 * @details global variables: sem_used
 * @details global variables: sem_wmutex
 * @details global variables: adj_mat_buffer
 * @details global variables: pgrm_name
 */
int main(int argc, const char **argv)
{
    pgrm_name = argv[0];

    //register exit callback
    if (atexit(free_resources) != 0)
    {
        fprintf(stderr, "[%s]: atexit register failed, Error: %s\n", pgrm_name, strerror(errno));
        exit(EXIT_FAILURE);
    }

    //generator must be started with at least one edge
    if (argc == 1)
    {
        fprintf(stderr, "[%s]: correct usage: generator EDGE1...\n", pgrm_name);
        exit_error("invalid number of arguments");
    }

    //seed is current nanosecond time, so multiple generators started in quick succession have different seeds
    set_random_seed();

    memset(&g, 0, sizeof(g));
    memset(&rs, 0, sizeof(rs));

    //initialize all relevant structures, semaphores, shared memory
    init_graph(&g, argv + 1);
    print_adj_mat(&g);
    map_shared_mem(&shm);
    open_semaphores(&sem_free, &sem_used, &sem_wmutex);
    init_2D_mat(&adj_mat_buffer, g.num_vertices);

    //main loop
    while (shm->state == 0)
    {
        //assign random color to each vertex
        for (size_t i = 0; i < g.num_vertices; i++)
            g.vertices[i] = rand() % 3;

        //restore original state
        memcpy(*adj_mat_buffer, *g.adj_mat, g.num_vertices * g.num_vertices);
        memset(&rs.edges, 0, sizeof(rs.edges));
        rs.num_edges = 0;

        //iterate over adjacency matrix and check for same color of two connected vertices
        for (size_t i = 0; i < g.num_vertices; i++)
        {
            for (size_t j = 0; j < g.num_vertices; j++)
            {
                //skip if there is no edge between these vertices
                if (adj_mat_buffer[i][j] == 0)
                    continue;

                //if the two vertices have the same color, remove them from adj mat, add to result set
                if (g.vertices[i] == g.vertices[j])
                {
                    adj_mat_buffer[i][j] = 0;

                    if (rs.num_edges < MAX_RESULT_EDGES)
                        rs.edges[rs.num_edges++] = ENCODE(i, j);
                }
            }
        }

        //write to shared mem
        sem_wait(sem_wmutex);
        if (shm->state == 1)
        {
            sem_post(sem_wmutex);
            break;
        }
        sem_wait(sem_free);
        if (shm->state == 0)
            shm->data[shm->write_pos] = rs;
        else
            break;

        sem_post(sem_used);
        shm->write_pos = (shm->write_pos + 1) % CIRCULAR_BUFFER_SIZE;
        sem_post(sem_wmutex);
    }

    printf("Generator exits gracefully\n");
    return EXIT_SUCCESS;
}

/**
 * initialize graph
 * @brief This function parses the edge data passed as program arguments and
 * stores the data such as number of vertices, number of edges, highest index in
 * the graph_t pointer
 * @param[out]  g       pointer to a graph to store the data
 * @param[in]   pedges  pointer to the edges passed as program arguments
 */
static void init_graph(graph_t *const g, const char **pedges)
{
    int max_idx = 0;
    int num_edges = 0;
    const char **buffer = pedges;

    // get highest vertex index
    while (*buffer != NULL)
    {
        char *end;
        //for error checking on strtol
        errno = 0;
        int u = strtol(*buffer, &end, 10);
        int v = strtol(++end, NULL, 10);
        if (errno != 0)
            exit_error("edge parsing error");

        if (u > max_idx || v > max_idx)
            max_idx = u > v ? u : v;

        num_edges++;
        buffer++;
    }

    int num_vertices = max_idx + 1;
    g->num_vertices = num_vertices;
    g->num_edges = num_edges;

    //init vertices for coloring
    if ((g->vertices = malloc(num_vertices * sizeof(int))) == NULL)
        exit_error("malloc failed");

    memset(g->vertices, 0, num_vertices);

    init_2D_mat(&g->adj_mat, num_vertices);

    //fill adjacency matrix
    while (*pedges != NULL)
    {
        char *end;
        //for error checking on strtol
        errno = 0;
        int u = strtol(*pedges, &end, 10);
        int v = strtol(++end, NULL, 10);
        if (errno != 0)
            exit_error("edge parsing error");

        g->adj_mat[u][v] = 1;
        pedges++;
    }
}

/**
 * print adjacency matrix
 * @brief This function prints the contents of the adjacency matrix to stdout
 * @param[in]  g    pointer to a graph to store the data
 */
static void print_adj_mat(graph_t *const g)
{
    printf("adjacency matrix:\n");
    for (size_t i = 0; i < g->num_vertices; i++)
    {
        for (size_t j = 0; j < g->num_vertices; j++)
        {
            printf("[%d] ", g->adj_mat[i][j]);
        }
        printf("\n");
    }
}

/**
 * initialize a 2D matrix
 * @brief This function initializes a 2D of given size
 * @param[out]  pmat        reference to a char pointer to store the data
 * @param[in]   numrows     reference to a char pointer to store the data
 */
static void init_2D_mat(char ***const pmat, int numrows)
{
    if ((*pmat = malloc(numrows * sizeof(char *))) == NULL)
    {
        exit_error("malloc failed");
    }

    if (((*pmat)[0] = malloc(numrows * numrows)) == NULL)
    {
        exit_error("malloc failed");
    }

    memset((*pmat)[0], 0, numrows * numrows);

    for (int i = 1; i < numrows; i++)
        (*pmat)[i] = (*pmat)[0] + i * numrows;
}

/**
 * sets random seed
 * @brief This function sets a seed based on current nanosecond time for rand function
 */
static void set_random_seed(void)
{
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0)
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

    free_resources();
    exit(EXIT_FAILURE);
}

/**
 * map shared memory
 * @brief This function maps a shared memory into the processes virtual adress space
 * @param[in]   pshm    a reference to a pshm pointer
 */
static void map_shared_mem(shm_t **const pshm)
{
    int shmfd;
    if ((shmfd = shm_open(SHM_NAME, O_RDWR, PERM_OWNER_R)) < 0)
        exit_error("shm_open failed");

    if (ftruncate(shmfd, sizeof(shm_t)) < 0)
        exit_error("ftruncated failed");

    if ((*pshm = mmap(NULL, sizeof(shm_t), PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0)) == MAP_FAILED)
        exit_error("mmap failed");

    if (close(shmfd) < 0)
        exit_error("closing shmfd failed");
}

/**
 * open semaphores
 * @brief This function opens the semaphores used for synchronization set up by supervisor
 * @param[out]  sem_free    a reference to the sem_free pointer
 * @param[out]  sem_used    a reference to a sem_used pointer
 * @param[out]  sem_wmutex  a reference to a sem_wmutex pointer
 */
static void open_semaphores(sem_t **const sem_free, sem_t **const sem_used, sem_t **const sem_wmutex)
{
    if ((*sem_free = sem_open(SEM_FREE_NAME, 0)) == SEM_FAILED)
        exit_error("sem_open failed");

    if ((*sem_used = sem_open(SEM_USED_NAME, 0)) == SEM_FAILED)
        exit_error("sem_open failed");

    if ((*sem_wmutex = sem_open(SEM_WMUTEX_NAME, 0)) == SEM_FAILED)
        exit_error("sem_open failed");
}

/**
 * delete all resources used
 * @brief This function unregisters and deletes all allocated resources
 */
static void free_resources(void)
{
    if (munmap(shm, sizeof(shm_t)) < 0)
        fprintf(stderr, "[%s]: munmmap failed, Error: %s\n", pgrm_name, strerror(errno));

    if (sem_close(sem_free) < 0)
        fprintf(stderr, "[%s]: sem_close failed, Error: %s\n", pgrm_name, strerror(errno));

    if (sem_close(sem_used) < 0)
        fprintf(stderr, "[%s]: sem_close failed, Error: %s\n", pgrm_name, strerror(errno));

    if (sem_close(sem_wmutex) < 0)
        fprintf(stderr, "[%s]: sem_close failed, Error: %s\n", pgrm_name, strerror(errno));

    free(g.vertices);
    free(g.adj_mat[0]);
    free(g.adj_mat);
    free(adj_mat_buffer[0]);
    free(adj_mat_buffer);
}
