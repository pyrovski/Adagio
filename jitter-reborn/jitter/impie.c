/**
 Jitter implementation
 by Nandini Kappiah
 cleaned and modified by Tyler Bletsch
*/

#include <mpi.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>
#include <sys/types.h>
#include <ctype.h>
#include <math.h>
#include <sys/times.h>
#include <numa.h>
#include "/osr/users/rountree/GreenMPI/src/shim/meters.h"
#include "/osr/users/rountree/GreenMPI/src/shim/cpuid.h"
#include "/osr/users/rountree/GreenMPI/src/shim/affinity.h"
#include "impie.h"

/**
 * We don't want to use the CALLER_ADDRESS any more, so it's just a no-op now.
 * To restore it, bring back platform.h and comment this out. -tkbletsc
 */
#define CALLER_ADDRESS(r) r=0;
/* #include "platform.h" */

/**
 * Set the mechanism for power scaling
 * 0 = Power scaling disabled
 * 1 = Mark's lpagent
 * 2 = Tyler's ssdaemon
 */
#define CPU_SCALING_MECHANISM 2

#if CPU_SCALING_MECHANISM==1
#	include "/osr/pac/include/agent.h"
#elif CPU_SCALING_MECHANISM==2
#	include <sscaling.h>
#endif

#define MPI_BUILD_PROFILING

// Do the (necessary) all-reduce?  Disable for profiling purposes
#define JITTER_ALLREDUCE 1

// Printf a bunch of stuff?
#define DEBUG_OUTPUT 1

// Write the gears selected at end of run to gears-used.txt
#define OUTPUT_GEARSET_AT_END 1

/**
 * Set to 0 for full, normal operation
 * Set to 1 for "with data collection", i.e. each MPI call has a pre- and post-
 *   hook, but no JITTER calculations are done with the dummy call.
 * Set to 2 for "without data collection", i.e. each MPI call just drops right
 *   to the PMPI call
 */
#define BENCH_DATA_COLLECTION 0

// Put a WHEREAMI to output the host and line number
#define WHEREAMI { char b[256]; FILE* f=popen("hostname","r"); fgets(b,255,f); b[strlen(b)-1]=0; fclose(f); printf("[%s] L %d\n",b,__LINE__); }


// This should be 6, unless you're doing something special
#define LOWEST_GEAR 4

#define WINDOW_SIZE 5
double SLACK, THRESHOLD_ITER, THRESHOLD_WT, BIAS;
int SKIP;
int prog_start_time=0;
int TRACE_JITTER, TRACE_OPM, RANK=-1, SHIFT=0, SIZE=-1;
int MANUAL=0; // Manual mode allows direct specification of gears in config.dat
int MANUAL_GEARS[256]; // Gear for nodes, max 256 nodes for now
//int TRACE=0;
//int fd;
static FILE* fpStats;
int current_gear ,  current_uops, current_misses, previous_gear, num_shiftingpoints, num_trace_points;
double last_iter_time, total_wait_time=0;
double slack_table[7] = {0.0,0.0,0.0,0.0,0.0,0.0,0.0}; //table to track the slack
double ALPHA;
double min_wait_time = INFINITY;
int call_type = 1; /////0 - if blocking call, 1 - non blocking call
double impie_init_time=0.0;
double total_send_time=0.0;
int NO_OF_CALLS=0;

double jitter_time;
int iteration=0;

static int num_waits, zero_wait;

static double downshift[10];
static double upshift[] = {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};


static void Pre_Non_Blocking(unsigned long int addr, char *name) ;
static void Post_Non_Blocking(unsigned long int addr, char *name) ;
static void Pre_Blocking(unsigned long int addr, char *name) ;
static void Post_Blocking(unsigned long int addr, char *name) ;

double MIN(double a, double b) {
	++num_waits;
	if (b < Z) {
		++zero_wait;
		return a;
	}
	if (a < Z) {
		++zero_wait;
		return b;
	}
	if (a<b)
		return a;
	else
		return b;
}

//double timer();
//void change_gear(unsigned long int);

double timer() {
	double tm;
	struct timeval t;
	if (gettimeofday(&t, NULL) < 0) {
		printf("gettimeofday failed\n");
		exit(-1);
	}
	tm=((t.tv_sec + t.tv_usec/(double)1000000));
	return(tm-impie_init_time);
}

static void jit_die(char* msg) {
	char buf[256];
	sprintf(buf,"[%d] %s",RANK,msg);
	perror(buf);
	exit(-1);
}

