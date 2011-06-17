#ifndef SHM_H
#define SHM_H

typedef struct {
  unsigned count;
  // list of int ranks follows...
} shm_ranks_t;


int shm_setup(char ** argv, unsigned socket, int rank);
int shm_teardown();
#endif
