#ifndef BLR_MACHINE_H
#define BLR_MACHINE_H
enum{ NUM_FREQ=5, NUM_FREQS=5, SLOWEST_FREQ=4 };
double frequency[NUM_FREQS] = {1.8, 1.6, 1.4, 1.2, 1.0};
#define GMPI_MIN_COMP_SECONDS (0.1)	// In seconds.
#define GMPI_BLOCKING_BUFFER (0.1)	// In seconds.
#endif //BLR_MACHINE_H
