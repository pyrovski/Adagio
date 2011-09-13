#!/bin/bash
pwd=`pwd`
#for i in `seq 1 16`; do rsh hyperion-fio$i $pwd/permissions.sh; done
list=`./parse_prountree.sh`
for i in $list
do
/usr/bin/rsh e$i $pwd/permissions.sh $1 &
done
wait
