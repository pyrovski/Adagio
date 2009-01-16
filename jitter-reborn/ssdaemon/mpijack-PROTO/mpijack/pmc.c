/******************************************************************************
 *
 *  File Name........: pmc.c
 *
 *  Description......:
 *
 *  Created by vin on 07/21/04
 *
 *  Revision History.:
 *
 *  $Log: pmc.c,v $
 *  Revision 1.2  2004/10/14 07:48:15  vin
 *  *** empty log message ***
 *
 *  Revision 1.1  2004/09/09 16:04:26  vin
 *  *** empty log message ***
 *
 *  Revision 1.1  2004/07/21 15:26:03  vin
 *  *** empty log message ***
 *
 *
 *  $Id: pmc.c,v 1.2 2004/10/14 07:48:15 vin Exp $
 *
 *****************************************************************************/

//#define PMCDIR	"/osr/users/mefemal/thesis/src/kernel/pmc/"
#define PMCDIR	"/osr/pac/bin/"
#include <stdio.h>
#include <stdlib.h>
int pmcInit()
{
  if (system("/sbin/lsmod | grep '^pmc' > /dev/null")) {
  //if (0) {
    printf("initInfo: pmc mod not installed\n");
    exit(-1);
  }

  return 0;
}

#define PMCLOG	"/proc/pmc/log"

FILE *pmcOpenLog() {
  return fopen(PMCLOG, "r");
}

#define BUFLEN 511

void pmcEvent(int counter, int event)
{
  char buffer[BUFLEN+1];
  
  sprintf(buffer, "%s/pmcctl -e %d,%x", PMCDIR, counter, event);
  if (system(buffer)) {
    printf("pmcEvent: pmcctl failed\n");
    exit(-1);
  }
}

/*-----------------------------------------------------------------------------
 *
 * Name...........: pmcPid
 *
 * Description....: 
 *
 * Input Param(s).: 
 *		char mode -- 'A' or 'R' to add or remove pid
 *		int pid -- 
 *
 * Return Value(s): 
 *
 */

void pmcPid(char mode, int pid)
{
  char buf[BUFLEN+1];

  if (pid < 0)
    sprintf(buf, "%spmcctl", PMCDIR, pid);
  else {
    sprintf(buf, "%spmcctl -%c -p %d", PMCDIR, mode, pid);
  }
  if (system(buf)) {
    printf("pmcPid: pmcctl failed\n");
    exit(-1);
  }
} /*---------- End of pmcPid ------------------------------------------------*/

/*-----------------------------------------------------------------------------
 *
 * Name...........: pmcSwitch
 *
 * Description....: 
 *
 * Input Param(s).: 
 *		int on_off -- turn pmc logging on or off
 *
 * Return Value(s): 
 *
 */

void pmcSwitch(int on_off)
{
  if (on_off)
  {
    if(system(PMCDIR"pmcon"))
        printf("something is wrong\n");
  }
  else {
    system(PMCDIR"pmcoff");
    system("cat "PMCLOG" >/dev/null");
  }
} /*---------- End of pmcSwitch ---------------------------------------------*/

/*........................ end of pmc.c .....................................*/
