# Params
SPEEDS='2000' 
ITERATIONS="1"
DATEFMT="%s"

DATAFILE="$LOGS/ascii_opm_slack"
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
        
#        RUNCMD="$PREFIXCMD $nproc ./bin/$bmark.$nproc"
	    
	    echo [`date`] $bmark: $speed $nproc $iteration
	    
#	    bname=`basename $bmark $nproc`
	    
      # Write beginning of log entry
	    echo -n "$bmark $speed $nproc $iteration" >> $DATAFILE
	    
      # ------------- BENCHMARK RUNNING ---
        rm ~/tmp/pmc.log
        $BINBASEDIR/pmclogd -o ~/tmp/pmc.log
        $BINBASEDIR/pmcon
	    seconds_pre=`date +%s`
        energy_pre=0
        for (( i = 0 ; i < $nproc ; i++))
        do
            get_energy=`$GET_E pac0$i`
            energy_pre=$(($get_energy + $energy_pre))
        done
	    #$RUNCMD > /dev/null 2>&1 
	    aztec_script
	    seconds_post=`date +%s`
	    energy_post=0
        for (( i = 0 ; i < $nproc ; i++))
        do
            get_energy=`$GET_E pac0$i`
            energy_post=$(($get_energy + $energy_post))
        done
        $BINBASEDIR/pmcoff
        killall pmclogd
        grep $bmark ~/tmp/pmc.log | awk '{print $2, $6, $8, $9, $10}' > $LOGS/mem_bw/$bmark.$actual_speed.$nproc.$iteration
        aggregate=`grep $bmark ~/tmp/pmc.log | awk -f aggregate.awk`
      # ------------- BENCHMARK RUNNING ---      
	    
	    runtime=$(($seconds_post - $seconds_pre))
	    energy=$(($energy_post - $energy_pre))
	    
	    ///calculate slack
	    print_results 
      # Write rest of log entry
	    SLACK = `cat slack.dat`
	    echo " $runtime $energy $aggregate $SLACK" >> $DATAFILE
	    
	done
    done
done
done
