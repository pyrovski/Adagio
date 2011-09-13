#include <stdio.h>
#include "cpuid.h"

int main(int argc, char ** argv){
  parse_proc_cpuinfo();
  printf("sockets: %d\ncores: %d\nmax apicid: %d\ncores per socket: %d\n", 
	 config.sockets, 
	 config.cores,
	 config.max_apicid,
	 config.cores_per_socket);

  int core, socket, local;
  get_cpuid(&core, &socket, &local);
  printf("core: %d\nsocket: %d\nlocal: %d\napicid: %d\nper-socket core: %d\n", 
	 core,   // which core on this node?
	 socket, // which socket
	 local,  // which core on this socket?
	 config.map_core_to_apicid[core],
	 config.map_core_to_per_socket_core[core]);

  return 0;
}
