#!/bin/bash
pwd=`pwd`
for i in `seq 1 16`; do rsh hyperion-fio$i $pwd/permissions.sh; done
