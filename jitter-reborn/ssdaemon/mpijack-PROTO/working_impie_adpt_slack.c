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
#include <math.h>
#include <sys/times.h>

#include "/osr/users/nandini/impie/src/main/mark.h"
#include "/osr/users/nandini/impie/src/main/platform.h"
#include "/osr/users/nandini/impie/src/main/impie.h"

//#include "/osr/cluster/include/shared.h"

#define MPI_BUILD_PROFILING

#define WINDOW_SIZE 5

int prog_start_time=0;
int TRACE_JITTER, TRACE_OPM, RANK, TRACE=0, SHIFT=0, fd, current_gear ,  current_uops, current_misses, previous_gear, num_shiftingpoints, num_trace_points;
double last_iter_time, total_wait_time=0;
double slack_table[7] = {0.0,0.0,0.0,0.0,0.0,0.0,0.0}; //table to track the slack
double ALPHA;
double min_wait_time = INFINITY;
int call_type =1; /////0 - if blocking call, 1 - non blocking call
double impie_init_time=0.0;
double total_send_time=0.0;

static int num_waits, zero_wait;

double MIN(double a, double b)
{
  ++num_waits;
  if(b < Z) {
    ++zero_wait;
    return a;
  }
  if(a < Z) {
    ++zero_wait;
    return b;
  }
  if(a<b)
  	return a;
  else
  	return b;
}



double timer();
void change_gear(unsigned long int);

double timer() 
{
  double tm;
  struct timeval t;
  if (gettimeofday(&t, NULL) < 0) {
          printf("gettimeofday failed\n");
        exit(-1);
       }
  tm=((t.tv_sec + t.tv_usec/(double)1000000));
   return (tm-impie_init_time);
}


