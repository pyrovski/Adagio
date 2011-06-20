#ifndef SHM_H
#define SHM_H

typedef struct {
  unsigned count;
  // list of int ranks follows...
} shm_ranks_t;

extern MPI_Comm comm_socket;

int shm_setup(char ** argv, int rank);
int shm_teardown();
#endif
