/******************************************************************************
 *
 *  File Name........: gear.h
 *
 *  Description......:
 *
 *  Created by vin on 06/18/04
 *
 *  Revision History.:
 *
 *  $Log: gear.h,v $
 *  Revision 1.1  2004/09/09 16:03:55  vin
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
 *  $Id: gear.h,v 1.1 2004/09/09 16:03:55 vin Exp $
 *
 *****************************************************************************/

#ifndef GEAR_H
#define GEAR_H

typedef int Gear;

#define NUM_GEARS 7
#define FASTEST_GEAR 0
#define SLOWEST_GEAR (NUM_GEARS - 1)

void initGear(int, int);
Gear whatGear();
int shift(Gear);

#endif /* GEAR_H */
/*........................ end of gear.h ....................................*/