int MPI_Init(int *argc, char ***argv)
{
  int rval;
  FILE* solution_file;
  FILE* fd_temp;
  char choice[30];
  int i;
  int addr, number;
 int tmp; 
 char tmpstr0[120];
 char tmpstr1[20];
 char tmpstr2[20];
 char tmpstr3[30];
 char tmpstr4[30];
 char tmpstr5[30];
 char tmpstr6[30];
 char tmpstr7[30];
 char tmpstr8[30];

  initmark_();
  rval=PMPI_Init(argc, argv);
  impie_init_time=timer();
  fd_temp=fopen("options.dat","r");
  if(fd_temp<=0)
  {
    fprintf(stderr, "error in opening options.dat, needs to be in current directory\n");
    exit(-1);
  }
  
  fscanf(fd_temp, "%s\n", tmpstr0);
  tmp = fscanf(fd_temp, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n", tmpstr1, tmpstr2, tmpstr3, tmpstr4, tmpstr5, tmpstr6, tmpstr7, tmpstr8);
  SLACK=atof(tmpstr1);
  THRESHOLD_ITER=atof(tmpstr2);
  THRESHOLD_WT=atof(tmpstr3);
  ALPHA=atof(tmpstr4);
  BIAS=atof(tmpstr5);
  SKIP=atoi(tmpstr6);
  TRACE_JITTER=atoi(tmpstr7);
  TRACE_OPM=atoi(tmpstr8);
  fclose(fd_temp);
  
  ///////////////////////////config.dat
  fd_temp=fopen("config.dat","r");
  if(fd_temp<=0)
  {
    fprintf(stderr, "error in opening config.dat, needs to be in current directory\n");
    exit(-1);
  }
  
//0-log 1-shift
  fscanf(fd_temp,"%s\n",choice);

  if(strcmp(choice, "TRACE") == 0)
    	TRACE=1;
  else if(strcmp(choice, "SHIFT") == 0)
    	SHIFT=1;
  else if(strcmp(choice, "TRACE_SHIFT")==0 || strcmp(choice, "SHIFT_TRACE") == 0)
	TRACE=SHIFT=1;
  else if(strcmp(choice, "SIMPLE") == 0)
      TRACE = SHIFT = 0;
  else
  {
      fprintf(stderr, "incorrect config file\n");
      exit(1);
  }
  
    solution_file = fopen("solution.txt", "r");
    if(solution_file != NULL)
    {
    num_shiftingpoints = 0;
     num_trace_points = 0;
    while (fscanf(solution_file, "%lu %d", &addr, &number) != EOF)
    {
        if(number < 0) 
        {
            //print list
            tracepoint[num_trace_points] = addr;
            num_trace_points++;
        }
        else
        {
            solutions[num_shiftingpoints].addr = addr;
            solutions[num_shiftingpoints].gear = number;
            num_shiftingpoints++;
        }
        //printf("%lu %d\n", solutions[num_shiftingpoints].addr, solutions[num_shiftingpoints].gear);
    }
    fclose(solution_file);

    }
    if(SHIFT)
      {
    agent_connect(0, 0);
    agent_set_pstate(0, 0);
      }
     return rval; 
}  


int MPI_Comm_rank(MPI_Comm comm, int *rank)
{
    int rval, i;
    char buff[80], tempbuf[100];
    static int check=0;
    rval=PMPI_Comm_rank(comm, rank);
   
    //printf("\nIn COMM RANK\n"); fflush(0);
//  printf("\nHello ********************************************\n"); fflush(0);
   

    if(check++==0)
    {
    RANK=*rank;
    if(RANK==NODE0)
    {
	    fd=open("node0.dat",O_WRONLY|O_CREAT|O_TRUNC,0666);
	    if(fd<0)
	    {
		    printf("\nCould not open file- node0\n");
		    exit(-1);
 	    }		    
		    
    }
    if(RANK==NODE1)
    {
	    fd=open("node1.dat",O_WRONLY|O_CREAT|O_TRUNC,0666);
	    if(fd<0)
	    {
		    printf("\nCould not open file -node1\n");
		    exit(-1);
 	    }		
    }
    if(RANK==NODE2)
    {
	    fd=open("node2.dat",O_WRONLY|O_CREAT|O_TRUNC,0666);
	    if(fd<0)
	    {
		    printf("\nCould not open file - node2\n");
		    exit(-1);
 	    }		
    }
    if(RANK==NODE3)
    {
	    fd=open("node3.dat",O_WRONLY|O_CREAT|O_TRUNC,0666);
	    if(fd<0)
	    {
		    printf("\nCould not open file - node3\n");
		    exit(-1);
 	    }		
    }
    if(RANK==NODE4)
    {
	    fd=open("node4.dat",O_WRONLY|O_CREAT|O_TRUNC,0666);
	    if(fd<0)
	    {
		    printf("\nCould not open file - node4\n");
		    exit(-1);
 	    }		
    }
    if(RANK==NODE5)
    {
	    fd=open("node5.dat",O_WRONLY|O_CREAT|O_TRUNC,0666);
	    if(fd<0)
	    {
		    printf("\nCould not open file - node5\n");
		    exit(-1);
 	    }		
    }
    if(RANK==NODE6)
    {
	    fd=open("node6.dat",O_WRONLY|O_CREAT|O_TRUNC,0666);
	    if(fd<0)
	    {
		    printf("\nCould not open file - node6\n");
		    exit(-1);
 	    }		
    }
    if(RANK==NODE7)
    {
	    fd=open("node7.dat",O_WRONLY|O_CREAT|O_TRUNC,0666);
	    if(fd<0)
	    {
		    printf("\nCould not open file - node7\n");
		    exit(-1);
 	    }		
    }
    
    
      
    //writing first line of trace - with benchmark name, date and time;
    sprintf(buff, "# node: %d\n", RANK);
    write_to_file(buff, strlen(buff));
    
    if(RANK==0)
    {
	    printf("\nSLACK: %0.6f THRESHOLD(ITER): %0.6f THRESHOLD(WAIT): %0.6f ALPHA %0.6f SHIFT: %d TRACE: %d BIAS %f SKIP %d TRACE_JITTER %d TRACE_OPM %d\n", SLACK, THRESHOLD_ITER,THRESHOLD_WT, ALPHA, SHIFT, TRACE, BIAS, SKIP, TRACE_JITTER, TRACE_OPM);
    }
    
    //writing first line into the file 
    //sprintf(tempbuf, "%d %0.6f %0.6f %0.6f %0.6f %d\n",     0, 0, 0, 0, 0, 0); 
    // write_to_file(tempbuf, strlen(buff));
   } 
    return rval;
}


void reset_bias(float* downshift, float* upshift, int n)
{
	int i;
	for(i=0;i<n;++i)
	{
		upshift[i] = 1.0;
		downshift[i] = 1.0 + i*0.25;
		//downshift[i] = 1.0;
	}	
}


//Can take a parameter 
int mpidummy_(int *n)
{
  char tempbuf[200];
  static int iteration=0;
  static int current_gear;
  static double previous_iter_length, previous_total_wait_time, prev_prev_iter_length;
  double time_compute, slack;
  float iter_length;
//  static float downshift[]  {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
  static float downshift[10];
  static float upshift[] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
	int i;
  double global_min_slack;
  static int increased_last_time=0,  decreased_last_time=0;
  static int prevGear;
  static double iter_wnd[WINDOW_SIZE]={-1.0, -1.0, -1.0, -1.0, -1.0};
  static int iter_wnd_ptr=0;
  
  int num=0,xy;
  double iter_sum, avg_iter;
//  static SLK= SLACK;
//  printf("Entering dummy \n"); fflush(0);
   
  prevGear=current_gear; 
  iteration++;
  if(iteration==1 )
   { 	
     reset_bias( downshift,upshift, 10);
   }
  if(iteration%SKIP != 0)
  	return 0;
  iter_length=timer() - last_iter_time; //current iteration length
  last_iter_time=timer();
  time_compute = iter_length - total_wait_time;  //Tc
 
  //updating slack:  X(i+1) = X(i).A + (1- A)X(now)
  slack = total_wait_time/iter_length;
  slack_table[current_gear] = slack_table[current_gear]*ALPHA + (1 - ALPHA)*slack;
  
  iter_wnd[iter_wnd_ptr++]=iter_length;
  if(iter_wnd_ptr==WINDOW_SIZE)
  	iter_wnd_ptr=0;
  
  if(iteration!=1 )
  {
  
    MPI_Allreduce(&slack, &global_min_slack, 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD); 
// if(RANK==0) printf("\nIter %d Global Minimum Slack %f", iteration, global_min_slack);
 
   slack = slack - global_min_slack ;
		
  if(SHIFT ) {
    char buf[256];
    int increased=0;
    double wait_ratio, iter_ratio;

    wait_ratio = (float)zero_wait/(float)num_waits;
    
    if (previous_iter_length == 0)
      iter_ratio = THRESHOLD_ITER;
    else if (prev_prev_iter_length == 0)
      iter_ratio = iter_length/previous_iter_length;
    else {
      iter_ratio = iter_length/previous_iter_length;
      if (iter_ratio < iter_length/prev_prev_iter_length)
         iter_ratio = iter_length/prev_prev_iter_length;
    }

    num=0; iter_sum=0;
    for(xy=0;xy<WINDOW_SIZE;++xy)
    {
	if(iter_wnd[xy]!=-1)
		{
			iter_sum+=iter_wnd[xy];
			num++;
		}
    }

    if(num!=0) 
    	avg_iter = iter_sum / num;
    else
    	avg_iter = iter_length;

   if(iteration<5)
   	avg_iter=previous_iter_length;


//    printf("\nAvg Iter Iteration lenght %0.6f  %0.6f || ", avg_iter, iter_length); 
//    sprintf(buf, "[%f, %f, %f, %f min wait: %f]",wait_ratio,iter_ratio,iter_length,slack + global_min_slack, min_wait_time*1000000); 
    sprintf(buf, "[%f, %f, %f, %f]",slack, slack + global_min_slack, upshift[current_gear],downshift[current_gear]); 

    if(iter_ratio >= 2.5)
    {
	int i;
	current_gear=0;
	agent_set_pstate(0, current_gear);
	reset_bias(downshift, upshift, 10);  
	printf("[%d] Reset to %d %s\n", RANK, current_gear, buf);
    }						
    //else if (iter_ratio < THRESHOLD_ITER && slack< 0.25*SLACK/upshift[current_gear] && decreased_last_time==0)
    else if (iter_length > THRESHOLD_ITER*avg_iter && decreased_last_time == 1 )
//    else if (iter_ratio > THRESHOLD_ITER && decreased_last_time == 1 )
//    else if (slack==0  && decreased_last_time == 1 )
    	 {
            if(current_gear != 0) 
	     {
		agent_set_pstate(0,--current_gear);
		printf("[%d] return to %d %s %f\n", RANK, current_gear, buf, upshift[current_gear+1]);
		upshift[current_gear+1]*=BIAS;
 		increased_last_time=1;
		decreased_last_time=0;
		SLACK=SLACK + 0.02;
	     }	
            else 
	     {
		printf("[%d] no return %d %s\n", RANK, current_gear, buf);
		increased_last_time=0;
		decreased_last_time=0;
             }
         }
        else if(increased_last_time == 0 && slack> SLACK*downshift[current_gear])
	      {
	      	   if( current_gear!=6 )
	      		{
		      	downshift[current_gear]*=BIAS;
		      	agent_set_pstate(0, ++current_gear);
	      	      	printf("[%d] slowdown to %d %s %f\n", RANK, current_gear, buf, downshift[current_gear -1]);
		      	increased_last_time=0;
		      	decreased_last_time=1;
        	      	} 
	    	  else
	   	 	{
			printf("[%d] no slowdown %d %s\n", RANK, current_gear, buf);
			increased_last_time=0;
			decreased_last_time=0;
		    	}	
       	      }
	      else if (decreased_last_time==0 && slack < 0.25*SLACK/upshift[current_gear])
	           {
		   	if(current_gear!=0)
			   {
				agent_set_pstate(0,--current_gear);
				printf("[%d] speedup to %d %s %f\n", RANK, current_gear, buf, upshift[current_gear+1]);
				upshift[current_gear+1]*=BIAS;
		 		increased_last_time=1;
				decreased_last_time=0;
			   }
			else
		   	  {
			 	printf("[%d] no speedup %d %s\n", RANK, current_gear, buf);
				increased_last_time=0;
				decreased_last_time=0;
			   }
	          }
	          else 
	          {
	               printf("[%d] NO CHANGE %d %s\n", RANK, current_gear, buf);
	               increased_last_time=0;
		       decreased_last_time=0;
	          }
     }
   else //SHIFT=0
    {
    	printf("[%d] NO SHIFT [%f] min wait: %f abs slack %f\n", RANK, iter_length, min_wait_time*1000000, slack+global_min_slack);
    }
 } //skips first iteration
  
  
  //writing trace
  //iteration total_wait_time, total_send_time iter_length, minimum_wait_time, prevGear, current_gear, timer()
  sprintf(tempbuf, "%d %0.6f %0.6f %0.6f %0.6f %d %d %0.6f\n", 
	    iteration, total_wait_time, total_send_time , iter_length, 
	    min_wait_time*1000000, prevGear, current_gear, timer()); 

  if(TRACE_JITTER)
	   write_to_file(tempbuf, strlen(tempbuf));
 
  //initialize for next iteration
  prev_prev_iter_length = previous_iter_length;
  num_waits = zero_wait = 0;
  previous_iter_length=iter_length; 
  previous_total_wait_time = total_wait_time;
  total_wait_time=0; 
  iter_length=0;
  total_send_time=0;
  min_wait_time = INFINITY;
  call_type = NON_BLOCKING;
  
  return 0;
  
}


void Pre_Non_Blocking(unsigned long int addr, char *name)
{
 // printf("%s",name);
  writetrace(addr, name);
}

void Post_Non_Blocking(unsigned long int addr, char *name)
{
  // printf("%s",name);
    int name1=1;
   call_type=NON_BLOCKING;
   writetrace(addr, name);
   if(TRACE_OPM)
	   mpimark_(&name1, &RANK);
   change_gear(addr);
}	     

double tim;
void Pre_Blocking(unsigned long int addr, char *name)
{
  // printf("%s",name);
   writetrace(addr, name);
   tim=timer();
}	     

void Post_Blocking(unsigned long int addr, char *name)
{
 //  printf("%s",name);
    int name1=2;
   double wait_time;
   wait_time=timer()-tim;
   total_wait_time+=wait_time;
   if(call_type==NON_BLOCKING)
   	min_wait_time=MIN(min_wait_time, wait_time);
   writetrace(addr, name);
   call_type=NON_BLOCKING;
   if(TRACE_OPM)	
	mpimark_(&name1, &RANK);
   change_gear(addr);
}
     
	     
//Post
int MPI_Irecv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Request *request)
{
  int rval;
  unsigned long int addr=0;
  CALLER_ADDRESS(addr);
  rval=PMPI_Irecv(buf, count, datatype, source, tag, comm, request);
  Post_Non_Blocking(addr, "Irecv1");
  return rval;

}  

