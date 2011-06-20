#ifndef BLR_CPUID_H
#define BLR_CPUID_H
int get_cpuid(int *core, int *socket, int *local);
int parse_proc_cpuinfo();
#endif
