#include <stdio.h>
#include "cpuid.h"

int my_core, my_socket, my_local;

int main(int argc, char ** argv){
  parse_proc_cpuinfo();
  printf("sockets: %d\ncores: %d\nmax apicid: %d\ncores per socket: %d\n", 
	 config.sockets, 
	 config.cores,
	 config.max_apicid,
	 config.cores_per_socket);
  return 0;
}
