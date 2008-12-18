#include <sys/time.h>	// struct timeval
#include <time.h>	// struct timeval
#include <stdio.h>	// FILE*

double delta_seconds(struct timeval *s, struct timeval *e);
void dump_timeval( FILE *f, char *str, struct timeval *t );
FILE* initialize_logfile(int rank);
int hash_stacktrace(int fid);
