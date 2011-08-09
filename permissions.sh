#!/bin/bash
for i in `seq 1 16`; do rsh hyperion-fio$i chmod go+rw /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor /sys/devices/system/cpu/cpu*/cpufreq/scaling_setspeed /sys/devices/system/cpu/cpu*/cpufreq/scaling_min_freq /sys/devices/system/cpu/cpu*/cpufreq/scaling_max_freq; done
