#ifndef BLR_SHIFT_H
#define BLR_SHIFT_H

int shift_core(int core, int freq_idx);
int shift_socket(int sock, int freq_idx);

extern int NUM_FREQS, SLOWEST_FREQ, FASTEST_FREQ;

#endif  //BLR_SHIFT_H

