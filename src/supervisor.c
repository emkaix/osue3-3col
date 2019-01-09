#include <signal.h>
#include <stdbool.h>
#include "shared.h"

typedef struct addrinfo addrinfo_t;             /*!< typedef for the standard addrinfo struct */
typedef struct sigaction sigaction_t;           /*!< used for registering a signal callback function */

static void handlesignal(int);
static void exit_error(const char*);

static volatile bool should_terminate = false;  /*!< when set via signal callback, the server closes gracefully */
static const char* pgrm_name = NULL;

int main(int argc, const char** argv)
{
    pgrm_name = argv[0];

    //signal handler is executed whenever SIGINT or SIGTERM occurs
    sigaction_t sa;
    memset(&sa, 0, sizeof(sigaction_t));
    sa.sa_handler = handlesignal;
    if(sigaction(SIGINT, &sa, NULL) < 0)
        exit_error("sigaction failed");
    if(sigaction(SIGTERM, &sa, NULL) < 0)
        exit_error("sigaction failed");

    int shmfd;
    if((shmfd = shm_open(SHM_NAME, O_RDWR | O_CREAT | O_EXCL, PERM_OWNER_RW)) < 0)
        exit_error("shm_open failed");

    if(ftruncate(shmfd, sizeof(shm_t)) < 0)
        exit_error("ftruncated failed");

    shm_t* p_mshm = mmap(NULL, sizeof(*p_mshm), PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);

    if(p_mshm == MAP_FAILED)
        exit_error("mmap failed");

    if(close(shmfd) < 0)
        exit_error("close fd failed");

    //main loop
    while(!should_terminate) {
        int dummy = 3;
    }

    if(munmap(p_mshm, sizeof(*p_mshm)) < 0)
        exit_error("munmap failed");
        
    if(shm_unlink(SHM_NAME) < 0)
        exit_error("shm_unlink failed");


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


