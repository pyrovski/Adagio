#!/bin/bash
pwd=`pwd`
./parse_alloc.sh>list
sudo bash -c "for i in \`cat $pwd/list\`; do echo \$i; /usr/bin/rsh \$i $pwd/modules.sh; done"
