#ifndef BLR_SHIFT_H
#define BLR_SHIFT_H

#define MAX_NUM_FREQUENCIES 16

int shift_set_socket_governor(int socket, const char *governor_str);
int shift_parse_freqs();
int shift_core(int core, int freq_idx);
int shift_socket(int sock, int freq_idx);
int shift_init_socket(int socket, const char *governor_str, int freq_idx);
void shift_set_initialized(int);

extern int NUM_FREQS, SLOWEST_FREQ, FASTEST_FREQ;
extern int turboboost_present;

#endif  //BLR_SHIFT_H

