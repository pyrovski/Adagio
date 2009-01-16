#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

int main(int argc, char *argv[]) {
   int rank, size;
   char hostname[1000];
   char buf[80];

   gethostname(hostname, 1000);
   printf("Running %s on %s\n", argv[0], hostname);
   MPI_Init(&argc, &argv);
   MPI_Comm_rank(MPI_COMM_WORLD, &rank);
   MPI_Comm_size(MPI_COMM_WORLD, &size);

   printf("Hello world, I am %d of %d\n", rank, size);

   MPI_Finalize();
   printf("Finished %s on %s\n", argv[0], hostname);
   return 0;
}

