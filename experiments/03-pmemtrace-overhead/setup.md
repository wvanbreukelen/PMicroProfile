Run the following commands:

sudo pmemtrace randread sudo bash -c "perf record -a -g --call-graph dwarf head -c 32M </dev/urandom >/mnt/pmem_emul/rand_file.txt" --disable-sampling

