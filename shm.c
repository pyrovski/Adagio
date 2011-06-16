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

int setup_shm(char **argv, unsigned socket, int rank){
  char name[NAME_MAX - 4];
  int status;

  assert(argv && argv[0]);

  status = snprintf(name, NAME_MAX - 4, "/%s.%u", argv[0], socket);
  shm_fd = shm_open(name, O_RDWR | O_CREAT, S_IRWXU);
  if(shm_fd < 0){
    perror("shm_open");
    exit(1);
  }

  sem = sem_open(name, O_CREAT);
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
