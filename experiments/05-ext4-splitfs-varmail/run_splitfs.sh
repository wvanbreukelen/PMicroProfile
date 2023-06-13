#!/bin/bash

# We need to turn off VA randomization for filebench -> https://github.com/filebench/filebench/issues/110 
# ;0

echo 0 | sudo tee /proc/sys/kernel/randomize_va_space

current_dir=$(pwd)
setup_dir=`readlink -f ../../splitfs/scripts/configs`
source_dir=`readlink -f ../../splitfs/splitfs`
filebench_dir=`readlink -f ../../splitfs/filebench`

pmem_dir=/mnt/pmem_emul

PRECMD="$source_dir/run_boost.sh -p $source_dir -t nvp_nvp.tree"

args="-s 30 --duty-cycle 0.1"
sudo rm -rf /mnt/pmem_emul/*

date

cd $source_dir
export LEDGER_DATAJ=0
export LEDGER_POSIX=1
export LEDGER_FILEBENCH=1 

make clean
make -e
sudo $setup_dir/dax_config.sh

cd $current_dir

sudo pmemtrace $args splitfs-varmail $PRECMD $filebench_dir/filebench -f varmailsmall.f

#run_filebench splitfs-posix copyfiles-small.f