int MPI_Init(int *argc, char ***argv) {
	int rval;
#if BENCH_DATA_COLLECTION==0
	/*
	FILE* fp;
	char choice[30];
	int i;
	*/
	//int addr, number;
	//int tmp; 
	/*
	char tmpstr0[120];
	char tmpstr1[20];
	char tmpstr2[20];
	char tmpstr3[30];
	char tmpstr4[30];
	char tmpstr5[30];
	char tmpstr6[30];
	char tmpstr7[30];
	char tmpstr8[30];
	*/
	fprintf(stderr,"JITTER: MPI_Init\n");

	//initmark_();
#endif // BENCH_DATA_COLLECTION
	rval=PMPI_Init(argc, argv);
	PMPI_Comm_rank(MPI_COMM_WORLD, &RANK);
	PMPI_Comm_size(MPI_COMM_WORLD, &SIZE);
	set_cpu_affinity(RANK);
	numa_set_localalloc();
	mark_joules(RANK, SIZE);
#if BENCH_DATA_COLLECTION==0
	impie_init_time=timer();
	/*
	fp=fopen("options.dat","r");
	if (!fp) {
		jit_die("fopen(./options.dat)");
	}

	fscanf(fp, "%s\n", tmpstr0);
	fscanf(fp, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n", tmpstr1, tmpstr2, tmpstr3, tmpstr4, tmpstr5, tmpstr6, tmpstr7, tmpstr8);
	SLACK=atof(tmpstr1);
	THRESHOLD_ITER=atof(tmpstr2);
	THRESHOLD_WT=atof(tmpstr3);
	ALPHA=atof(tmpstr4);
	BIAS=atof(tmpstr5);
	SKIP=atoi(tmpstr6);
	//TRACE_JITTER=atoi(tmpstr7);
	TRACE_OPM=atoi(tmpstr8);
	fclose(fp);
	*/
	SLACK=0.1;			//Seconds?  Percent?  "Minimum net slack to shift."
	THRESHOLD_ITER=1.05;		//Speed up if misprediction exceeds this.
	THRESHOLD_WT=0;			//ignore.
	ALPHA=0.5;			//ignore.
	BIAS=2.0;			//See jitter paper.
	SKIP=1;			//Iterations per recalculation.
	TRACE_JITTER=0;			//ignore.
	TRACE_OPM=0;			//ignore?
	///////////////////////////config.dat
	/*
	fp=fopen("config.dat","r");
	if (!fp) {
		jit_die("fopen(./config.dat)");
	}

	while (fscanf(fp,"%s",choice) == 1) {

		// NOTE: this used to set var TRACE, but that was never used, so it now sets TRACE_JITTER
		if (strcmp(choice, "TRACE") == 0) {
			TRACE_JITTER=1;
		} else if (strcmp(choice, "SHIFT") == 0) {
			SHIFT=1;
			MANUAL=0;
		} else if (strcmp(choice, "MANUAL") == 0) {
			printf("Manual mode enabled, parsing gears...\n");
			SHIFT = 0;
			MANUAL = 1;
			i=0;
			for (i=0; fscanf(fp,"%d",&MANUAL_GEARS[i])==1; i++) {
				printf("  Node [%d] runs in gear <%d>\n",i,MANUAL_GEARS[i]);
			}
		} else if (strcmp(choice, "TRACE_SHIFT")==0 || strcmp(choice, "SHIFT_TRACE") == 0) {
			// legacy, better to use SHIFT TRACE now
			TRACE_JITTER = SHIFT = 1;
			MANUAL=0;
		} else if (strcmp(choice, "SIMPLE") == 0) {
			MANUAL = TRACE_JITTER = SHIFT = 0;
		} else {
			fprintf(stderr, "File config.dat: syntax incorrect\n");
			exit(1);
		}
	}
	fclose(fp);
	*/
	SHIFT=1;
	TRACE_JITTER=0;
	MANUAL=0;

	/*
	solution_file = fopen("solution.txt", "r");
	if (solution_file != NULL) {
		num_shiftingpoints = 0;
		num_trace_points = 0;
		while (fscanf(solution_file, "%lu %d", &addr, &number) != EOF) {
			if (number < 0) {
				//print list
				tracepoint[num_trace_points] = addr;
				num_trace_points++;
			} else {
				solutions[num_shiftingpoints].addr = addr;
				solutions[num_shiftingpoints].gear = number;
				num_shiftingpoints++;
			}
			//printf("%lu %d\n", solutions[num_shiftingpoints].addr, solutions[num_shiftingpoints].gear);
		}
		fclose(solution_file);

	}
	*/

	if (SHIFT || MANUAL) {
#if CPU_SCALING_MECHANISM==1
		if (agent_connect(0, 0)) {
			printf("agent_connect: Failed!  Is lpagent running?\n");
			perror("agent_connect");
			exit(1);
		}
		if (agent_set_pstate(0, 0)) {
			printf("agent_set_pstate: Failed!  Is lpagent running?\n");
			perror("agent_set_pstate");
			exit(1);
		}
#elif CPU_SCALING_MECHANISM==2
		if (ss_setGearEx(get_cpuid(),0,1)) {
			printf("ss_setGear failed.\n");
			exit(1);
		}
#endif // CPU_SCALING_MECHANISM
	}

#endif // BENCH_DATA_COLLECTION
	return rval;
}  


