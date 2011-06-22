/* -*- Mode: C; tab-width: 8; c-basic-offset: 8 -*- */
/*!
  @todo get info on CPUs, cores, and frequencies from system
  @todo allow configuration of Turboboost
*/

#include <assert.h>
#include <stdio.h>
#include "shift.h"
#include "cpuid.h"
#include "shm.h"
#define BLR_USE_SHIFT
//#undef BLR_USE_SHIFT

 int NUM_FREQS, SLOWEST_FREQ, FASTEST_FREQ;

 static int current_freq=0;
 static const char *cpufreq_path[] = {"/sys/devices/system/cpu/cpu",
				      "/cpufreq/"};
 static const char cpufreq_governor[] = "scaling_governor";
 static const char cpufreq_speed[] = "scaling_setspeed";
 static int *freqs;// in kHz, fastest to slowest
 static int *prev_freq_idx;
 static int shift_initialized=0;

 int shift_init(){
	 char filename[100];
	 FILE *sfp;

	 /*! @todo
	   A single process on each socket should set governors for all 
	   cores on the socket.  Otherwise, if not all cores 
	   are participating, there could be interference in 
	   frequency selection.

	   @todo set NUM_FREQS, SLOWEST_FREQ, FASTEST_FREQ
	 */

	 get_cpuid(&my_core, &my_socket, &my_local);

	 // enable userspace governor
	 if(!socket_rank){
		 snprintf(filename, 100, "%s%u%s%s", cpufreq_path[0], my_core, 
			  cpufreq_path[1], cpufreq_governor);
		 sfp = fopen(filename, "w");
		 if(!sfp){
			 fprintf(stderr, 
				 "!!! unable to open governor selector %s\n", 
				 filename);
		 }
		 assert(sfp);
		 fprintf(sfp, "%s", "userspace");
		 fclose(sfp);
	 }

	 // Shift both processors to top gear.  This is clumsy, but
	 // we can clean it up later.
	 snprintf(filename, 100, "%s%u%s%s", cpufreq_path[0], my_core, 
		  cpufreq_path[1], cpufreq_speed);
	 sfp = fopen(filename, "w");
	 if(!sfp){
		 fprintf(stderr, "!!! %s does not exist.  Bye!\n", 
			 filename);
	 }
	 assert(sfp);
	 fprintf(sfp, "%u", freqs[ 0 ]);
	 fclose(sfp);

	 /*! @todo figure out what was going on here
	   sfp = fopen(cpufreq_filename[ 2 ], "w");
	   if(!sfp){
	   fprintf(stderr, 
	   "!!! cpufreq_filename[%d]=%s does not exist.  Bye!\n", 
	   cpuid, cpufreq_filename[cpuid]);
	   }
	   assert(sfp);
	   fprintf(sfp, "%s", freq_str[ 0 ]);
	   fclose(sfp);
	 */

	 shift_initialized=1;
	 return 0;
 }
 /*! @todo this needs to read frequency scaling info at runtime
  */
 int shift_core(int core, int freq_idx){
 #ifdef BLR_USE_SHIFT
	 char filename[100];
	 FILE *sfp;
 #endif
	 int temp_cpuid;
	 if(!shift_initialized){
		 shift_init();
	 }
	 get_cpuid(&temp_cpuid, &my_socket, &my_local);
	 assert( temp_cpuid == my_core );

	 if( freq_idx == prev_freq_idx[ my_core ] ){
		 return freq_idx;
	 }
	 prev_freq_idx[ my_core ] = freq_idx;

	 assert( (freq_idx >= 0) && (freq_idx < NUM_FREQS) );

	 //Make the change
 #ifdef BLR_USE_SHIFT
	 snprintf(filename, 100, "%s%u%s%s", cpufreq_path[0], my_core, 
		  cpufreq_path[1], cpufreq_speed);
	 sfp = fopen(filename, "w");
	 if(!sfp){
		 fprintf(stderr, "!!! %s does not exist.  Bye!\n", 
			 filename);
	 }
	 assert(sfp);
	 fprintf(sfp, "%u", freqs[ freq_idx ]);
	 fclose(sfp);
 #endif
	 current_freq = freq_idx;
	 return freq_idx;
 }

int shift_socket(int sock, int freq_idx){
	 assert(sock >= 0);
	 assert(sock < config.sockets);
	 int core_index;
	 for(core_index = 0; core_index < config.cores_per_socket; core_index++)
		 shift_core(config.map_socket_to_core[sock][core_index], freq_idx);
	 return 0;
}
