#include <sys/time.h>	// struct timeval
#include <time.h>	// struct timeval
#include <stdio.h>	// FILE*

double delta_seconds(struct timeval *s, struct timeval *e);
FILE* initialize_logfile(int rank);
int hash_stacktrace(int fid);
