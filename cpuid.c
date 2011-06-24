#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "cpuid.h"

int my_core, my_socket, my_local;

/*-----------------------------------------------------------------------*/
/* adapted from                                                          */
/* Multicore/NUMA support library                                        */
/* Martin Schulz, LLNL, 2008/2009                                        */
/*-----------------------------------------------------------------------*/

#define MCSUP_OK                 0
#define MCSUP_WARN_TOOLONG       1
#define MCSUP_ERR_FILEIO        -1
#define MCSUP_ERR_IDOUTOFRANGE  -2
#define MCSUP_ERR_PARSEERROR    -3
#define MCSUP_ERR_NOMEM         -7

#define MAX_LINE 256

//#define VERBOSE

mcsup_nodeconfig_t config;

int whitespace(char c)
{
  if ((c=='\t') || (c==' ') || (c=='\n') || (c==EOF))
    return 1;
  #ifdef AIX
  if ((int)c==255)
    return 1;
  #endif
  return 0;
}

int readline(FILE *fd, char *line, int len)
{
  char c,lastc;
  int pos;

  pos=0;
  lastc=' ';
  c=' ';

  while ((!feof(fd)) && (c!='\n'))
    {
      c=(char)getc(fd);

      if (pos==len-1)
	{
	  line[len]=(char) 0;
	  return MCSUP_WARN_TOOLONG;
	}
      else
	{
	  if ((!(whitespace(c))) || (!(whitespace(lastc))))
	    {
	      if (whitespace(c))
		line[pos]=' ';
	      else
		line[pos]=c;
	      lastc=c;
	      pos++;
	    }
	}
    }
  line[pos]=(char) 0;
  return MCSUP_OK;
}

int parse_proc_cpuinfo()
{
  char line[1024];
  FILE *cpuinfo;
  int core,i,j,err = -1;
  
  cpuinfo=fopen("/proc/cpuinfo","r");
  if (cpuinfo==NULL)
    return MCSUP_ERR_FILEIO;

  config.cores=0;
  while (!(feof(cpuinfo)))
    {
      err=readline(cpuinfo,line, 1024);
      if (err<MCSUP_OK) 
	return err;

      if (strncmp(line,"processor",9)==0)
	{
	  config.cores++;
	}
    }

  config.map_core_to_socket=(int*)malloc(config.cores*sizeof(int));
  config.map_core_to_local=(int*)malloc(config.cores*sizeof(int));
  config.map_core_to_per_socket_core=(int*)malloc(config.cores*sizeof(int));

    if ((config.map_core_to_socket==NULL)||
      (config.map_core_to_local==NULL))
    return MCSUP_ERR_NOMEM;

  for (i=0; i<config.cores; i++)
    {
      config.map_core_to_socket[i]=-1;
      config.map_core_to_local[i]=-1;
      config.map_core_to_per_socket_core[i] = -1;
    }


  cpuinfo=freopen("/proc/cpuinfo","r",cpuinfo);
  if (cpuinfo==NULL)
    return MCSUP_ERR_FILEIO;

  core=-1;
  
  while (!(feof(cpuinfo)))
    {
      readline(cpuinfo,line, 1024);
      if (err<MCSUP_OK) 
	return err;

      if (strncmp(line,"processor",9)==0)
	{
	  core=atoi(strchr(line,':')+2);
	  if ((core<0) || (core>=config.cores))
	    return MCSUP_ERR_IDOUTOFRANGE;
	}

      if (strncmp(line,"physical id",11)==0)
	{
	  if (core>=0)
	    config.map_core_to_socket[core]=atoi(strchr(line,':')+2);
	  else
	    return MCSUP_ERR_PARSEERROR;
	}

      if (strncmp(line,"core id",7)==0)
	{
	  if (core>=0)
	    config.map_core_to_local[core]=atoi(strchr(line,':')+2);
	  else
	    return MCSUP_ERR_PARSEERROR;
	}
    }

  fclose(cpuinfo);

  config.sockets=0;
  config.cores_per_socket=0; // assume all sockets have the same number of cores
  for (i=0; i<config.cores; i++)
    {
      if (config.map_core_to_socket[i]+1>config.sockets)
	config.sockets=config.map_core_to_socket[i]+1;
      if(config.map_core_to_socket[i] == 0)
	config.cores_per_socket++;
    }

  config.map_socket_to_core=(int**)malloc(sizeof(int*)*config.sockets);
  for (i=0; i<config.sockets; i++)
    {
      config.map_socket_to_core[i]=(int*)malloc(sizeof(int)*config.cores_per_socket);
      core=0;
      int coreCount = 0;
      for (j=0; j<config.cores; j++)
	{
	  if (config.map_core_to_socket[j]==i)
	    {
	      config.map_core_to_per_socket_core[j] = coreCount++;
	      config.map_socket_to_core[i][core]=j;
	      core++;
	    }
	}
    } 

#ifdef VERBOSE
  printf("Found %i cores, %i sockets, %i cores per socket\n",
	 config.cores,config.sockets,config.cores_per_socket);
  printf("Mapping of cores to sockets:\n");
  for (i=0; i<config.cores; i++)
    {
      printf("\tCore %2i -> Socket %2i\n",i,config.map_core_to_socket[i]);
    }
  printf("Mapping of sockets to cores:\n");
  for (i=0; i<config.sockets; i++)
    {
      printf("\tSocket %2i ->",i);
      for (j=0; j<config.cores_per_socket; j++)
	printf(" %i",config.map_socket_to_core[i][j]);
      printf("\n");
    }
#endif

  return MCSUP_OK;
}


int get_cpuid(int *core, int *socket, int *local){

  //Return value should be in the range 0-3.
  int a,b,c,d;

  assert(config.map_socket_to_core);
  assert(config.map_core_to_socket);
  assert(config.map_core_to_local);


#define cpuid( in, a, b, c, d )					\
  asm ( "cpuid" :						\
	"=a" (a), "=b" (b), "=c" (c), "=d" (d) : "a" (in));
#define INITIAL_APIC_ID_BITS  0xFF000000

  //Figure out which core we're on.
  cpuid( 1, a, b, c, d );
  a=a; b=b; c=c; d=d;
  
  if(core)
    *core = ( b & INITIAL_APIC_ID_BITS ) >> 24;
  if(socket)
    *socket=config.map_core_to_socket[*core];
  if(local)
    *local=config.map_core_to_local[*core];

#ifdef _DEBUG
  printf("%s: %s ", __FILE__, __FUNCTION__);
  if(core)
    printf("core: %d ", *core);
  if(socket)
    printf("socket: %d ", *socket);
  if(local)
    printf("local: %d\n", *local);
#endif

#undef cpuid
#undef INITAL_APIC_ID_BITS
  return 0;
}
