/******************************************************************************
 *
 *  File Name........: eMeter.h
 *
 *  Description......:
 *
 *  Created by vin on 06/23/04
 *
 *  Revision History.:
 *
 *  $Log: eMeter.h,v $
 *  Revision 1.2  2004/08/02 20:49:41  vin
 *  *** empty log message ***
 *
 *  Revision 1.1.1.1  2004/06/23 18:58:19  vin
 *  consolidated code
 *
 *
 *  $Id: eMeter.h,v 1.2 2004/08/02 20:49:41 vin Exp $
 *
 *****************************************************************************/

#ifndef NETMETER_H
#define NETMETER_H

int eMeterInit();
unsigned int eMeterRead();
double eMeterReadP();
int eMeterClose();

#endif /* NETMETER_H */
/*........................ end of eMeter.h ..................................*/
