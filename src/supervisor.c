/**
 * @file supervisor.c
 * @author Klaus Hahnenkamp <e11775823@student.tuwien.ac.at>
 * @date 10.01.2019
 *
 * @brief Supervisor program module.
 * 
 * The supervisor sets up the shared memory and the semaphores and initializes the circular buffer required
 * for the communication with the generators. It then waits for the generators to write solutions to the
 * circular buffer.
 *
 **/

#include <signal.h>
#include <stdbool.h>
#include <limits.h>
#include "shared.h"

typedef struct sigaction sigaction_t;           /*!< used for registering a signal callback function */

static volatile bool should_terminate = false;  /*!< when set via signal callback, the supervisor advises generators to terminate and terminates itself */
static const char *pgrm_name = NULL;            /*!< the program name, set in early stage of execution */
static sem_t *sem_free = NULL;                  /*!< pointer to the semaphore tracking the free space in the ring buffer */
static sem_t *sem_used = NULL;                  /*!< pointer to the semaphore tracking the used space in the ring buffer */
static sem_t *sem_wmutex = NULL;                /*!< pointer to the semaphore used for mutex-access to the write end of the ring buffer */
static shm_t *shm = NULL;                       /*!< pointer to the shared memory */

static void handle_signal(int);
static void exit_error(const char *);
static void create_shared_mem(shm_t **const);
static void create_semaphores(sem_t **const, sem_t **const, sem_t **const);
static void init_signal_handling(sigaction_t *const);
static void free_resources(void);

/**
 * Main entry point of the supervisor program
 * @brief This function sets up necessary resources, e.g signal handling,
 * shared memory, semaphores and reads the result data out of the ring buffer
 * @param[in]  argc     argument count
 * @param[in]  argv     argument vector
 * @returns returns     EXIT_SUCCESS
 * @details global variables: pgrm_name
 * @details global variables: should_terminate
 * @details global variables: sem_free
 * @details global variables: sem_used
 * @details global variables: sem_wmutex
 * @details global variables: shm
 */
int main(int argc, const char **argv)
{
    pgrm_name = argv[0];

    if (atexit(free_resources) != 0)
    {
        fprintf(stderr, "[%s]: atexit register failed, Error: %s\n", pgrm_name, strerror(errno));
        exit(EXIT_FAILURE);
    }

    //program must be executed without any options or arguments per synopsis
    if (argc != 1)
    {
        fprintf(stderr, "[%s]: correct usage: supervisor\n", pgrm_name);
        exit_error("invalid number of arguments");
    }

    //signal handler is executed whenever SIGINT or SIGTERM occurs
    sigaction_t sa;
    init_signal_handling(&sa);

    //init shared memory
    create_shared_mem(&shm);
    shm->state = 0;
    shm->write_pos = 0;

    //init semaphore
    create_semaphores(&sem_free, &sem_used, &sem_wmutex);

    //main loop
    int read_pos = 0;
    rset_t best_rset;
    best_rset.num_edges = INT_MAX;

    while (!should_terminate)
    {
        if (sem_wait(sem_used) < 0)
        {
            if (errno == EINTR)
                continue;
            exit_error("sem_wait failed");
        }

        rset_t cur_rset = shm->data[read_pos];

        //graph is acyclic, no edges need to be removed
        if (cur_rset.num_edges == 0)
        {
            printf("The graph is 3-colorable!\n");
            shm->state = 1;
            break;
        }

        //print better solutions
        if (cur_rset.num_edges < best_rset.num_edges)
        {
            best_rset = cur_rset;
            printf("Solution with %d edges: ", best_rset.num_edges);

            for (size_t i = 0; i < best_rset.num_edges; i++)
            {
                int u = DECODE_U(best_rset.edges[i]);
                int v = DECODE_V(best_rset.edges[i]);
                printf("%d-%d ", u, v);
            }
            printf("\n");
        }

        sem_post(sem_free);
        read_pos = (read_pos + 1) % CIRCULAR_BUFFER_SIZE;
    }

    sem_post(sem_free);
    shm->state = 1;

    printf("Supervisor exits gracefully\n");
    return EXIT_SUCCESS;
}

/**
 * Prints an error message and exits with code 1
 * @brief This function prints the error message specified as argument, prints
 * it to stderr with additionally information if errno is set
 * @param[in]   s   error message to be printed
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
 * Signal callback
 * @brief This function is executed whenever the program receives a SIGTERM or SIGINT signal
 * @param[in]  signal   the signal to be handled
 */