int MPI_Comm_rank(MPI_Comm comm, int *rank) {
	int rval;
	static int check=0;
	rval=PMPI_Comm_rank(comm, rank);

#if BENCH_DATA_COLLECTION==0
	if (check++==0) {
		char filename[64];
		RANK=*rank;
		if (TRACE_JITTER) {
			sprintf(filename,"jitter__node%d.dat",RANK);
			fpStats=fopen(filename,"w");
			if (!fpStats) jit_die("fopen(fpStats)");
			
			//writing first line of trace - with benchmark name, date and time;
			if (fprintf(fpStats,"# node: %d\n",RANK) < 0) perror("fprintf");
		}
		
#if DEBUG_OUTPUT
		if (RANK==0) {
			printf("\nSLACK: %0.6f THRESHOLD(ITER): %0.6f THRESHOLD(WAIT): %0.6f ALPHA %0.6f SHIFT: %d TRACE: %d BIAS %f SKIP %d TRACE_JITTER %d TRACE_OPM %d\n", SLACK, THRESHOLD_ITER,THRESHOLD_WT, ALPHA, SHIFT, 999999999, BIAS, SKIP, TRACE_JITTER, TRACE_OPM);
		}
#endif

#endif // BENCH_DATA_COLLECTION
		// We print this one message regardless of DEBUG_OUTPUT
		/*
		printf("[%d] JITTER is ENABLED! {\n"
			   "       DEBUG_OUTPUT=%s\n"
			   "       JITTER_ALLREDUCE=%s\n"
			   "       CPU_SCALING_MECHANISM=%d\n"
			   "       BENCH_DATA_COLLECTION=%d\n"
			   "}\n",
			   RANK,
			   DEBUG_OUTPUT?"TRUE":"false",
			   JITTER_ALLREDUCE?"TRUE":"false",
			   CPU_SCALING_MECHANISM,
			   BENCH_DATA_COLLECTION
			  );
		*/
#if BENCH_DATA_COLLECTION==0
	}
#endif // BENCH_DATA_COLLECTION
	return rval;
}


void reset_bias(int n) {
	if (!SHIFT) return; // Other modes don't bother with this
	int i;
	for (i=0;i<n;++i) {
		upshift[i] = 1.0;
		downshift[i] = 1.0 + i*0.05;
//		downshift[i] = 1.0;
	}   
#if DEBUG_OUTPUT
	printf("[%d] %-12s <%d>\n", RANK, "Reset Bias", current_gear);
#endif
}

void reset_all() {
	if (!SHIFT) return; // Other modes don't bother with this
	// Gear to 0
	current_gear=0;
#if CPU_SCALING_MECHANISM==1
	agent_set_pstate(0, current_gear);
#elif CPU_SCALING_MECHANISM==2
	if (ss_setGearEx(get_cpuid(), current_gear,1)) exit(1);
#endif

	// Reset shift biases
	reset_bias(10);
#if DEBUG_OUTPUT
	printf("[%d] %-12s <%d>\n", RANK, "Reset All", current_gear);
#endif
}

