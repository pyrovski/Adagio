#ifndef BLR_SHIFT_H
#define BLR_SHIFT_H

#define MAX_NUM_FREQUENCIES 16

int shift_parse_freqs();
int shift_core(int core, int freq_idx);
int shift_socket(int sock, int freq_idx);

extern int NUM_FREQS, SLOWEST_FREQ, FASTEST_FREQ;

#endif  //BLR_SHIFT_H

