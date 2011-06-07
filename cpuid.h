#ifndef BLR_CPUID_H
#define BLR_CPUID_H
#include <stdint.h>
//#include <sched.h>
//#define get_cpuid() sched_getcpu()
uint32_t get_cpuid();
#endif