//Can take a parameter 
//int mpidummy_(int *n) {
int MPI_Pcontrol(const int n, ...){
	//fprintf(stderr, "BLR: Entering Jitter, iteration=%d\n", iteration);
	if(n!=0){
		return 0;
	}
#if BENCH_DATA_COLLECTION==0
	//static int current_gear;
	static double previous_iter_length, prev_prev_iter_length;
	double slack;
	double iter_length;
//  static float downshift[]  {1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
	double global_min_slack=0;
	static int increased_last_time=0,  decreased_last_time=0;
	static int prevGear;
	static double iter_wnd[WINDOW_SIZE]={-1.0, -1.0, -1.0, -1.0, -1.0};
	static int iter_wnd_ptr=0;
	double jit_time;  
	int num=0,xy;
	double iter_sum, avg_iter;
#if DEBUG_OUTPUT
	char buf[256];
#endif
//  static SLK= SLACK;
	//printf("&"); fflush(0);

	jit_time=timer();
	prevGear=current_gear; 
	iteration++;
	if (iteration==1) {
		reset_bias(10);
		if (MANUAL) {
			current_gear = MANUAL_GEARS[RANK];
#if CPU_SCALING_MECHANISM==1
			agent_set_pstate(0,current_gear);
#elif CPU_SCALING_MECHANISM==2
			if (ss_setGearEx(get_cpuid(), current_gear,1)) exit(1);
#endif
		}
	}
	if (iteration%SKIP != 0){
		//fprintf(stderr,"BLR: Skipping.... iteration=%d\n", iteration);
		return 0;
	}
		
	
	iter_length=jit_time - last_iter_time; //current iteration length
	last_iter_time=jit_time;

	//updating slack:  X(i+1) = X(i).A + (1- A)X(now)
	slack = total_wait_time/iter_length;
	slack_table[current_gear] = slack_table[current_gear]*ALPHA + (1 - ALPHA)*slack;

	
	iter_wnd[iter_wnd_ptr++]=iter_length;
	iter_wnd_ptr = (iter_wnd_ptr+1)%WINDOW_SIZE;
	//if (iter_wnd_ptr==WINDOW_SIZE) iter_wnd_ptr=0;

	if (iteration!=1) {

#if JITTER_ALLREDUCE
		MPI_Allreduce(&slack, &global_min_slack, 1, MPI_DOUBLE, MPI_MIN, MPI_COMM_WORLD);
#endif
 //if(RANK==0) fprintf(stderr,"\nBLR: Iter %d Global Minimum Slack %f", iteration, global_min_slack);

		slack -= global_min_slack;
		
		if (SHIFT) {
			double iter_ratio;

			if (previous_iter_length < 0.0001 && previous_iter_length > -0.0001)
				iter_ratio = THRESHOLD_ITER;
			else if (prev_prev_iter_length < 0.0001 && prev_prev_iter_length > -0.0001)
				iter_ratio = iter_length/previous_iter_length;
			else {
				iter_ratio = iter_length/previous_iter_length;
				if (iter_ratio < iter_length/prev_prev_iter_length)
					iter_ratio = iter_length/prev_prev_iter_length;
			}

			num=0; iter_sum=0;
			for (xy=0;xy<WINDOW_SIZE;++xy) {
				if (iter_wnd[xy]<-0.9999 && iter_wnd[xy]>-1.0001) {
					iter_sum+=iter_wnd[xy];
					num++;
				}
			}

			if (num!=0)
				avg_iter = iter_sum / num;
			else
				avg_iter = iter_length;

			if (iteration<5)
				avg_iter=previous_iter_length;
			
#if DEBUG_OUTPUT
			sprintf(buf, "[NSl=%.3f, GSl=%.3f, UpSh=%.3f, DnSh=%.3f ItAvT=%.3f ItT=%.3f]",slack, slack + global_min_slack, upshift[current_gear],downshift[current_gear], avg_iter, iter_length); 
#endif
			
			//else if (iter_ratio < THRESHOLD_ITER && slack< 0.25*SLACK/upshift[current_gear] && decreased_last_time==0)
			//else if (iter_ratio > THRESHOLD_ITER && decreased_last_time == 1 )
			//else if (slack==0  && decreased_last_time == 1 )
			if (iter_length > THRESHOLD_ITER*avg_iter && decreased_last_time == 1) {
				if (current_gear != 0) {
#if CPU_SCALING_MECHANISM==1
					agent_set_pstate(0,--current_gear);
#elif CPU_SCALING_MECHANISM==2
					if (ss_setGearEx(get_cpuid(), --current_gear,1)) exit(1);
#endif
#if DEBUG_OUTPUT
					printf("[%d] %-12s <%d> %s UpSh[+1]=%.3f\n", RANK, "Return", current_gear, buf, upshift[current_gear+1]);
#endif
					upshift[current_gear+1]*=BIAS;
					increased_last_time=1;
					decreased_last_time=0;
					SLACK=SLACK + 0.02;
				} else {
#if DEBUG_OUTPUT
					printf("[%d] %-12s <%d> %s\n", RANK, "No return", current_gear, buf);
#endif
					increased_last_time=0;
					decreased_last_time=0;
				}
			} else if (increased_last_time == 0 && slack> SLACK*downshift[current_gear]) {
				if (current_gear!=LOWEST_GEAR) {
					downshift[current_gear] *= BIAS;
#if CPU_SCALING_MECHANISM==1
					agent_set_pstate(0, ++current_gear);
#elif CPU_SCALING_MECHANISM==2
					if (ss_setGearEx(get_cpuid(), ++current_gear,1)) exit(1);
#endif
#if DEBUG_OUTPUT
					printf("[%d] %-12s <%d> %s DnSh[-1]=%.3f\n", RANK, "Slowdown", current_gear, buf, downshift[current_gear -1]);
#endif
					increased_last_time=0;
					decreased_last_time=1;
				} else {
#if DEBUG_OUTPUT
					printf("[%d] %-12s <%d> %s\n", RANK, "No slowdown",current_gear, buf);
#endif
					increased_last_time=0;
					decreased_last_time=0;
				}   
			} else if (decreased_last_time==0 && slack < 0.25*SLACK/upshift[current_gear]) {
				if (current_gear!=0) {
#if CPU_SCALING_MECHANISM==1
					agent_set_pstate(0,--current_gear);
#elif CPU_SCALING_MECHANISM==2
					if (ss_setGearEx(get_cpuid(), --current_gear,1)) exit(1);
#endif
#if DEBUG_OUTPUT
					printf("[%d] %-12s <%d> %s UpSh[+1]=%.3f\n", RANK, "Speedup",current_gear, buf, upshift[current_gear+1]);
#endif
					upshift[current_gear+1]*=BIAS;
					increased_last_time=1;
					decreased_last_time=0;
				} else {
#if DEBUG_OUTPUT
					printf("[%d] %-12s <%d> %s\n", RANK, "No speedup", current_gear, buf);
#endif
					increased_last_time=0;
					decreased_last_time=0;
				}
			} else {
#if DEBUG_OUTPUT
				printf("[%d] %-12s <%d> %s\n", RANK, "unchanged", current_gear, buf);
#endif
				increased_last_time=0;
				decreased_last_time=0;
			}
		} else { //SHIFT=0
#if DEBUG_OUTPUT
			printf("[%d] <%d> It=%d [NetSlk=%.3f, GrSlk=%.3f, ItrCur=%.3f]\n",RANK, current_gear, iteration, slack, slack + global_min_slack, iter_length);
#endif
		}
	} //skips first iteration

	//writing trace
	//iteration total_wait_time, total_send_time iter_length, minimum_wait_time, prevGear, current_gear, jit_time

	if (TRACE_JITTER) {
		int result = fprintf(fpStats,"%d %0.6f %0.6f %0.6f %0.6f %d %d %0.6f\n",
				iteration, total_wait_time, total_send_time , iter_length,
				min_wait_time*1000000, prevGear, current_gear, jit_time);
		if (result<0) jit_die("fprintf(fpStats)");
		fflush(fpStats);
	}
	fprintf(stderr,"BLR: %d %0.6f %0.6f %0.6f %0.6f %d %d %0.6f\n",
		iteration, total_wait_time, total_send_time , iter_length,
		min_wait_time*1000000, prevGear, current_gear, jit_time);

	//initialize for next iteration
	prev_prev_iter_length = previous_iter_length;
	num_waits = zero_wait = 0;
	previous_iter_length=iter_length; 
	total_wait_time=0; 
	iter_length=0;
	total_send_time=0;
	min_wait_time = INFINITY;
	call_type = NON_BLOCKING;
	jitter_time += timer() - jit_time;
	return 0;

#endif // BENCH_DATA_COLLECTION
}


