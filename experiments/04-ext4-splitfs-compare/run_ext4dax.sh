set -o xtrace

sudo rm -rf /pmt/pmem_emul/*

date
args="-s 30 --duty-cycle 0.95"
# sudo time ./pmemtrace -s 15 --duty-cycle 4.0 randread sudo bash -c "head -c 1000M </dev/urandom >/mnt/pmem_emul/some_rand5.txt

sudo env "PATH=$PATH" pmemtrace $args fio-randrw-50-ext4-dax sudo fio --name=fio-randrw-0 --directory=/mnt/pmem_emul --size=500M --rw=randrw --bs=1K --direct=0 --rwmixread=50 --rwmixwrite=50 --numjobs=1 --ioengine=sync
#sudo env "PATH=$PATH" pmemtrace $args fio-randrw-25 fio --name=fio-randrw-25 --directory=/mnt/pmem_emul --size=2Gb --rw=randrw --bs=1K --direct=0 --rwmixread=25 --rwmixwrite=75 --numjobs=1 --ioengine=sync
#sudo env "PATH=$PATH" pmemtrace $args fio-randrw-50 fio --name=fio-randrw-50 --directory=/mnt/pmem_emul --size=2Gb --rw=randrw --bs=1K --direct=0 --rwmixread=50 --rwmixwrite=50 --numjobs=1 --ioengine=sync
#sudo env "PATH=$PATH" pmemtrace $args fio-randrw-75 fio --name=fio-randrw-75 --directory=/mnt/pmem_emul --size=2Gb --rw=randrw --bs=1K --direct=0 --rwmixread=75 --rwmixwrite=25 --numjobs=1 --ioengine=sync
#sudo env "PATH=$PATH" pmemtrace $args fio-randrw-100 fio --name=fio-randrw-100 --directory=/mnt/pmem_emul --size=2Gb --rw=randrw --bs=1K --direct=0 --rwmixread=100 --rwmixwrite=0 --numjobs=1 --ioengine=sync


