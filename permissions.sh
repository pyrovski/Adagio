#!/bin/bash
if [ "$1" == 'off' ]
then
perm=go+r-w
else
perm=go+rw
fi
chmod $perm /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor /sys/devices/system/cpu/cpu*/cpufreq/scaling_setspeed /sys/devices/system/cpu/cpu*/cpufreq/scaling_min_freq /sys/devices/system/cpu/cpu*/cpufreq/scaling_max_freq