static void Pre_Non_Blocking(unsigned long int addr, char *name) {
	addr=addr;
	name=name;
	// printf("%s",name);
	// writetrace(addr, name);
}

static void Post_Non_Blocking(unsigned long int addr, char *name) {
	addr=addr;
	name=name;
	// printf("%s",name);
	call_type=NON_BLOCKING;
	//writetrace(addr, name);
	/*
	if (TRACE_OPM)
		mpimark_(&name1, &RANK);
	*/
	//change_gear(addr);
	NO_OF_CALLS++;
}        

double tim;
static void Pre_Blocking(unsigned long int addr, char *name) {
	addr=addr;
	name=name;
	// printf("%s",name);
	//writetrace(addr, name);
	tim=timer();
}        

static void Post_Blocking(unsigned long int addr, char *name) {
	addr=addr;
	name=name;
	//  printf("%s",name);
	double wait_time;
	wait_time=timer()-tim;
	total_wait_time+=wait_time;
	if (call_type==NON_BLOCKING)
		min_wait_time=MIN(min_wait_time, wait_time);
	//writetrace(addr, name);
	call_type=NON_BLOCKING;
	/*
	if (TRACE_OPM)
		mpimark_(&name1, &RANK);
	*/
	//change_gear(addr);
	NO_OF_CALLS++;
}


