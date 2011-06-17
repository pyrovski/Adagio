#!/bin/bash
chmod go+rw /sys/devices/system/cpu/cpu*/cpufreq/scaling_setspeed
chmod go+rw /sys/devices/system/cpu/cpu*/cpufreq/scaling_min_freq
chmod go+rw /sys/devices/system/cpu/cpu*/cpufreq/scaling_max_freq