int MPI_Isend(void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request)
{
  int rval;
  unsigned long int addr=0;
  CALLER_ADDRESS(addr);
  rval=PMPI_Isend(buf, count, datatype, dest, tag, comm, request);
  Post_Non_Blocking(addr, "Isend1");
  return rval;
}  
																					     

//Pre and Post
int MPI_Wait(MPI_Request *request, MPI_Status *status)
{
  int rval;
  unsigned long int addr=0;
  CALLER_ADDRESS(addr);
  Pre_Blocking(addr, "Wait0");
  rval=PMPI_Wait(request, status);
  Post_Blocking(addr, "Wait1");
  return rval;
}  



int MPI_Reduce(void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype , MPI_Op op, int root, MPI_Comm comm)
{
  int rval;
  static int iter;
  unsigned long int addr=0;
  CALLER_ADDRESS(addr);
  Pre_Blocking(addr, "Red0");
  rval=PMPI_Reduce(sendbuf, recvbuf, count, datatype, op, root, comm);
  Post_Blocking(addr, "Red1");
  return rval;
}


int MPI_Allreduce(void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype , MPI_Op op, MPI_Comm comm)
{
  int rval;
  unsigned long int addr=0;
  CALLER_ADDRESS(addr);
  Pre_Blocking(addr, "Allred0");
  rval=PMPI_Allreduce(sendbuf, recvbuf, count, datatype, op, comm);
  Post_Blocking(addr, "Allred1");
  return rval;

}  


