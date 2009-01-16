#!/bin/bash
for i in `seq 20`
do
	mpirun -np 32 -hostfile $HOSTFILE ./hello.nosched >> hello.nosched.out 2>&1
	mpirun -np 32 -hostfile $HOSTFILE ./hello.jitter >> hello.jitter.out 2>&1
done

