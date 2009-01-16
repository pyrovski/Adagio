SLACK0=`awk '{if(NR > 1) print $2/$4}' node0.dat | awk -f average.awk`
#echo "$1 $2 0 $SLACK0"
SLACK1=`awk '{if(NR > 1) print $2/$4}' node1.dat | awk -f average.awk`
#echo "$1 $2 1 $SLACK1"
SLACK2=`awk '{if(NR > 1) print $2/$4}' node2.dat | awk -f average.awk`
#echo "$1 $2 2 $SLACK2"
SLACK3=`awk '{if(NR > 1) print $2/$4}' node3.dat | awk -f average.awk`
#echo "$1 $2 3 $SLACK3"
SLACK4=`awk '{if(NR > 1) print $2/$4}' node4.dat | awk -f average.awk`
#echo "$1 $2 4 $SLACK4"
SLACK5=`awk '{if(NR > 1) print $2/$4}' node5.dat | awk -f average.awk`
#echo "$1 $2 5 $SLACK5"
SLACK6=`awk '{if(NR > 1) print $2/$4}' node6.dat | awk -f average.awk`
#echo "$1 $2 6 $SLACK6"
SLACK7=`awk '{if(NR > 1) print $2/$4}' node7.dat | awk -f average.awk`
#echo "$1 $2 7 $SLACK7"

SLACK=`echo $SLACK0 + $SLACK1 + $SLACK2 + $SLACK3 + $SLACK4 + $SLACK5 + $SLACK6 + $SLACK7|bc -l`
SLACK=`echo $SLACK/8|bc -l`

echo $SLACK #> slack.dat

