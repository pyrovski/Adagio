#ifndef SHM_H
#define SHM_H
#include <mpi.h>

#define MAX_CORES_PER_SOCKET 8

/*
typedef struct {
  unsigned count;
  // list of int ranks follows...
} shm_ranks_t;
*/

extern MPI_Comm comm_socket;
extern int socket_rank;
extern int socket_size;

/*
int shm_setup(char ** argv, int rank);
int shm_teardown();

int shm_mark_freq(int freq_idx);
*/
#endif
