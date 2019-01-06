#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

typedef struct myshm {
unsigned int state;
unsigned int data[1000];
} myshm_t;

int main(int argc, char const *argv[])
{
    int shmfd = shm_open("/test_shm", O_RDWR, 0400);
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
    munmap(p_mshm, sizeof(*p_mshm));
    shm_unlink("/test_shm");



    return EXIT_SUCCESS;
}