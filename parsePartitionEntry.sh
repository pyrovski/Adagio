#!/bin/bash
all=`cat`
root=`echo $all|cut -d'[' -f1`
range=`echo $all|cut -d'[' -f2`
start=`echo $range|tr ',' '\n'|cut -d'-' -f1`
end=`echo $range|tr ',' '\n'|cut -d'-' -f2`
j=0
for i in $start
do
j=$((j+1))
it_end=`echo $end|cut -d' ' -f$j`
  for k in `seq $i $it_end`
  do
    echo $root$k
  done
done