#!/bin/bash
for i in `seq 1 16`; do rsh hyperion-fio$i bash -c 'chmod go+rw /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor;chmod go+rw /sys/devices/system/cpu/cpu*/cpufreq/scaling_setspeed;chmod go+rw /sys/devices/system/cpu/cpu*/cpufreq/scaling_min_freq;chmod go+rw /sys/devices/system/cpu/cpu*/cpufreq/scaling_max_freq'; done
