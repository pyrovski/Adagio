/******************************************************************************
 *
 *  File Name........: pmc.h
 *
 *  Description......:
 *
 *  Created by vin on 07/21/04
 *
 *  Revision History.:
 *
 *  $Log: pmc.h,v $
 *  Revision 1.1  2004/09/09 16:03:55  vin
 *  *** empty log message ***
 *
 *  Revision 1.1  2004/07/21 15:26:03  vin
 *  *** empty log message ***
 *
 *
 *  $Id: pmc.h,v 1.1 2004/09/09 16:03:55 vin Exp $
 *
 *****************************************************************************/

#ifndef PMC_H
#define PMC_H

int pmcInit();
FILE *pmcOpenLog();
void pmcPid(char, int);
void pmcSwitch(int);
void pmcEvent(int, int);

#endif /* PMC_H */
/*........................ end of pmc.h .....................................*/