static void handle_signal(int signal)
{
    should_terminate = true;
}

/**
 * create the shared memory
 * @brief This function initializes shared memory used for communication between the supervisor and the generators
 * @param[out]  pshm    a reference to a shm_t pointer
 */
static void create_shared_mem(shm_t **const pshm)
{
    int shmfd;
    if ((shmfd = shm_open(SHM_NAME, O_RDWR | O_CREAT | O_EXCL, PERM_OWNER_RW)) < 0)
        exit_error("shm_open failed");

    if (ftruncate(shmfd, sizeof(shm_t)) < 0)
        exit_error("ftruncated failed");

    *pshm = mmap(NULL, sizeof(shm_t), PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);

    if (*pshm == MAP_FAILED)
        exit_error("mmap failed");

    if (close(shmfd) < 0)
        exit_error("close fd failed");
}

/**
 * create semaphores
 * @brief This function initializes semaphores used for synchronization between the supervisor and the generators
 * @param[out]  sem_free    a reference to the sem_free pointer
 * @param[out]  sem_used    a reference to a sem_used pointer
 * @param[out]  sem_wmutex  a reference to a sem_wmutex pointer
 */
static void create_semaphores(sem_t **const sem_free, sem_t **const sem_used, sem_t **const sem_wmutex)
{
    if ((*sem_free = sem_open(SEM_FREE_NAME, O_CREAT | O_EXCL, PERM_OWNER_RW, CIRCULAR_BUFFER_SIZE)) == SEM_FAILED)
        exit_error("sem_open failed");

    if ((*sem_used = sem_open(SEM_USED_NAME, O_CREAT | O_EXCL, PERM_OWNER_RW, 0)) == SEM_FAILED)
        exit_error("sem_open failed");

    if ((*sem_wmutex = sem_open(SEM_WMUTEX_NAME, O_CREAT | O_EXCL, PERM_OWNER_RW, 1)) == SEM_FAILED)
        exit_error("sem_open failed");
}

/**
 * initializes signal handling
 * @brief This function registers the signal handling callback which is executed whenever the application receives a SIGTERM or SIGINT signal
 * @param[out]  sem_free    a reference to a sigaction_t struct
 */
static void init_signal_handling(sigaction_t *const sa)
{
    memset(sa, 0, sizeof(sigaction_t));
    sa->sa_handler = handle_signal;
    if (sigaction(SIGINT, sa, NULL) < 0)
        exit_error("sigaction failed");
    if (sigaction(SIGTERM, sa, NULL) < 0)
        exit_error("sigaction failed");
}

/**
 * delete all resources used
 * @brief This function unregisters and deletes all allocated resources
 */
static void free_resources(void)
{
    if (munmap(shm, sizeof(shm_t)) < 0)
        fprintf(stderr, "[%s]: munmmap failed, Error: %s\n", pgrm_name, strerror(errno));

    if (shm_unlink(SHM_NAME) < 0)
        fprintf(stderr, "[%s]: shm_unlink failed, Error: %s\n", pgrm_name, strerror(errno));

    if (sem_close(sem_free) < 0)
        fprintf(stderr, "[%s]: sem_close failed, Error: %s\n", pgrm_name, strerror(errno));

    if (sem_close(sem_used) < 0)
        fprintf(stderr, "[%s]: sem_close failed, Error: %s\n", pgrm_name, strerror(errno));

    if (sem_close(sem_wmutex) < 0)
        fprintf(stderr, "[%s]: sem_close failed, Error: %s\n", pgrm_name, strerror(errno));

    if (sem_unlink(SEM_FREE_NAME) < 0)
        fprintf(stderr, "[%s]: sem_unlink failed, Error: %s\n", pgrm_name, strerror(errno));

    if (sem_unlink(SEM_USED_NAME) < 0)
        fprintf(stderr, "[%s]: sem_unlink failed, Error: %s\n", pgrm_name, strerror(errno));

    if (sem_unlink(SEM_WMUTEX_NAME) < 0 && errno != ENOENT) //no such file or directory (already unlinked)
        fprintf(stderr, "[%s]: sem_unlink failed, Error: %s\n", pgrm_name, strerror(errno));
}