//Post
int MPI_Irecv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Request *request) {
	int rval;
	unsigned long int addr=0;
#if BENCH_DATA_COLLECTION<=1
	CALLER_ADDRESS(addr);
#endif
	rval=PMPI_Irecv(buf, count, datatype, source, tag, comm, request);
#if BENCH_DATA_COLLECTION<=1
	Post_Non_Blocking(addr, "Irecv1");
#endif
	return rval;

}

int MPI_Isend(void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request) {
	int rval;
	unsigned long int addr=0;
#if BENCH_DATA_COLLECTION<=1
	CALLER_ADDRESS(addr);
#endif
	rval=PMPI_Isend(buf, count, datatype, dest, tag, comm, request);
#if BENCH_DATA_COLLECTION<=1
	Post_Non_Blocking(addr, "Isend1");
#endif
	return rval;
}  


//Pre and Post
int MPI_Wait(MPI_Request *request, MPI_Status *status) {
	int rval;
	unsigned long int addr=0;
#if BENCH_DATA_COLLECTION<=1
	CALLER_ADDRESS(addr);
	Pre_Blocking(addr, "Wait0");
#endif
	rval=PMPI_Wait(request, status);
#if BENCH_DATA_COLLECTION<=1
	Post_Blocking(addr, "Wait1");
#endif
	return rval;
}  

//tkbletsc begin
int MPI_Sendrecv( void *sendbuf, int sendcount, MPI_Datatype sendtype, 
				  int dest, int sendtag, 
				  void *recvbuf, int recvcount, MPI_Datatype recvtype, 
				  int source, int recvtag, MPI_Comm comm, MPI_Status *status ) {
	int rval;
	unsigned long int addr=0;
#if BENCH_DATA_COLLECTION<=1
	CALLER_ADDRESS(addr);
	Pre_Blocking(addr, "Sendrecv0");
#endif
	rval=PMPI_Sendrecv( sendbuf, sendcount, sendtype, dest, sendtag, recvbuf, recvcount, recvtype, source, recvtag, comm, status );
#if BENCH_DATA_COLLECTION<=1
	Post_Blocking(addr, "Sendrecv1");
#endif
	return rval;
}  

int MPI_Iprobe( int source, int tag, MPI_Comm comm, int *flag, MPI_Status *status ) {
	int rval;
	unsigned long int addr=0;
#if BENCH_DATA_COLLECTION<=1
	CALLER_ADDRESS(addr);
	Pre_Non_Blocking(addr, "Iprobe0");
#endif
	rval=PMPI_Iprobe(source, tag, comm, flag, status);
#if BENCH_DATA_COLLECTION<=1
	Post_Non_Blocking(addr, "Iprobe1");
#endif
	return rval;
}

int MPI_Test ( MPI_Request *request, int *flag, MPI_Status *status) {
	int rval;
	unsigned long int addr=0;
#if BENCH_DATA_COLLECTION<=1
	CALLER_ADDRESS(addr);
	Pre_Non_Blocking(addr, "Test0");
#endif
	rval=PMPI_Test(request, flag, status);
#if BENCH_DATA_COLLECTION<=1
	Post_Non_Blocking(addr, "Test1");
#endif
	return rval;
}

//tkbletsc end

int MPI_Reduce(void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype , MPI_Op op, int root, MPI_Comm comm) {
	int rval;
	unsigned long int addr=0;
#if BENCH_DATA_COLLECTION<=1
	CALLER_ADDRESS(addr);
	Pre_Blocking(addr, "Red0");
#endif
	rval=PMPI_Reduce(sendbuf, recvbuf, count, datatype, op, root, comm);
#if BENCH_DATA_COLLECTION<=1
	Post_Blocking(addr, "Red1");
#endif
	return rval;
}


