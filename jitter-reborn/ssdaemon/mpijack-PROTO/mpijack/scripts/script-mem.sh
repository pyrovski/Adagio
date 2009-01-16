#!/bin/bash

# script.sh - Run $BMARKS through $SPEEDS, $ITERATIONS times
#
# $Id: script.sh,v 1.2 2004/02/18 21:37:41 dmsmith2 Exp $

# Params
#SPEEDS='2000 1800 1600 1400 1200 800'
SPEEDS='2000'
ITERATIONS="600"
#DATEFMT="%m-%d-%Y-%H:%M:%S"
DATEFMT="%s"

# Locations
#BMARKS="bin/cg.B bin/lu.C bin/mg.B"
#BMARKS="bin/is.C"
#BMARKS="mg.B is.C lu.B ep.B sp.B bt.C"
BMARKS="jacobi"

WIDTHS="6000"
#HEIGHTS="6000 3000 1500 1000 750 600"
HEIGHTS="600"

NPROCS="1"
LOGS="/osr/Projects/PARTS/amd64/logs"
DATAFILE="$LOGS/miss_uop"
GET_E="/osr/Projects/PARTS/src/net_client/net_client"

PREFIXCMD="mpirun -np"

BINBASEDIR="/osr/cluster/bin"
LIBBASEDIR="/osr/cluster/lib/modules"


$BINBASEDIR/pmcctl -e 0,41
$BINBASEDIR/pmcctl -e 1,81
$BINBASEDIR/pmcctl -e 2,C1


change_freq() {

    khz_speed=$(($1 * 1000))
    
    echo $khz_speed > /sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed

    cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed

}

for bmark in $BMARKS; do
    for speed in $SPEEDS; do
        
      # Switch to correct speed
	    actual_speed=`change_freq $speed `
                                                                
	for iteration in $ITERATIONS; do 
        for width in $WIDTHS; do
            for height in $HEIGHTS; do
                for nproc in $NPROCS; do
        
        RUNCMD="$bmark $width $height $iteration 1 0"
	    
	    echo [`date`] $bmark: $speed $nproc $width $height $iteration
	    
	    bname=`basename $bmark $nproc`
	    
      # Write beginning of log entry
	    echo -n "$bmark $actual_speed $nproc $width $height $iteration" >> $DATAFILE
	    

        
      # ------------- BENCHMARK RUNNING ---
	    rm ~/tmp/pmc.log
        $BINBASEDIR/pmclogd -o ~/tmp/pmc.log
        $BINBASEDIR/pmcon
                          
      seconds_pre=`date +%s`
	    #energy_pre=`$GET_E`
	    $RUNCMD 2>&1 
	    seconds_post=`date +%s`
	    #energy_post=`$GET_E`
        $BINBASEDIR/pmcoff
        killall pmclogd
        aggregate=`grep $bmark ~/tmp/pmc.log | awk -f aggregate.awk`        
      # ------------- BENCHMARK RUNNING ---      
	    
	    runtime=$(($seconds_post - $seconds_pre))
	    #energy=$(($energy_post - $energy_pre))

	    datestamp=`date +$DATEFMT`
	    
      # Write rest of log entry
	    echo " $datestamp $runtime $aggregate" >> $DATAFILE
	done
    done
done 
done
done
done
