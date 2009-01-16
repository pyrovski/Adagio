#!/bin/bash

# script.sh - Run $BMARKS through $SPEEDS, $ITERATIONS times
#
# $Id: script.sh,v 1.2 2004/02/18 21:37:41 Exp $

# Params
#SPEEDS='2000 1800 1600 1400 1200 1000 800'
#SPEEDS='2000 1800 1600 1400'
#SPEEDS='2000 1800 1600 1400 1200' 
SPEEDS='2000' 
ITERATIONS="1"
#DATEFMT="%m-%d-%Y-%H:%M:%S"
DATEFMT="%s"

# Locations
#BMARKS="bin/cg.B bin/lu.C bin/mg.B"
#BMARKS="/osr/Projects/ampere/NPB3.1/NPB3.1-MPI/bin/cg.B"
BMARKS='cg.B'
#BMARKS="mg.B lu.B ep.B cg.B"
#BMARKS="sp.A bt.A sp.C bt.C"
#BMARKS="cg.C lu.C is.C mg.C"
NPROCS="8" # 4 8
#NPROCS="1 4 9"
PREFIXCMD="mpirun -np"
LOGS="/osr/Projects/PARTS/amd64/logs/new_meter"
#LOGS="."
DATAFILE="$LOGS/cg_check"
GET_E="/osr/pac/bin/mclient -U"
BINBASEDIR="/osr/pac/bin"
LIBBASEDIR="/osr/pac/lib/modules"

$BINBASEDIR/pmcctl -e 0,41
$BINBASEDIR/pmcctl -e 1,81
$BINBASEDIR/pmcctl -e 2,C1

for bmark in $BMARKS; do
    for speed in $SPEEDS; do
       change-speed.sh $speed
    for nproc in $NPROCS; do
	for iteration in $ITERATIONS; do 
        
        RUNCMD="$PREFIXCMD $nproc ./bin/$bmark.$nproc"
#       RUNCMD="$PREFIXCMD $nproc $bmark.$nproc"
	    
	    echo [`date`] $bmark: $speed $nproc $iteration
	    
	    bname=`basename $bmark $nproc`
	    
      # Write beginning of log entry
	    echo -n "$bmark $speed $nproc $iteration" >> $DATAFILE
	    
      # ------------- BENCHMARK RUNNING ---
        rm ./tmp/pmc.log
        $BINBASEDIR/pmclogd -o ./tmp/pmc.log
        $BINBASEDIR/pmcon
	    seconds_pre=0
        energy_pre=0
        for (( i = 0 ; i < $nproc ; i++))
        do
            get_energy=`$GET_E pac0$i`
            energy_pre=$(($get_energy + $energy_pre))
        done
	    $RUNCMD #> /dev/null 2>&1 
#	    ./bin/mg.B.1

#	    mpirun -np 8 $bmark
	    
	    seconds_post=`cat progtime.dat`
	    runtime=`cat progtime.dat`
	    echo "Runtime  $runtime"
	    energy_post=0
        for (( i = 0 ; i < $nproc ; i++))
        do
            get_energy=`$GET_E pac0$i`
            energy_post=$(($get_energy + $energy_post))
        done
        $BINBASEDIR/pmcoff
        killall pmclogd
        #grep $bmark ./tmp/pmc.log | awk '{print $2, $6, $8, $9, $10}' > $LOGS/mem_bw/$bmark.$actual_speed.$nproc.$iteration
	#aggregate=`grep $bmark ./tmp/pmc.log | awk -f aggregate.awk`

      # ------------- BENCHMARK RUNNING ---      
	    
	    #runtime=$(($seconds_post - $seconds_pre))
	    energy=$(($energy_post - $energy_pre))
	    
	    #calculate slack
	    find-slack.sh 
      # Write rest of log entry
	    SLACK=`awk -f geometric.awk slack.dat`
	    echo " $runtime $energy $aggregate $SLACK" >> $DATAFILE
	    #echo $aggregate
	   # echo " $runtime $energy $SLACK" >> $DATAFILE
	    
	done
    done
done
done
