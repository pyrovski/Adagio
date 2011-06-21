#ifndef SHM_H
#define SHM_H

#define MAX_CORES_PER_SOCKET 8

/*
typedef struct {
  unsigned count;
  // list of int ranks follows...
} shm_ranks_t;
*/

extern MPI_Comm comm_socket;

int shm_setup(char ** argv, int rank);
int shm_teardown();

int shm_mark_freq(unsigned freq_idx);
#endif
