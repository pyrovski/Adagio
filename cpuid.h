#ifndef BLR_CPUID_H
#define BLR_CPUID_H

extern int my_core, my_socket, my_local; // intialized in post_MPI_Init

typedef struct mcsup_nodeconfig_d 
{
  int sockets;
  int cores;
  int cores_per_socket;
  int *map_core_to_socket;   /* length: cores */
  int *map_core_to_local;    /* length: cores */
  int **map_socket_to_core;  /* length: sockets, cores_per_socket */
  int *map_core_to_per_socket_core; /* length: cores */
} mcsup_nodeconfig_t;

extern mcsup_nodeconfig_t config;

int get_cpuid(int *core, int *socket, int *local);
int parse_proc_cpuinfo();
#endif
