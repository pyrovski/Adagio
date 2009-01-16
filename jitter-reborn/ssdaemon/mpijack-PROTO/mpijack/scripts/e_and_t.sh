

GET_E0="netclient -H pac00 "
GET_E1="netclient -H pac01 "
GET_E2="netclient -H pac02 "
GET_E3="netclient -H pac03 "
GET_E4="netclient -H pac04 "
GET_E5="netclient -H pac05 "
GET_E6="netclient -H pac06 "
GET_E7="netclient -H pac07 "


energy_pre0=`$GET_E0 | awk '{print $1}'`
energy_pre1=`$GET_E1 | awk '{print $1}'`
energy_pre2=`$GET_E2 | awk '{print $1}'`
energy_pre3=`$GET_E3 | awk '{print $1}'`
energy_pre4=`$GET_E4 | awk '{print $1}'`
energy_pre5=`$GET_E5 | awk '{print $1}'`
energy_pre6=`$GET_E6 | awk '{print $1}'`
energy_pre7=`$GET_E7 | awk '{print $1}'`

time_pre=`date +%s`

#mpirun -np 8 jacobi 2048 4096 95 1024 5 1 

time_post=`date +%s`
energy_post0=`$GET_E0 | awk '{print $1}'`
energy_post1=`$GET_E1 | awk '{print $1}'`
energy_post2=`$GET_E2 | awk '{print $1}'`
energy_post3=`$GET_E3 | awk '{print $1}'`
energy_post4=`$GET_E4 | awk '{print $1}'`
energy_post5=`$GET_E5 | awk '{print $1}'`
energy_post6=`$GET_E6 | awk '{print $1}'`
energy_post7=`$GET_E7 | awk '{print $1}'`

echo "Energy in Joules for nodes 0-7"
e0=$(($energy_post0 - $energy_pre0))
e1=$(($energy_post1 - $energy_pre1))
e2=$(($energy_post2 - $energy_pre2))
e3=$(($energy_post3 - $energy_pre3))
e4=$(($energy_post4 - $energy_pre4))
e5=$(($energy_post5 - $energy_pre5))
e6=$(($energy_post6 - $energy_pre6))
e7=$(($energy_post7 - $energy_pre7))

echo $e0
echo $e1
echo $e2
echo $e3
echo $e4
echo $e5
echo $e6
echo $e7

time_spent=$(($time_post - $time_pre))
total_energy=$(($e0 + $e1 + $e2 + $e3 + $e4 + $e5 + $e6 +$e7 ))

echo "Time spent " $time_spent
echo "Total energy " $total_energy
