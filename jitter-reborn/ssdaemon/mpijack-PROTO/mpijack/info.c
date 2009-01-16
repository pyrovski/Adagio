/******************************************************************************
 *
 *  File Name........: info.c
 *
 *  Description......:
 *
 *  Created by vin on 06/18/04
 *
 *  Revision History.:
 *
 *  $Log: info.c,v $
 *  Revision 1.5  2004/09/09 16:03:20  vin
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
 *  $Id: info.c,v 1.5 2004/09/09 16:03:20 vin Exp $
 *
 *****************************************************************************/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <sys/poll.h>
#include <stdlib.h>

#include "info.h"
#include "pmc.h"
#include "net.h"
#include "eMeter.h"
#include "util.h"

#define SENTINEL 99
#define RETRIES  3
#define TIMEOUT  500

#define BUFLEN	511
static int Debug=0, Verbose=0;

/*-----------------------------------------------------------------------------
 *
 * Name...........: initInfo
 *
 * Description....: 
 *	Stolen from net_client.c
 *
 * Input Param(s).: 
 *
 * Return Value(s): 
 *
 */

unsigned long initInfo(int pid, int debug, int verbose)
{
  Debug = debug;
  Verbose = verbose;
  
  pmcInit();
  pmcSwitch(0);			/* clear log file */
  
  pmcPid('A', pid);
  pmcSwitch(1);

  //  now();			/* initialize timer */
  readTsc();			/* initialize counter */

  return 0;
} /*---------- End of initInfo ----------------------------------------------*/

/*-----------------------------------------------------------------------------
 *
 * Name...........: readInfo
 *
 * Description....: 
 *
 * Input Param(s).: 
 *		Info info -- 
 *
 * Return Value(s): 
 *
 */

int readInfo(int ncounters, Info info)
{
  FILE *fd;
  char *bp, buffer[BUFLEN+1];
  unsigned long long ns, rt, tsc;
  unsigned long long counters[MAX_INFO_COUNTERS], icounters[MAX_INFO_COUNTERS];
  int i, lp, pid, uid, rc, prio;
  char name[BUFLEN+1];
  
  fd = pmcOpenLog();
  if (fd == NULL) {
    perror("readInfo: couldn't open pmc log file PMC log\n");
    exit(-1);
  }

  for (i = 0; i < ncounters; ++i)
    counters[i] = 0;

  while ((bp = fgets(buffer, BUFLEN, fd)) != NULL) {
    if (*bp == '#')
      continue;
    switch (ncounters) {
    case 0:
      rc = sscanf(buffer, "%d %s %d %d %d %llu %llu", &lp, &name, &pid, &uid, &prio, &tsc, &rt); 
      break;
    case 1:
      rc = sscanf(buffer, "%d %s %d %d %llu %llu", &lp, &name, &pid, &uid, &prio, &tsc, &rt,
		  icounters); 
      break;
    case 2:
      rc = sscanf(buffer, "%d %s %d %d %d %llu %llu %llu %llu", 
		  &lp, &name, &pid, &uid, &prio, &tsc, &rt,
		  icounters, icounters+1); 
      break;
    case 3:
      rc = sscanf(buffer, "%d %s %d %d %llu %llu %llu %llu", 
		  &lp, &name, &pid, &uid, &prio, &tsc, &rt,
		  icounters, icounters+1, icounters+2); 
      break;
    case 4:
      rc = sscanf(buffer, "%d %s %d %d %llu %llu %llu %llu %llu", 
		  &lp, &name, &pid, &uid, &rt,
		  icounters, icounters+1, icounters+2, icounters+3); 
      break;
    default:
      printf("readInfo: ncounters invalid %d\n", ncounters);
      exit(-1);
      break;
    }
    if (rc != 5 + ncounters) {
      //      perror("readInfo: fscanf didn't scan correct number of items\n");
      //      exit(-1);
    }

    for (i = 0; i < ncounters; ++i) {
      counters[i] += icounters[i];
    }
    ns += rt;
  }
  info->cycles = readTsc();
  info->nsecs = ns;
  for (i = 0; i < ncounters; ++i)
    info->counters[i] = counters[i];

  fclose(fd);

  return 0;
} /*---------- End of readInfo ----------------------------------------------*/

/*........................ end of info.c ....................................*/
