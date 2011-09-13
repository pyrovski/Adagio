#!/bin/bash
pwd=`pwd`
#for i in `seq 1 16`; do rsh hyperion-fio$i $pwd/modules.sh; done
list=`./parse_prountree.sh`
for i in $list
do
rsh $i $pwd/modules.sh&
done
wait