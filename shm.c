#include <assert.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <dirent.h>
#include <semaphore.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>
#include <mpi.h>
#include <stdint.h>
#include "cpuid.h"
#include "md5.h"
#include "shm.h"
#include "shift.h"

/*! This represents selected frequencies for each core on a socket 
  for a given time period.
  Reference the freq_index[] array by config.map_core_to_per_socket_core[core]
 */
typedef struct{
  int8_t freq_index[MAX_CORES_PER_SOCKET]; // 0-based frequency index
  struct timeval tStart;
} freq_select_t;

MPI_Comm comm_socket;
int socket_rank;
int socket_size;

static int shm_fd = -1;
static sem_t *sem = 0;
static char shm_name[NAME_MAX - 4];

/*! @todo how does ftruncate affect memory-mapped shared objects? 
  @todo error checking
  This function would be useful if we wanted to void a collective operation
  or two (a global barrier and MPI_Comm_split).  We can always use it later.
 */
/*
static int shm_update(){
  assert(shm_fd >= 0);
  assert(sem);
  int status;
  struct stat shm_stat;
  unsigned count = 1;
  status = sem_wait(sem);
  status = lseek(shm_fd, 0, SEEK_SET);
  //! @todo if size 0, truncate to sizeof(unsigned)
  status = fstat(shm_fd, &shm_stat);
  if(shm_stat.st_size <= sizeof(unsigned)){ // I am first
    status = ftruncate(shm_fd, sizeof(unsigned) + count * sizeof(int));
    status = write(shm_fd, &count, sizeof(unsigned));
    status = write(shm_fd, &rank, sizeof(int));
  } else { // I am not first
    status = read(shm_fd, &count, sizeof(unsigned));
    #ifdef _DEBUG
    printf("rank %d arrived at position %u\n", rank, count);
    #endif
    count++;
    status = ftruncate(shm_fd, sizeof(unsigned) + count * sizeof(int));
    status = lseek(shm_fd, 0, SEEK_SET);
    status = write(shm_fd, &count, sizeof(count));
    status = lseek(shm_fd, sizeof(unsigned) + (count-1) * sizeof(int), 
		   SEEK_SET);
    status = write(shm_fd, &rank, sizeof(int));
  }
  status = sem_post(sem);

  return 0;
}
*/

int shm_setup(char **argv, int rank){
  char *runName;
  int status;

  assert(argv && argv[0]);
  
  // use part of name after last '/'
  runName = strrchr(argv[0], '/');
  runName = runName ? runName + 1 : argv[0];
  runName = *runName ? runName : argv[0];
  
  status = get_cpuid(&my_core, &my_socket, &my_local);

  status = snprintf(shm_name, NAME_MAX - 4, "/%s.%u", runName, my_socket);
  shm_fd = shm_open(shm_name, O_RDWR | O_CREAT, S_IRWXU);
  if(shm_fd < 0){
    perror("shm_open");
    exit(1);
  }

  sem = sem_open(shm_name, O_CREAT, S_IRWXU, 1);
  if(sem == SEM_FAILED){
    perror("sem_open");
    exit(1);
  }

  // alternatively, shm_update() could be used here
  PMPI_Barrier(MPI_COMM_WORLD);

  MPI_Comm comm_node;
  char hostname[NAME_MAX];
  unsigned char digest[16];

  status = gethostname(hostname, NAME_MAX);
  hostname[NAME_MAX - 1] - 0;
  md5_state_t pms;
  md5_init(&pms);
  md5_append(&pms, (unsigned char*)hostname, strlen(hostname));
  md5_finish(&pms, digest);
  int digestInt = abs(*(int*)digest); // first few bytes should be unique...

  PMPI_Comm_split(MPI_COMM_WORLD, digestInt, rank, &comm_node);
  PMPI_Comm_split(comm_node, my_socket, my_core, &comm_socket);
  PMPI_Comm_rank(comm_socket, &socket_rank);
  PMPI_Comm_size(comm_socket, &socket_size);

  /* 
     lock semaphore, resize shared object
   */
  if(!socket_rank){
    status = sem_wait(sem);
    status = ftruncate(shm_fd, getpagesize());
    status = sem_post(sem);
    shift_set_socket_governor(my_socket, "userspace");
    shift_socket(my_socket, 0); // set all cores on socket to highest frequency
  }
  PMPI_Barrier(comm_socket);

  return 0;
}

/*!
  close and unlink shared memory segment and semaphore
 */
int shm_teardown(){

  close(shm_fd);
  sem_wait(sem);
  if(!socket_rank)
    shm_unlink(shm_name);
  sem_post(sem);
  
  sem_close(sem);
  if(!socket_rank)
    sem_unlink(shm_name);
  
  return 0;
}

/*! @todo
  Mark an entry in shared memory with an attempted frequency transition.
  This may have no effect on the actual frequency.
 */
int shm_mark_freq(unsigned freq_idx){
  return 0;
}
