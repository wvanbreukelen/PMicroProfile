set -o xtrace

# We need to turn off VA randomization for filebench -> https://github.com/filebench/filebench/issues/110 
# ;0

echo 0 | sudo tee /proc/sys/kernel/randomize_va_space

workload=filebench
workload="varmailsmall.f"
current_dir=$(pwd)
filebench_dir=`readlink -f ../../splitfs/filebench`
workload_dir=$filebench_dir/workloads

sudo rm -rf /pmt/pmem_emul/*

date
args="-s 30 --duty-cycle 0.95" # old --duty-cycle 0.95
# sudo time ./pmemtrace -s 15 --duty-cycle 4.0 randread sudo bash -c "head -c 1000M </dev/urandom >/mnt/pmem_emul/some_rand5.txt

sudo env "PATH=$PATH" pmemtrace $args ext4-dax-varmail sudo $filebench_dir/filebench -f varmailsmall.f 2>&1
