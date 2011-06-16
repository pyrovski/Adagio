#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <dirent.h>

static int shm_fd = -1;

int setup_shm(char **argv, const char *hostname){
  char name[NAME_MAX];
  snprintf(name, NAME_MAX, "/%s.%s");
  shm_fd = shm_open(name, O_RDWR | O_CREAT, S_IRWXU);
  if(shm_fd < 0){
    perror("shm_open");
    exit(1);
  }
  return 0;
}
