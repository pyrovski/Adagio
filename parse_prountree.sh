#!/bin/bash
cat /etc/slurm/slurm.conf|grep prountree|egrep -v '#'|grep -o 'Nodes.*]'|cut -d'=' -f2|tr ']' '\n'|sed -e 's/^,//' -e '/^$/d'|xargs -I{} bash -c 'echo {} | ./parsePartitionEntry.sh'
