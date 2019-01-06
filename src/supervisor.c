#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include "shared.h"


static char* pgrm_name = NULL;

static void exit_error(const char*);

int main(int argc, char** argv)
{
    pgrm_name = argv[0];

    int shmfd = shm_open(SHM_NAME, O_RDWR | O_CREAT, PERM_OWNER_RW);
    if(shmfd == -1) {
        strerror(errno);
        perror("error shm_open");
    }

    ftruncate(shmfd, sizeof(myshm_t));

    myshm_t* p_mshm = mmap(NULL, sizeof(*p_mshm), PROT_READ | PROT_WRITE, MAP_SHARED, shmfd, 0);

    if(p_mshm == MAP_FAILED){
        strerror(errno);
        perror("error map");
    }

    close(shmfd);


    p_mshm->data[0] = 1234;


    munmap(p_mshm, sizeof(*p_mshm));
    shm_unlink(SHM_NAME);



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