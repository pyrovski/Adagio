#!/bin/bash
pwd=`pwd`
./parse_alloc.sh>list
bash -c "for i in \`cat $pwd/list\`; do echo \$i; /usr/bin/rsh \$i $pwd/modulesTest.sh; done"
