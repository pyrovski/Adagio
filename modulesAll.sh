#!/bin/bash
pwd=`pwd`
#for i in `seq 1 16`; do rsh hyperion-fio$i $pwd/modules.sh; done
list=`./parse_pbatch3.sh`
for i in $list
do
/usr/bin/rsh $i $pwd/modules.sh&
done
wait
