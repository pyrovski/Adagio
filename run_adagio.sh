#!/bin/bash
if [ "$1" == "trace" ];
then 
trace="-mca gmpi_trace all"
else 
trace=
fi
name=`date +%Y_%m_%d_%H_%M_%S`
#root=/p/l_ddn3/$USER/ParaDiS
root=$HOME/data/ParaDiS
path=$root/$name
mkdir $path
config=./blr.cn.16nodes.050steps.lb0
cp $config ./blr.data ../Inputs/Rijm.cube.out ../Inputs/RijmPBC.cube.out ../Inputs/fm-ctab.m2.t4.dat $path/
env > $path/env
echo "algo = adagio" > $path/info
hostname >> $path/info
echo $name >> $path/info
uname -a >> $path/info
git log -n 1 --oneline >> $path/info
mpirun -n 1 cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_available_frequencies | cut -d' ' -f2- > $path/scaling_available_frequencies
oldDir=`pwd`
cd $path
mpirun -n $SLURM_NPROCS -bind-to-core -report-bindings $trace -mca gmpi_algo adagio dd3d -d ./blr.data $config > log 2>errlog
#mpirun -n 32 -cpus-per-proc 4 -report-bindings $trace -mca gmpi_bind collapse -mca gmpi_algo adagio dd3d -d ./blr.data $config >log 2> errlog
mpirun -n $SLURM_NNODES bash -c "cat scaling_available_frequencies | cut -d' ' -f1 | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_max_freq"
mpirun -n $SLURM_NNODES bash -c "cat scaling_available_frequencies | sed -re 's/[[:space:]]+/\n/g'|sed -e '/^$/d'|tail -n 1|tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_min_freq"
#mpirun -n $SLURM_NNODES bash -c "echo ondemand | tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor"
cd ..
#namez=$name.tgz
namez=$oldDir/$name.bz2
#tar -czvf $namez $name
tar -cjvf $namez $name
rsync --progress $namez lec.cs.arizona.edu:/home/pbailey/data/GreenMPI/ParaDiS/
