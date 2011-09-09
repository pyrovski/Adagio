#!/bin/bash
pwd=`pwd`
#for i in `seq 1 16`; do rsh hyperion-fio$i $pwd/permissions.sh; done
list=`./parse_pbatch3.sh`
for i in $list
do
rsh $i $pwd/permissions.sh $1 &
done
wait
