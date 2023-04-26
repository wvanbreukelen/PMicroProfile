#!/bin/bash

if [ "$#" -ne 3 ]; then
    echo "Usage: sudo ./run_fs.sh <fs> <run_id> <workload>"
    exit 1
fi

set -x

workload=filebench
fs=$1
run_id=$2
workload=$3
current_dir=$(pwd)
filebench_dir=`readlink -f ../../filebench`
workload_dir=$filebench_dir/workloads
pmem_dir=/mnt/pmem_emul
boost_dir=`readlink -f ../../splitfs`
result_dir=`readlink -f ../../results`
fs_results=$result_dir/$fs/$workload

pre_cmd="sudo"
#pre_cmd="sudo pmemtrace bla --sample-rate 30 --duty-cycle 4.0"
#pre_cmd="sudo env "PATH=$PATH" pmemtrace /dev/ndctl0"

if [ "$fs" == "splitfs-posix" ]; then
    run_boost=1
elif [ "$fs" == "splitfs-sync" ]; then
    run_boost=1
elif [ "$fs" == "splitfs-strict" ]; then
    run_boost=1
else
    run_boost=0
fi

sudo apt-get install cpufrequtils -y

echo 'GOVERNOR="performance"' | sudo tee /etc/default/cpufrequtils

ulimit -c unlimited
echo 0 | sudo tee /proc/sys/kernel/randomize_va_space
sync; sudo sh -c "echo 3 > /proc/sys/vm/drop_caches"

sudo rm -rf /mnt/pmem_emul/*
sync

echo Sleeping for 5 seconds ..
sleep 5

run_workload()
{

    echo ----------------------- FILEBENCH WORKLOAD ---------------------------

    mkdir -p $fs_results
    rm $fs_results/run$run_id

    rm -rf $pmem_dir/*

    date

    if [ $run_boost -eq 1 ]; then
        $pre_cmd $boost_dir/run_boost.sh -p $boost_dir -t nvp_nvp.tree $filebench_dir/filebench -f $workload_dir/$workload 2>&1 | tee $fs_results/run$run_id
    else
        $pre_cmd $filebench_dir/filebench -f $workload_dir/$workload 2>&1 | tee $fs_results/run$run_id
    fi

    date

    cd $current_dir

    echo Sleeping for 5 seconds . .
    sleep 5

}

run_workload

cd $current_dir
