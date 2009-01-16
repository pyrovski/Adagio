node0=1200000
node1=1200000
node2=1200000
node3=1200000
node4=1200000
node5=1200000
node6=1200000
node7=1200000


ssh pac00 "echo $node0 > /sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed"
ssh pac01 "echo $node1 > /sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed"
ssh pac02 "echo $node2 > /sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed"
ssh pac03 "echo $node3 > /sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed"
ssh pac04 "echo $node4 > /sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed"
ssh pac05 "echo $node5 > /sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed"
ssh pac06 "echo $node6 > /sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed"
ssh pac07 "echo $node7 > /sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed"