int MPI_Allreduce(void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype , MPI_Op op, MPI_Comm comm) {
	int rval;
	unsigned long int addr=0;
#if BENCH_DATA_COLLECTION<=1
	CALLER_ADDRESS(addr);
	Pre_Blocking(addr, "Allred0");
#endif
	rval=PMPI_Allreduce(sendbuf, recvbuf, count, datatype, op, comm);
#if BENCH_DATA_COLLECTION<=1
	Post_Blocking(addr, "Allred1");
#endif
	return rval;

}  


//Pre and Post
int MPI_Alltoall(void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcnt, MPI_Datatype recvtype, MPI_Comm comm) {
	int rval;
	unsigned long int addr=0;
#if BENCH_DATA_COLLECTION<=1
	CALLER_ADDRESS(addr);
	Pre_Blocking(addr, "Alltoall0");
#endif
	rval=PMPI_Alltoall(sendbuf, sendcount, sendtype, recvbuf, recvcnt, recvtype, comm);
#if BENCH_DATA_COLLECTION<=1
	Post_Blocking(addr, "Alltoall1");
#endif
	return rval;
}  


//Pre and Post
int MPI_Alltoallv(void *sendbuf, int *sendcnts, int *sdispls, MPI_Datatype sendtype, void *recvbuf, int *recvcnts, int *rdispls, MPI_Datatype recvtype, MPI_Comm comm) {
	int rval;
	unsigned long int addr=0;
#if BENCH_DATA_COLLECTION<=1
	CALLER_ADDRESS(addr);
	Pre_Blocking(addr, "Alltoallv0");
#endif
	rval=PMPI_Alltoallv(sendbuf, sendcnts, sdispls, sendtype, recvbuf, recvcnts, rdispls, recvtype, comm);
#if BENCH_DATA_COLLECTION<=1
	Post_Blocking(addr, "Alltoallv1");
#endif
	return rval;
}  



//Post
int MPI_Send(void *buf, int count, MPI_Datatype datatype, int dest, int tag,MPI_Comm comm) {
	int rval;
	unsigned long int addr=0;
	double send_time;
#if BENCH_DATA_COLLECTION<=1
	CALLER_ADDRESS(addr);
	send_time=timer();
#endif
	rval=PMPI_Send(buf, count, datatype, dest, tag, comm);
#if BENCH_DATA_COLLECTION<=1
	total_send_time+= timer()-send_time;
	Post_Non_Blocking(addr, "Send1");
#endif
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


int MPI_Allgatherv ( void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int *recvcount,int *displs, MPI_Datatype recvtype, MPI_Comm comm ) {
	int rval;
	unsigned long int addr=0;
#if BENCH_DATA_COLLECTION<=1
	CALLER_ADDRESS(addr);
	Pre_Blocking(addr, "Allgatherv0");
#endif
	rval=PMPI_Allgatherv(sendbuf, sendcount, sendtype, recvbuf, recvcount, displs,  recvtype, comm );
#if BENCH_DATA_COLLECTION<=1
	Post_Blocking(addr, "Allgatherv1");
#endif
	return rval; 
}


int MPI_Allgather ( void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, MPI_Comm comm ) {
	int rval;
	unsigned long int addr=0;
#if BENCH_DATA_COLLECTION<=1
	CALLER_ADDRESS(addr);
	Pre_Blocking(addr, "Allgatherv0");
#endif
	rval=PMPI_Allgather(sendbuf,sendcount, sendtype,recvbuf, recvcount,  recvtype,  comm );
#if BENCH_DATA_COLLECTION<=1
	Post_Blocking(addr, "Allgatherv1");
#endif
	return rval; 
}


int MPI_Gather ( void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm ) {
	int rval;
	unsigned long int addr=0;
#if BENCH_DATA_COLLECTION<=1
	CALLER_ADDRESS(addr);
	Pre_Blocking(addr, "Gather0");
#endif
	rval=PMPI_Gather(sendbuf,sendcount, sendtype,recvbuf, recvcount,  recvtype, root, comm );
#if BENCH_DATA_COLLECTION<=1
	Post_Blocking(addr, "Gather1");
#endif
	return rval; 
}

int MPI_Scatter ( void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype recvtype, int root, MPI_Comm comm ) {
	int rval;
	unsigned long int addr=0;
#if BENCH_DATA_COLLECTION<=1
	CALLER_ADDRESS(addr);
	Pre_Blocking(addr, "Scatter0");
#endif
	rval=PMPI_Scatter(sendbuf,sendcount, sendtype,recvbuf, recvcount,  recvtype, root, comm );
#if BENCH_DATA_COLLECTION<=1
	Post_Blocking(addr, "Scatter1");
#endif
	return rval; 
}


//Pre and Post
int MPI_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status) {
	int rval;
	unsigned long int addr=0;
#if BENCH_DATA_COLLECTION<=1
	CALLER_ADDRESS(addr);
	Pre_Blocking(addr, "Recv0");
#endif
	rval=PMPI_Recv(buf,count,datatype, source, tag, comm, status);
#if BENCH_DATA_COLLECTION<=1
	Post_Blocking(addr, "Recv1");
#endif
	return rval;
}  