//Pre and Post
int MPI_Alltoall(void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcnt, MPI_Datatype recvtype, MPI_Comm comm)
{
  int rval;
  unsigned long int addr=0;
  CALLER_ADDRESS(addr);
  Pre_Blocking(addr, "Alltoall0");
  rval=PMPI_Alltoall(sendbuf, sendcount, sendtype, recvbuf, recvcnt, recvtype, comm);
  Post_Blocking(addr, "Alltoall1");
  return rval;
}  


//Pre and Post
int MPI_Alltoallv(void *sendbuf, int *sendcnts, int *sdispls, MPI_Datatype sendtype, void *recvbuf, int *recvcnts, int *rdispls, MPI_Datatype recvtype, MPI_Comm comm)
{
  int rval;
  unsigned long int addr=0;
  Pre_Blocking(addr, "Alltoallv0");
  CALLER_ADDRESS(addr);
  rval=PMPI_Alltoallv(sendbuf, sendcnts, sdispls, sendtype, recvbuf, recvcnts, rdispls, recvtype, comm);
  Post_Blocking(addr, "Alltoallv1");
  return rval;
}  



//Post
int MPI_Send(void *buf, int count, MPI_Datatype datatype, int dest, int tag,MPI_Comm comm)
{
  int rval;
  unsigned long int addr=0;
  double send_time;
 // printf(".");
  CALLER_ADDRESS(addr);
  send_time=timer();
  rval=PMPI_Send(buf, count, datatype, dest, tag, comm);
  total_send_time+= timer()-send_time;
  Post_Non_Blocking(addr, "Send1");
  return rval;
}  

