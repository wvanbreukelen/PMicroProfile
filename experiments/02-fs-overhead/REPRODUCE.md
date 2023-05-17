# Evaluating file system overhead

## Prerequisites
- A machine with real persistent memory;
- Linux 5.* kernel. You will need to compile custom kernel builds in order to run the NOVA file system. Our own kernel build (see `../../kernels/linux-5.4.232`) already includes support for SplitFS. Instructions how to build a kernel with NOVA support can be found at this URL: https://github.com/NVSL/linux-nova. The UFS source is not public at this time, hence these results cannot be reproduced at this time.
- perf;
- fio benchmark;

## Steps
1. First boot our custom kernel by executing `../../vm/run_kvm.sh`. Within the VM, made sure you install are required packages by running the `../../install.sh` (see Thesis Artifact for further instructions);
2. Within the VM, make sure you mount the file system (e.g., Ext4-DAX, NOVA, SplitFS) at the `\mnt\pmem_emul` mount path, and execute the following command:
    `perf record -a -g --call-graph dwarf fio --name=fio-randrw-50 --directory=/mnt/pmem_emul --size=1G --rw=randrw --bs=4K  --rwmixread=50 --rwmixwrite=50  --ioengine=sync`
3. Now, open the perf report file by running `perf report --sort=overhead,sym`.
4. Populate the `data.csv` file by manaully accumulating the different forms of overhead.
5. Repeat these steps for all the file systems.
6. Run the `plot.py` script to create the figure.

