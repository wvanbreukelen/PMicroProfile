#!/bin/bash

sudo mkdir -p /mnt/pmem_emul/   

sudo rm -rf /mnt/pmem_emul/*
sync
sleep 1

sudo umount /mnt/pmem_emul/

sudo ndctl destroy-namespace all --force
sudo ndctl create-namespace -m fsdax

sudo mkfs.ext4 -b 4096 /dev/pmem0
sudo mount -o dax /dev/pmem0 /mnt/pmem_emul

sudo chown $USER /mnt/pmem_emul/

rm -rf /mnt/pmem_emul/*
