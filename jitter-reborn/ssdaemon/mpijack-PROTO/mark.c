#include "mark.h"
#include <stdio.h>
#define MPI_BUILD_PROFILING

#define _X86 1

Info info;
struct pair solutions[200];
int tracepoint[200];


int write_to_file(char *buf, int len)
{
 	printf("mark.c: write_to_file disabled by tkbletsc ('%s')\n",buf); return 0;
  //write(fd, buf, len);
   return 0;
}

//pc call time gear uops misses

int writetrace(unsigned long pc, const char* call) {
	printf("mark.c: writetrace has been disabled by tkbletsc\n"); return 0;
	
  double opr, oprT;   
    static unsigned long long opsT, refsT;
  unsigned long long ops, refs, tlb_miss, branch_miss;
  double tm;
  char buff[80];
    int i;
  if(TRACE_OPM && (RANK==NODE0 || RANK==NODE2))
    {
        //if(strlen(call) >= 4 && strncmp(call, "MARK", 4) == 0)
        ops = refs = tlb_miss = branch_miss = 0;
        if(strncmp(call, "MARK", 4) == 0 || call[strlen(call)-1] == '1')
        for(i=0;i<num_trace_points;i++)
        {
            if(tracepoint[i] == pc)
            {
                readInfo(2, info);
                ops  = info->counters[0];
                refs = info->counters[1];
                break;
            }
        }
      tm=timer();    
      opsT += ops;
      refsT += refs;

      opr = refs==0 ? 1000 : ((double)ops)/refs;
      oprT = refsT==0 ? 1000 : ((double)opsT)/refsT;
      sprintf(buff, "%lu %s %0.6f %d %llu %llu\n", pc, call, tm, current_gear, ops, refs);

      //printf("%d: opr %5g (%5g) cyc %llu gear %d\n", marker, opr,oprT,info->cycles, whatGear());
      //sprintf(buff,"%lu %s %0.6f %d %d %d\n", *pc, call, *tm, *gear, *uops, *misses);
      if(TRACE_OPM)
	      write_to_file(buff,strlen(buff));
    }
}


void initmark_() 
{
    int pid;
 return;

    //current_gear = whatGear();
    //pid = getpid();
    //info = ckalloc(sizeof(*info));
    //pmcEvent(0, 0xc1);
    ///pmcEvent(1, 0x41);
    //initInfo(pid, 0, 0);
    //readInfo(2, info); // clear log
}


void mpimark_(int* marker, int* id)
{
  unsigned long int addr = 0;
  char name[20];
  
  MARK_CALL_ADDRESS1(addr);

  sprintf(name, "MARK%d", *marker);
  writetrace(addr, name);
 change_gear(addr);
}


void change_gear(unsigned long int addr)
{
    int i;
    int new_gear;

  if(SHIFT)
    {
      for(i=0;i<num_shiftingpoints;i++)
	  {
	    if(addr == solutions[i].addr)
	      {
		    new_gear = solutions[i].gear;
            //printf("new gear for node %d: %d\n", RANK, new_gear);
            agent_set_pstate(0, new_gear);
//	    set_device_pstate(0, new_gear);
            //printf("out of shift\n");
            current_gear = new_gear;
		    break;
	      }
        
	    //fprintf(stderr, "no matching pc:%lu?\n", addr);
	  }
    }
} 
