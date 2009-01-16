/******************************************************************************
 *
 *  File Name........: info.h
 *
 *  Description......:
 *
 *  Created by vin on 06/18/04
 *
 *  Revision History.:
 *
 *  $Log: info.h,v $
 *  Revision 1.4  2004/09/09 16:03:20  vin
 *  *** empty log message ***
 *
 *  Revision 1.3  2004/07/20 21:30:58  vin
 *  *** empty log message ***
 *
 *  Revision 1.2  2004/07/10 11:17:42  vin
 *  *** empty log message ***
 *
 *  Revision 1.1.1.1  2004/06/23 18:58:21  vin
 *  consolidated code
 *
 *
 *  $Id: info.h,v 1.4 2004/09/09 16:03:20 vin Exp $
 *
 *****************************************************************************/

#ifndef INFO_H
#define INFO_H

#define MAX_INFO_COUNTERS	4
struct info_S {
  unsigned long long cycles, nsecs;
  unsigned long long counters[MAX_INFO_COUNTERS];
};

typedef struct info_S *Info;

unsigned long initInfo(int, int, int);
unsigned int getEnergy();
int readInfo(int, Info);
void pmcPid(char, int);

#endif /* INFO_H */
/*........................ end of info.h ....................................*/
