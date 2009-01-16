/******************************************************************************
 *
 *  File Name........: gear.c
 *
 *  Description......:
 *
 *  Created by vin on 06/18/04
 *
 *  Revision History.:
 *
 *  $Log: gear.c,v $
 *  Revision 1.1  2004/09/09 16:04:26  vin
 *  *** empty log message ***
 *
 *  Revision 1.4  2004/07/21 15:26:02  vin
 *  *** empty log message ***
 *
 *  Revision 1.3  2004/07/20 21:30:58  vin
 *  *** empty log message ***
 *
 *  Revision 1.2  2004/07/10 11:17:42  vin
 *  *** empty log message ***
 *
 *  Revision 1.1.1.1  2004/06/23 18:58:22  vin
 *  consolidated code
 *
 *
 *  $Id: gear.c,v 1.1 2004/09/09 16:04:26 vin Exp $
 *
 *****************************************************************************/

#include "gear.h"
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#if 1
// new way
#define BASE	"/sys/devices/system/cpu/cpu0/cpufreq/"
static char *cpuinfoF = BASE"scaling_setspeed";
static char *cpufreqF = BASE"scaling_setspeed";
static char *gearString[] = {
  "2000000",
  "1800000",
  "1600000",
  "1400000",
  "1200000",
  "1000000",
  "800000",
  "end"
};

// pick values that are *between* the expected freqs
int freqs[] = { 2000000, 1800000, 1600000, 1400000, 1200000, 1000000, 800000 };

#else
// old way
static char *cpufreqF = "/proc/cpufreq";
static char *cpuinfoF = "/proc/cpuinfo";
static char *gearString[] = {
  "2000000:2000000:userspace",
  "1800000:1800000:userspace",
  "1600000:1600000:userspace",
  "1000000:1000000:userspace",
  "800000:800000:userspace",
  "end"
};

// pick values that are *between* the expected freqs
int freqs[] = { 2000, 1800, 1600, 1000, 800 };


//cat /proc/cpuinfo | grep "cpu MHz" | sed 's/ *//g' | cut -f2 -d ':' | cut -f 1 -d '.'
#endif

#define BUFLEN 511

static int Debug=0, Verbose=0;
static Gear Current;

void initGear(int debug, int verbose)
{
  whatGear();
  Debug = debug;
  Verbose = verbose;
}

/*-----------------------------------------------------------------------------
 *
 * Name...........: whatGear
 *
 * Description....: determine what gear we are in.
 *	Read the value from cpuinfoF (also can read from cpufreqF).
 *
 * Input Param(s).: 
 *
 * Return Value(s): Gear number
 *
 */

Gear whatGear() 
{
  FILE *fd;
  char *bp, buf[BUFLEN+1];
  char errorB[BUFLEN+1];
  int speed, gear;

  // tried to open this once and rewind between calls...didn't work
  // so we open it each time.
  fd = fopen(cpuinfoF, "r");
  if (fd == NULL) {
    sprintf(errorB, "whatGear: couldn't open %s", cpuinfoF);
    perror(errorB);
    exit(-1);
  }

  // skip to the line we are looking for (~7 lines)
  bp = fgets(buf, BUFLEN, fd);
  if (bp == NULL) {
    sprintf(errorB, "whatGear: read of %s failed", cpuinfoF);
    perror(errorB);
    return -1;
  }
  fclose(fd);
  speed = atoi(buf);

  if (Debug) printf("whatGear: speed read = %d \n", speed);

  for (gear = 0; gear < NUM_GEARS; gear++) {
    if (freqs[gear] <= speed)
      break;
  }

  Current = gear;
  return gear;
} /*---------- End of whatGear ----------------------------------------------*/

/*-----------------------------------------------------------------------------
 *
 * Name...........: shift
 *
 * Description....: 
 *
 * Input Param(s).: 
 *		Gear g -- 
 *
 * Return Value(s): 
 *
 */

int shift(Gear g) 
{
  int gg;
  char cmd[1024];

  if (g < 0 || g > NUM_GEARS) {
    if (Verbose || Debug) printf("shift: invalid gear %d\n", g);
    return -1;
  }

  // system seem to have problem shifting from 800 directly to 1000 !
  if (g == 5 && Current == 6) {
    if (shift(0)) {
      printf("shift failed\n");
      return 1;
    }
  }

  // shift by writing to /sys
  sprintf(cmd, "echo \"%s\" > %s", gearString[g], cpufreqF);
  if (Verbose||Debug) printf("shift: %s\n",  cmd);
			       
  system(cmd);

  // check that the shift worked
  gg = whatGear();
  if (gg == g) return 0;
  if (gg < 0) {
    printf("shift: cannot determine current gear\n");
    return -1;
  }
  printf("shift: failed to change to gear %d (in %d)\n", g, gg);
  return 1;
} /*---------- End of shift -------------------------------------------------*/

/*........................ end of gear.c ....................................*/
