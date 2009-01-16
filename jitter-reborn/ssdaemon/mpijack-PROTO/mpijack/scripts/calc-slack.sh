awk '{if (NR > 2) print $2/$4}' node0.dat | awk -f average.awk
awk '{if (NR > 2) print $2/$4}' node1.dat | awk -f average.awk
awk '{if (NR > 2) print $2/$4}' node2.dat | awk -f average.awk
awk '{if (NR > 2) print $2/$4}' node3.dat | awk -f average.awk
awk '{if (NR > 2) print $2/$4}' node4.dat | awk -f average.awk
awk '{if (NR > 2) print $2/$4}' node5.dat | awk -f average.awk
awk '{if (NR > 2) print $2/$4}' node6.dat | awk -f average.awk
awk '{if (NR > 2) print $2/$4}' node7.dat | awk -f average.awk
