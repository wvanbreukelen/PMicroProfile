set -o xtrace

current_dir=$(pwd)
source_dir=`readlink -f ../../splitfs/splitfs`
setup_dir=`readlink -f ../../splitfs/scripts/configs`

PRECMD="../../splitfs/splitfs/run_boost.sh -p ../splitfs/splitfs -t nvp_nvp.tree"

args="-s 30 --duty-cycle 0.6"
sudo rm -rf /mnt/pmem_emul/*

date

cd $source_dir
export LEDGER_DATAJ=0
export LEDGER_POSIX=1
export LEDGER_FIO=1 
#export LEDGER_DEBUG=0

make clean
make -e
sudo $setup_dir/dax_config.sh

cd $current_dir

#sudo env "PATH=$PATH" $PRECMD fio --name=split-fsfio-randrw-0 --directory=/mnt/pmem_emul --size=2Gb --rw=randrw --bs=4K --rwmixread=0 --rwmixwrite=100 --ioengine=sync

$PRECMD fio --name=fio-randrw-50 --directory=/mnt/pmem_emul --size=100M --rw=randrw --bs=4K  --rwmixread=100 --rwmixwrite=0  --ioengine=sync
#sudo pmemtrace $args splitfs-fio-randrw-50 $PRECMD fio --name=fio-randrw-50 --directory=/mnt/pmem_emul --size=100M --rw=randrw --bs=4K  --rwmixread=100 --rwmixwrite=0  --ioengine=sync
#sudo env "PATH=$PATH" $PRECMD fio --name=splitfs-fio-randrw-0 --directory=/mnt/pmem_emul --size=1Gb --rw=randrw --bs=4K  --rwmixread=50 --rwmixwrite=50  --ioengine=sync
#sudo env "PATH=$PATH" pmemtrace /dev/ndctl0 splitfs-fio-randrw-25 fio --name=fio-randrw-25 --directory=/mnt/pmem_emul --size=2Gb --rw=randrw --bs=4K  --rwmixread=25 --rwmixwrite=75  --ioengine=sync
#sudo env "PATH=$PATH" pmemtrace /dev/ndctl0 splitfs-fio-randrw-50 fio --name=fio-randrw-50 --directory=/mnt/pmem_emul --size=1Gb --rw=randrw --bs=4K  --rwmixread=50 --rwmixwrite=50  --ioengine=sync
#sudo env "PATH=$PATH" pmemtrace /dev/ndctl0 splitfs-fio-randrw-75 fio --name=fio-randrw-75 --directory=/mnt/pmem_emul --size=2Gb --rw=randrw --bs=4K  --rwmixread=75 --rwmixwrite=25  --ioengine=sync
#sudo env "PATH=$PATH" pmemtrace /dev/ndctl0 splitfs-fio-randrw-100 fio --name=fio-randrw-100 --directory=/mnt/pmem_emul --size=2Gb --rw=randrw --bs=4K  --rwmixread=100 --rwmixwrite=0  --ioengine=sync


