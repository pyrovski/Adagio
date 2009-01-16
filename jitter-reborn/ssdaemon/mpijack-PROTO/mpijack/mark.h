#ifndef MPIMARK_H
#define MPIMARK_H

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

#include "info.h"
#include "util.h"
#include "gear.h"
#include "pmc.h"
#include "impie.h"
#include "platform.h"

#include "/osr/pac/include/agent.h"

extern Info info;

struct pair {
        unsigned long int addr;
            int gear;
};

extern struct pair solutions[200];
extern int tracepoint[200];

void change_gear(unsigned long int);
int write_to_file(char* buf, int len);
int writetrace(unsigned long pc, const char* call);
void initmark_();
void mpimark_(int* marker, int* id);


#endif
