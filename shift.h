#include "machine.h"
#ifndef BLR_SHIFT_H
#define BLR_SHIFT_H
int shift(int freq_idx);
//These have to be negative since actual frequencies are 
//  non-negative integers.
enum{ NO_SHIFT=-1, GO_FASTER=-2, GO_FASTEST=-3, GO_SLOWER=-4, GO_SLOWEST=-5 };
#endif  //BLR_SHIFT_H
