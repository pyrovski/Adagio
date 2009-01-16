node0=2000000
node1=2000000
node2=2000000
node3=2000000
node4=2000000
node5=2000000
node6=2000000
node7=2000000


ssh pac00 "echo $node0 > /sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed"
ssh pac01 "echo $node1 > /sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed"
ssh pac02 "echo $node2 > /sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed"
ssh pac03 "echo $node3 > /sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed"
ssh pac04 "echo $node4 > /sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed"
ssh pac05 "echo $node5 > /sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed"
ssh pac06 "echo $node6 > /sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed"
ssh pac08 "echo $node7 > /sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed"



