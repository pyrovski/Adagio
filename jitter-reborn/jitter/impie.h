/*
 * $Id: impie.h,v 1.3 2004/04/21 19:36:01 mefemal Exp $
 */

#ifndef __IMPIE_H
#define __IMPIE_H
#include <mpi.h>

#include <mpi.h>
#include<stdio.h>
#include<time.h>
#include<sys/time.h>
#include<unistd.h>
#include<sys/stat.h>
#include <fcntl.h>
#include<string.h>
#include<pthread.h>
#include<stdlib.h>
#include <sys/types.h>
#include <ctype.h>


#define PRESEND 1
#define POSTSEND 2
#define PREWAIT 3
#define POSTWAIT 4
#define PRERECV 5
#define POSTRECV 6
#define PREBCAST 7
#define POSTBCAST 8
#define PREBARRIER 9
#define POSTBARRIER 10
#define PREALLREDUCE 11
#define POSTALLREDUCE 12
#define PREALLTOALLV 13
#define POSTALLTOALLV 14
#define PREREDUCE 15
#define POSTREDUCE 16
#define PREISEND 17
#define POSTISEND 18
#define PREREDUCEALL 19
#define POSTREDUCEALL 20
#define PREWAITALL 21
#define POSTWAITALL 22
#define PREALLTOALL 23
#define POSTALLTOALL 24

#define ZERO 0.01
#define DELTA 1.1
#define INFINITY 999
#define _X86 1
#define BLOCKING_CALL 0
#define NON_BLOCKING 1
#define Z 0.00001



#define NODE0 0 
#define NODE1 1 
#define NODE2 2
#define NODE3 3
#define NODE4 4
#define NODE5 5
#define NODE6 6
#define NODE7 7


extern int TRACE_JITTER, TRACE_OPM, RANK, TRACE, SHIFT, fd, current_gear ,  current_uops, current_misses, previous_gear, num_shiftingpoints, num_trace_points;
extern double last_iter_time, total_wait_time;
extern double ALPHA;


double timer(void);
//extern "C" int mpidummy_(int *n);

void foo(void);
double MIN(double a, double b);
double timer(void);
void reset_bias(int n);
void reset_all(void);
int mpidummy_(int *n);


#endif

