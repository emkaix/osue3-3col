/**
 * @file shared.h
 * @author Klaus Hahnenkamp <e11775823@student.tuwien.ac.at>
 * @date 10.01.2019
 *
 * @brief Shared header file
 * 
 * This header contains shared includes, macros and typedefs commonly used by supervisor and generator
 *
 **/

#ifndef SHARED_H
#define SHARED_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <semaphore.h>

#define SHM_NAME "/11775823_shm"                /*!< the name of the shared memory region used by the supervisor and generator */
#define SEM_FREE_NAME "/11775823_sem_free"      /*!< the name of the semaphore that keeps track of free space in the ring buffer */
#define SEM_USED_NAME "/11775823_sem_used"      /*!< the name of the semaphore that keeps track of used space in the ring buffer */
#define SEM_WMUTEX_NAME "/11775823_sem_wmutex"  /*!< the name of the semaphore that provides mutex-access to the write end of the ring buffer */

#define PERM_OWNER_RW (0600)                    /*!< read/write permission */
#define PERM_OWNER_R (0400)                     /*!< read only permission */
#define MAX_RESULT_EDGES (8)                    /*!< maximum number of removed edges to be written to the ring buffer */
#define CIRCULAR_BUFFER_SIZE (100)              /*!< size of the ringbuffer as elements of type rset */

#define DECODE_U(val) (val >> 16)               /*!< parses the first vertex of a single edge */
#define DECODE_V(val) (val & 255)               /*!< parses the second vertex of a single edge */
#define ENCODE(u, v) (((int16_t)u << 16) | v)   /*!< encodes two 16 bit vertex indices as a single 32 bit integer */

typedef struct rset
{
    int num_edges;                              /*!< number of edges the result set holds */
    int32_t edges[MAX_RESULT_EDGES];            /*!< an array that holds the edges removed from the graph */
} rset_t;                                       /*!< result set holds the removed edges of a graph to be written to the ring buffer */

typedef struct shm
{
    unsigned int state;                         /*!< indicating whether all processes should terminate */
    int write_pos;                              /*!< current writing position in the ring buffer */
    rset_t data[CIRCULAR_BUFFER_SIZE];          /*!< the ring buffer */
} shm_t;                                        /*!< describes the memory layout of the shared memory region */

#endif // SHARED_H