/*

int MPI_Waitany (int count, MPI_Request *array_of_requests, int *index, MPI_Status *status)
{
int rval;
unsigned long int addr=0;
CALLER_ADDRESS(addr);
WRITETRACE(addr, "Waitany0");
rval=PMPI_Waitany(count, array_of_requests, index, status);
WRITETRACE(addr, "Waitany1");
return rval; 
}

*/					 


int MPI_Allgatherv ( void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int *recvcount,int *displs, MPI_Datatype recvtype, MPI_Comm comm )
{
  int rval;
  unsigned long int addr=0;
  CALLER_ADDRESS(addr);
  Pre_Blocking(addr, "Allgatherv0");
  rval=PMPI_Allgatherv(sendbuf, sendcount, sendtype, recvbuf, recvcount, displs,  recvtype, comm );
  Post_Blocking(addr, "Allgatherv1");
  return rval; 
}


int MPI_Allgather ( void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, MPI_Comm comm )
{
int rval;
unsigned long int addr=0;
CALLER_ADDRESS(addr);
Pre_Blocking(addr, "Allgatherv0");
rval=PMPI_Allgather(sendbuf,sendcount, sendtype,recvbuf, recvcount,  recvtype,  comm );
Post_Blocking(addr, "Allgatherv1");
return rval; 
}


