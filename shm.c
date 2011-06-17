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

static int shm_fd = -1;
static sem_t *sem = 0;

/*! @todo how does ftruncate affect memory-mapped shared objects? 
  @todo error checking
 */
static int shm_update(int rank){
  assert(shm_fd >= 0);
  assert(sem);
  int status;
  struct stat shm_stat;
  unsigned count = 1;
  status = sem_wait(sem);
  status = lseek(shm_fd, 0, SEEK_SET);
  //! @todo if size 0, truncate to sizeof(unsigned)
  status = fstat(shm_fd, &shm_stat);
  if(shm_stat.st_size < sizeof(unsigned)){
    status = ftruncate(shm_fd, sizeof(unsigned) + count * sizeof(int));
    status = write(shm_fd, &count, sizeof(unsigned));
    status = write(shm_fd, &rank, sizeof(int));
  } else {
    status = read(shm_fd, &count, sizeof(unsigned));
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

int shm_setup(char **argv, unsigned socket, int rank){
  char name[NAME_MAX - 4];
  char *runName;
  int status;

  assert(argv && argv[0]);
  
  // use part of name after last '/'
  runName = strrchr(argv[0], '/');
  runName = runName ? runName + 1 : argv[0];
  runName = runName ? runName : argv[0];
  
  status = snprintf(name, NAME_MAX - 4, "/%s.%u", runName, socket);
  shm_fd = shm_open(name, O_RDWR | O_CREAT, S_IRWXU);
  if(shm_fd < 0){
    perror("shm_open");
    exit(1);
  }

  sem = sem_open(name, O_CREAT, S_IRWXU, 1);
  if(sem == SEM_FAILED){
    perror("sem_open");
    exit(1);
  }

  /* 
     lock semaphore, resize shared object, increment count, 
     and place rank in list
   */
  shm_update(rank);

  return 0;
}

/*!
  close and unlink shared memory segment and semaphore
  this should be easier in Finalize since we have a communicator for each socket
 */
int shm_teardown(){
  //! @todo
  return 0;
}
