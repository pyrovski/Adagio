echo userspace | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor > /dev/null
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies | cut -d' ' -f1 | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_max_freq > /dev/null
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies | tr -s ' ' | tr ' ' '\n'|sed -e '/^$/d'|tail -n 1|tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_min_freq > /dev/null
cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies | cut -d' ' -f1 | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_setspeed > /dev/null
$HOME/local/bin/turbo off
$HOME/local/bin/rapl_clamp off
