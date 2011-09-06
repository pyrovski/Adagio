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
#include "shim.h"

//#define _GNU_SOURCE
#define __USE_GNU
#include <sched.h>

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

  if(!argv || !argv[0]){
    runName = "MPI_program";
  } else {
    // use part of name after last '/'
    runName = strrchr(argv[0], '/');
    runName = runName ? runName + 1 : argv[0];
    runName = *runName ? runName : argv[0];
  }
  /*
    Ensure establishment of cpu affinity.
    If not already bound to a core,
    let MPI_Comm_split assign ranks to processes on a socket,
    then use those ranks to chose cpu cores.
   */
  
  cpu_set_t cpuset;
  status = sched_getaffinity(0, sizeof(cpu_set_t), &cpuset);
  int bound = 0;
  //! @todo make this test into a function
#ifdef CPU_COUNT
  bound = (CPU_COUNT(&cpuset) == 1);
#else
  {
    int index;
    for(index = 0; index < sizeof(cpu_set_t) / sizeof(int); index++)
      bound +=  __builtin_popcount(*((int*)&cpuset + index));
    bound = bound == 1;
  }
#endif
#ifdef _DEBUG
  printf("rank %d is ", rank);
  if(bound)
    printf("bound\n");
  else
    printf("not bound\n");
#endif
  
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

  //! @todo this needs to be a function
  MPI_Comm comm_node;
  char hostname[NAME_MAX];
  unsigned char digest[16];

  status = gethostname(hostname, NAME_MAX);
  hostname[NAME_MAX - 1] = 0;
  md5_state_t pms;
  md5_init(&pms);
  md5_append(&pms, (unsigned char*)hostname, strlen(hostname));
  md5_finish(&pms, digest);
  int digestInt = abs(*(int*)digest); // first few bytes should be unique...

  PMPI_Comm_split(MPI_COMM_WORLD, digestInt, rank, &comm_node);
  if(bound)
    PMPI_Comm_split(comm_node, my_socket, my_core, &comm_socket);
  else
    PMPI_Comm_split(comm_node, my_socket, 0, &comm_socket);
  PMPI_Comm_rank(comm_socket, &socket_rank);
  PMPI_Comm_size(comm_socket, &socket_size);
  
  //! @todo fix for g_cores_per_socket
  if(!bound){
#ifdef _DEBUG
    printf("prior to binding, rank %d is in socket %d rank %d (%d)\n", 
	   rank, my_socket, socket_rank, socket_size);
#endif
    if(g_bind & bind_COLLAPSE){
      int i;
      cpu_set_t cpusetCollapsed;
      for(i = 0; i < sizeof(cpu_set_t) / sizeof(int); i++)
	if(CPU_ISSET(i, &cpuset))
	  break;
      if(i < sizeof(cpu_set_t) / sizeof(int)){
	CPU_ZERO(&cpusetCollapsed);
	CPU_SET(i, &cpusetCollapsed);
#ifdef _DEBUG
	printf("rank %d collapsing binding to core %d\n", rank, i);
#endif
	sched_setaffinity(0, sizeof(cpu_set_t), &cpusetCollapsed);
	status = get_cpuid(&my_core, &my_socket, &my_local);
      }else{
	MPI_Abort(MPI_COMM_WORLD, 1);
      }
    } else {    
      my_core = socket_rank;
#ifdef _DEBUG
      printf("rank %d binding to core %d\n", rank, my_core);
#endif
      CPU_ZERO(&cpuset);
      CPU_SET(my_core, &cpuset);
      status = sched_setaffinity(0, sizeof(cpu_set_t), &cpuset);
      get_cpuid(&my_core, &my_socket, &my_local);
    }
#ifdef _DEBUG
    printf("after binding, rank %d is in socket %d core %d\n", rank, my_socket, 
	   my_core);
#endif
  } else {
#ifdef _DEBUG
    printf("rank %d is in socket %d rank %d (%d)\n", rank, my_socket, 
	   socket_rank, socket_size);
#endif
  }

  /* 
     lock semaphore, resize shared object
     
     A single process on each socket should set governors for all 
     cores on the socket.  Otherwise, if not all cores 
     are participating, there could be interference in 
     frequency selection.
     
  */
  if(!socket_rank){
    status = sem_wait(sem);
    status = ftruncate(shm_fd, getpagesize());
    status = sem_post(sem);
    shift_init_socket(my_socket, "userspace", SLOWEST_FREQ);
  }
  PMPI_Barrier(comm_socket);
  shift_set_initialized(1);
  shift_core(my_core, FASTEST_FREQ);
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
/*
int shm_mark_freq(int freq_idx){
  
  return 0;
}
*/