//Pre and Post
int MPI_Waitall(int count, MPI_Request array_of_requests[], MPI_Status  array_of_statuses[]) {
	int rval;
	unsigned long int addr=0;
#if BENCH_DATA_COLLECTION<=1
	CALLER_ADDRESS(addr);
	Pre_Blocking(addr, "Waitall0");
#endif
	rval=PMPI_Waitall(count, array_of_requests, array_of_statuses);
#if BENCH_DATA_COLLECTION<=1
	Post_Blocking(addr, "Waitall1");
#endif
	return rval;
}  


//Pre and Post
int MPI_Barrier(MPI_Comm comm) {
	int rval;
	unsigned long int addr=0;
#if BENCH_DATA_COLLECTION<=1
	CALLER_ADDRESS(addr);
	Pre_Blocking(addr, "Barrier0");
#endif
	rval=PMPI_Barrier(comm);
#if BENCH_DATA_COLLECTION<=1
	Post_Blocking(addr, "Barrier1");
#endif
	return rval;
}  

//Post
// tkbletsc: I believe Bcast is blocking, so this was changed from non-blocking to blocking
int MPI_Bcast(void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm) {
	int rval;
	unsigned long int addr=0;
#if BENCH_DATA_COLLECTION<=1
	CALLER_ADDRESS(addr);
	Pre_Blocking(addr, "Bcast0");
#endif
	rval=PMPI_Bcast(buffer, count, datatype, root, comm);
#if BENCH_DATA_COLLECTION<=1
	Post_Blocking(addr, "Bcast1");
#endif
	return rval;
}  


int MPI_Finalize(void) {
	int rval;
	mark_joules(RANK, SIZE);
#if BENCH_DATA_COLLECTION==0
	if (TRACE_JITTER) {
		if (fclose(fpStats) != 0) perror("fclose(fpStats)");
	}
	
	PMPI_Barrier(MPI_COMM_WORLD);
#if DEBUG_OUTPUT
	printf("[%d] SLACK: %0.3f GEAR: %d\n", RANK, SLACK, current_gear);
#endif
#if OUTPUT_GEARSET_AT_END
	int numProcs;
	int r;
	r=MPI_Comm_size ( MPI_COMM_WORLD, &numProcs );
	int* gears=(int*)calloc(numProcs,sizeof(int));
	MPI_Gather(&current_gear,1,MPI_INT,gears,1,MPI_INT,0,MPI_COMM_WORLD);
	if (r) { fprintf(stderr,"Error with MPI_Gather\n"); }
	if (!r && !RANK) {
		FILE* fpGears = fopen("gears-used.txt","w");
		if (!fpGears) {
			perror("Unable to open gears-used.txt for writing");
			exit(1);
		} else {
			int i;
			for (i=0;i<numProcs;i++) {
				fprintf(fpGears,"%d ",gears[i]);
			}
			fprintf(fpGears,"\n");
			fclose(fpGears);
		}
	}
#endif
#endif // BENCH_DATA_COLLECTION
	rval= PMPI_Finalize();
#if BENCH_DATA_COLLECTION==0
	if (SHIFT) {
#if CPU_SCALING_MECHANISM==1
		agent_set_pstate(0, 0);
		agent_disconnect();
#elif CPU_SCALING_MECHANISM==2
		if (ss_setGearEx(get_cpuid(), 0,1)) exit(1);
#endif
	}
#endif // BENCH_DATA_COLLECTION
	return rval;
} 


