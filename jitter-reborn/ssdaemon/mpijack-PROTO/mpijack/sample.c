/*
 * This is an extremely simple sample file that can be used with PMPIP.
 * Note that the function signatures implemented here must match the
 * prototypes that are available in the MPI specification (i.e. the
 * parameters, not the function names) with the optional data pointer
 * that can be used to track any data you need.  It is also possible
 * to setup a function with no parameters that can be used with
 * multiple pre/post hooks.  Both methods are illustrated below.
 *
 * $Id: sample.c,v 1.2 2004/04/09 08:04:48 mefemal Exp $
 */
#include <stdio.h>
#include <mpi.h>
#include <time.h>
#include <sys/time.h>
#include "plugin.h"

double init_time;

int time_pre() {
   struct timeval tv;
   gettimeofday(&tv, NULL);
   init_time = tv.tv_sec + (double)tv.tv_usec / 1000000;  
   impie_debug(IMPIE_DEBUG_FUNCTION);

   return 0;
}

int time_post() {
   return 0;
}

int send_pre(void *buf, int count , MPI_Datatype dtype, int dest, int tag, MPI_Comm comm, void **data) {
   struct timeval tv;
   double tm;
   int rank;
   MPI_Comm_rank(comm, &rank);
   printf("\nIN SEND ***************\n");
   gettimeofday(&tv, NULL);
   tm = tv.tv_sec + (double)tv.tv_usec / 1000000 - init_time;  

   printf("Rank = %d Tag = %d Dest = %d Pre_Send: = %f", rank, tag,dest,tm);
   impie_debug(IMPIE_DEBUG_FUNCTION);
   return 0;
}

int send_post(void *buf, int count , MPI_Datatype dtype, int dest, int tag, MPI_Comm comm, void **data) {
   struct timeval tv;
   double tm;
   int rank;

   MPI_Comm_rank(comm, &rank);
   gettimeofday(&tv, NULL);
   tm = tv.tv_sec + (double)tv.tv_usec / 1000000 - init_time;
   printf("\nRank = %d Tag = %d Dest = %d Post_Send: = %f",rank, tag,dest,tm);

   return 0;
}


int recv_pre(void *buf, int count , MPI_Datatype dtype, int dest, int tag, MPI_Comm comm, void **data) {
   struct timeval tv;
   double tm;
   int rank;

   MPI_Comm_rank(comm, &rank);
   gettimeofday(&tv, NULL);
   tm = tv.tv_sec + (double)tv.tv_usec / 1000000 - init_time;
   printf("\nRank = %d Tag = %d Dest = %d Pre_Recv: = %f",rank, tag,dest,tm);
   impie_debug(IMPIE_DEBUG_FUNCTION);

   return 0;
}

int recv_post(void *buf, int count , MPI_Datatype dtype, int dest, int tag, MPI_Comm comm, void **data) {
   struct timeval tv;
   double tm;
   int rank;

   MPI_Comm_rank(comm, &rank);
   gettimeofday(&tv, NULL);
   tm = tv.tv_sec + (double)tv.tv_usec / 1000000 - init_time;
   printf("\nRank = %d Tag = %d Dest = %d Post_Recv: = %f\n",rank, tag,dest,tm);

   return 0;
}

