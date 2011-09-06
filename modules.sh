#!/bin/bash

for i in `seq 1 16`; do rsh hyperion-fio$i 'hostname; /sbin/insmod /home/bailey56/code/GreenMPI/src/perfmod/perfmod.ko'; done