#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include "shared.h"


static char* pgrm_name = NULL;

static void exit_error(const char*);

typedef struct addrinfo addrinfo_t;             /*!< typedef for the standard addrinfo struct */
typedef struct sigaction sigaction_t;           /*!< used for registering a signal callback function */
static void handlesignal(int);
static volatile bool should_terminate = false;  /*!< when set via signal callback, the server closes gracefully */


int main(int argc, char** argv)
{
    pgrm_name = argv[0];

    //signal handler is executed whenever SIGINT or SIGTERM occurs
    sigaction_t sa;
    memset(&sa, 0, sizeof(sigaction_t));
    sa.sa_handler = handlesignal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    int shmfd = shm_open(SHM_NAME, O_RDWR | O_CREAT, PERM_OWNER_RW);
    if(shmfd < 0)
        exit_error("shm_open failed");

    if(ftruncate(shmfd, sizeof(myshm_t)) < 0)
        exit_error("ftruncated failed");

    myshm_t* p_mshm = mmap(NULL, sizeof(*p_mshm), PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);

    if(p_mshm == MAP_FAILED)
        exit_error("mmap failed");

    if(close(shmfd) < 0)
        exit_error("close fd failed");


    //main loop
    while(!should_terminate) {
        int dummy = 3;
    }

    munmap(p_mshm, sizeof(*p_mshm));
    shm_unlink(SHM_NAME);


    printf("exit gracefully\n");
    return EXIT_SUCCESS;
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

static void handlesignal(int signal) {
    should_terminate = true;
}


