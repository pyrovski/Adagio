#!/bin/bash
hostname
if [ "$1" == 'off' ]
then
/sbin/rmmod perfmod
else
/sbin/insmod /home/bailey56/code/GreenMPI/src/perfmod/perfmod.ko
fi
#/sbin/insmod /home/bailey56/code/GreenMPI/src/perfmod/perfmod.ko


