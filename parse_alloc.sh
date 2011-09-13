#!/bin/bash
#! @todo this won't work; env for sudo is not env for user
# how to drop sudo?
env|grep SLURM_NODELIST|cut -d'=' -f2|tr ']' '\n'|sed -e 's/^,//' -e '/^$/d'|xargs -I{} bash -c 'echo {} | ./parsePartitionEntry.sh'
