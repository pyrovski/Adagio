/* -*- Mode: C; tab-width: 8; c-basic-offset: 8 -*- */

#include <assert.h>
#include <stdio.h>
#include "shift.h"
#include "cpuid.h"
#include "shm.h"
#include "shim.h"

#define BLR_USE_SHIFT

/*! @todo FASTEST_FREQ, turboboost_present, current_freq are initialized
  assuming turboboost is present.
 */
int NUM_FREQS, SLOWEST_FREQ, FASTEST_FREQ = 1;
int turboboost_present = 1;

static const char *cpufreq_path[] = {"/sys/devices/system/cpu/cpu",
				     "/cpufreq/"};
static const char cpufreq_governor[] = "scaling_governor";
static const char cpufreq_speed[] = "scaling_setspeed";
static const char cpufreq_min[] = "scaling_min_freq";
static const char cpufreq_max[] = "scaling_max_freq";
       
static const char cpufreq_frequencies[] = "scaling_available_frequencies";
static int freqs[MAX_NUM_FREQUENCIES];// in kHz, fastest to slowest
static int prev_freq_idx = -1;
static int shift_initialized = 0;

int shift_set_socket_governor(int socket, const char *governor_str){
	char filename[100];
	FILE *sfp;

	assert(socket >= 0);
	assert(socket < config.sockets);
	assert(governor_str);

	int core_index;
#ifdef BLR_USE_SHIFT
	for(core_index = 0; core_index < config.cores_per_socket; core_index++){
		snprintf(filename, 100, "%s%u%s%s", cpufreq_path[0], 
			 config.map_socket_to_core[socket][core_index], 
			 cpufreq_path[1], cpufreq_governor);
		sfp = fopen(filename, "w");
		if(!sfp){
			fprintf(stderr, 
				"!!! unable to open governor selector %s\n", 
				filename);
			return 1;
		}
		//assert(sfp);
		fprintf(sfp, "%s", governor_str);
		fclose(sfp);
	}
#endif
	return 0;
}

int shift_parse_freqs(){
	char filename[100];
	FILE *sfp;
	
	sprintf(filename, "%s%u%s%s", cpufreq_path[0], 0, cpufreq_path[1], 
		cpufreq_frequencies);
	
	sfp = fopen(filename, "r");

	assert(sfp);

	int status;

	while(!feof(sfp) && !ferror(sfp) && NUM_FREQS < MAX_NUM_FREQUENCIES){
		status = fscanf(sfp, "%u", &freqs[NUM_FREQS]);
		if(status != 1)
			break;
		frequencies[NUM_FREQS] = (double)freqs[NUM_FREQS] * 1000.0;
		NUM_FREQS++;
	}
	fclose(sfp);

	SLOWEST_FREQ = NUM_FREQS - 1;
	if(NUM_FREQS > 1){
		turboboost_present = (frequencies[0] / frequencies[1]) < 1.01;
	} else {
		turboboost_present = 0;
	}

	return 0;
}

int shift_core(int core, int freq_idx){
#ifdef BLR_USE_SHIFT
	char filename[100];
	FILE *sfp;
#endif
	assert(shift_initialized);
	assert( (freq_idx >= FASTEST_FREQ) && (freq_idx < NUM_FREQS) );

	int temp_cpuid;
	get_cpuid(&temp_cpuid, 0, 0);
	if(binding_stable)
		assert( temp_cpuid == my_core );

	//shm_mark_freq(freq_idx);

	if(my_core == core){
		if( freq_idx == prev_freq_idx)
			return freq_idx;
		
		prev_freq_idx = freq_idx;
	}

	//Make the change
#ifdef BLR_USE_SHIFT
	snprintf(filename, 100, "%s%u%s%s", cpufreq_path[0], core, 
		 cpufreq_path[1], cpufreq_speed);
	sfp = fopen(filename, "w");
	if(!sfp)
		perror(filename);
	assert(sfp);
#ifdef _DEBUG
	printf("rank %d socket %d rank %d core %d shifting core %d to %d\n", 
	       rank, my_socket, socket_rank, my_core, core, freq_idx);
#endif
	fprintf(sfp, "%u\n", freqs[ freq_idx ]);
	fclose(sfp);
#endif
	if(my_core == core)
		current_freq = freq_idx;
	return freq_idx;
}

int shift_socket(int sock, int freq_idx){
	 assert(sock >= 0);
	 assert(sock < config.sockets);
	 
	 int core_index;
#ifdef _DEBUG
	 printf("rank %d shifting %d cores on socket %d to %d\n", 
		rank,
		config.cores_per_socket, 
		sock, freq_idx);
#endif
	 for(core_index = 0; core_index < config.cores_per_socket; core_index++)
		 shift_core(config.map_socket_to_core[sock][core_index], freq_idx);
	 return 0;
}

int shift_set_socket_min_freq(int socket){
	 assert(socket >= 0);
	 assert(socket < config.sockets);
	 
	 int core_index;
	 FILE *sfp;
	 char filename[100];
#ifdef BLR_USE_SHIFT
	 for(core_index = 0; core_index < config.cores_per_socket; core_index++){
		 snprintf(filename, 100, "%s%u%s%s", cpufreq_path[0], 
			  config.map_socket_to_core[socket][core_index], 
			  cpufreq_path[1], cpufreq_min);
		 sfp = fopen(filename, "w");
		 if(!sfp)
			 perror(filename);
		 assert(sfp);
		 fprintf(sfp, "%u", freqs[ SLOWEST_FREQ ]);
		 fclose(sfp);
	 }
#endif
	 return 0;
}

int shift_set_socket_max_freq(int socket){
	 assert(socket >= 0);
	 assert(socket < config.sockets);
	 
	 int core_index;
	 FILE *sfp;
	 char filename[100];
#ifdef BLR_USE_SHIFT
	 for(core_index = 0; core_index < config.cores_per_socket; core_index++){
		 snprintf(filename, 100, "%s%u%s%s", cpufreq_path[0], 
			  config.map_socket_to_core[socket][core_index], 
			  cpufreq_path[1], cpufreq_max);
		 sfp = fopen(filename, "w");
		 if(!sfp)
			 perror(filename);
		 assert(sfp);
		 fprintf(sfp, "%u", freqs[ FASTEST_FREQ ]);
		 fclose(sfp);
	 }
#endif
	 return 0;
}

int shift_init_socket(int socket, const char *governor_str, int freq_idx){
	assert(socket >= 0);
	assert(socket < config.sockets);
	assert(governor_str);

	shift_set_socket_governor(socket, governor_str);
	shift_set_socket_min_freq(socket);
	shift_set_socket_max_freq(socket);
	shift_initialized = 1;
	
	/*!
	  @todo check to see if unrelated cores are affecting selected frequency
	*/
	/* set all cores to slowest frequency, 
	   thenx let individual cores select a higher frequency
	*/
	shift_socket(socket, freq_idx);
	return 0;
}

void shift_set_initialized(int init){
	shift_initialized = init;
}
