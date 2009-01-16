#ifndef __TRACE_H
#define __TRACE_H

#include <sys/time.h>

void add_time(unsigned int *tm, struct timeval *t);
void trace_msg(const char *fmt, ...);
void tracefile_open(const char *filename);
void tracefile_close();

#endif

