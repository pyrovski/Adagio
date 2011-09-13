#!/bin/bash
pwd=`pwd`
list=`./parse_alloc.sh`
for i in $list
do
echo $i
/usr/bin/rsh e$i $pwd/permissions.sh $1 &
done
wait
