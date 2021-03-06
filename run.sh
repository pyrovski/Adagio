#!/bin/bash
if [ "$1" == "trace" ];
then 
trace="-genv OMPI_MCA_gmpi_trace all"
else 
trace=
fi
name=`date +%Y_%m_%d_%H_%M_%S`
#root=/p/l_ddn3/$USER/ParaDiS
benchmark=harness
root=$HOME/data/$benchmark
path=$root/$name
mkdir $path
env > $path/env
hostname >> $path/info
echo $name >> $path/info
uname -a >> $path/info
git log -n 1 --oneline >> $path/info
mpirun -n 1 cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies | cut -d' ' -f2- > $path/scaling_available_frequencies
command='harness -S'
echo 'command:'$command >> $path/info
oldDir=`pwd`
cd $path
cores=$SLURM_NPROCS
mpirun -n $SLURM_NPROCS MV2_CPU_BINDING_POLICY=bunch $trace $oldDir/$command >log 2>errlog

#cores=32
#mpirun -n $cores MV2_CPU_BINDING_POLICY=scatter $trace -genv OMPI_MCA_gmpi_bind collapse $oldDir/harness -S >log 2>errlog
# pernode gives segfaults...
echo 'cores:'$cores >> $path/info
mpirun -n $SLURM_NNODES bash -c "cat scaling_available_frequencies | cut -d' ' -f1 | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_max_freq"
mpirun -n $SLURM_NNODES bash -c "cat scaling_available_frequencies | sed -re 's/[[:space:]]+/\n/g'|sed -e '/^$/d'|tail -n 1|tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_min_freq"
cd ..
namez=$oldDir/$name.bz2
tar -cjvf $namez $name
#rsync --progress $namez lec.cs.arizona.edu:/home/pbailey/data/GreenMPI/$benchmark/

