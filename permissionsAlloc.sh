#!/bin/bash
pwd=`pwd`
./parse_alloc.sh>list
sudo bash -c "for i in \`cat $pwd/list\`; do /usr/bin/rsh \$i $pwd/permissions.sh $1; done"
#wait
