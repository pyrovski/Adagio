#ifndef BLR_CPUID_H
#define BLR_CPUID_H
#include <sched.h>
#define get_cpuid() sched_getcpu()
#endif