int MPI_Gather ( void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm )
{
int rval;
unsigned long int addr=0;
CALLER_ADDRESS(addr);
Pre_Blocking(addr, "Gather0");
rval=PMPI_Gather(sendbuf,sendcount, sendtype,recvbuf, recvcount,  recvtype, root, comm );
Post_Blocking(addr, "Gather1");
return rval; 
}

int MPI_Scatter ( void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm )
{
int rval;
unsigned long int addr=0;
CALLER_ADDRESS(addr);
Pre_Blocking(addr, "Scatter0");
rval=PMPI_Scatter(sendbuf,sendcount, sendtype,recvbuf, recvcount,  recvtype, root, comm );
Post_Blocking(addr, "Scatter1");
return rval; 
}


//Pre and Post
int MPI_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status)
{
  int rval;
  unsigned long int addr=0;
  CALLER_ADDRESS(addr);
  Pre_Blocking(addr, "Recv0");
  rval=PMPI_Recv(buf,count,datatype, source, tag, comm, status);
  Post_Blocking(addr, "Recv1");
  return rval;
}  



//Pre and Post
int MPI_Waitall(int count, MPI_Request array_of_requests[], MPI_Status  array_of_statuses[])
{
  int rval;
  unsigned long int addr=0;
  CALLER_ADDRESS(addr);
  Pre_Blocking(addr, "Waitall0");
  rval=PMPI_Waitall(count, array_of_requests, array_of_statuses);
  Post_Blocking(addr, "Waitall1");
  return rval;
}  


//Pre and Post
int MPI_Barrier(MPI_Comm comm)
{
  int rval;
  unsigned long int addr=0;
  CALLER_ADDRESS(addr);
  Pre_Blocking(addr, "Barrier0");
  rval=PMPI_Barrier(comm);
  Post_Blocking(addr, "Barrier1");
  return rval;
}  

//Post
int MPI_Bcast(void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm)
{
  int rval;
  unsigned long int addr=0;
  CALLER_ADDRESS(addr);
  rval=PMPI_Bcast(buffer, count, datatype, root, comm);
  Post_Non_Blocking(addr, "Bcast1");
  return rval;
}  


int MPI_Finalize(void)
{
 int i,j, k,gear=0, phase=1,rval;
 char buf[256],tmpbuf[10];
 struct tms t;
 FILE *ftmp;
 MPI_Barrier(MPI_COMM_WORLD);
 times(&t);
// printf("\n[%d] utime:%0.6f stime:%0.6f cutime:%0.6f cstime:%0.6f", RANK, t.tms_utime,  t.tms_stime, t.tms_cutime, t.tms_cstime);
//each node prints the slack table 
/* i = sprintf(buf, "[%d] Slack Table:", RANK);
 for(j=0;j<6;++j)
 {
   k = sprintf(buf+i, " %0.6f", slack_table[j]);
   i += k;
 }	
 printf("%s\n", buf);*/
 if(RANK==1)
 {
 	ftmp=fopen("progtime.dat", "w");
	fprintf(ftmp, "%0.6f", timer());
	printf("\nProgtime %0.6f", timer());
	fclose(ftmp);
	
 }
 printf("\n[%d] SLACK: %0.6f", RANK, SLACK);
 rval= PMPI_Finalize();
 if(SHIFT)
 {
    agent_set_pstate(0, 0);
    agent_disconnect(); 
 }
 return rval;
} 

