#!/bin/bash

speed=$(($1*1000))
ssh pac00 echo $speed \> /sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed 2> /dev/null
ssh pac01 echo $speed \> /sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed 2> /dev/null
ssh pac02 echo $speed \> /sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed 2> /dev/null
ssh pac03 echo $speed \> /sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed 2> /dev/null
ssh pac04 echo $speed \> /sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed 2> /dev/null
ssh pac05 echo $speed \> /sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed 2> /dev/null
ssh pac06 echo $speed \> /sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed 2> /dev/null
ssh pac07 echo $speed \> /sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed 2> /dev/null
ssh pac08 echo $speed \> /sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed 2> /dev/null
ssh pac09 echo $speed \> /sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed 2> /dev/null
