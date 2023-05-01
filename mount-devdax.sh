#!/bin/bash

sudo mkdir -p /mnt/pmem_emul/   

sudo umount /mnt/pmem_emul/
sudo ndctl destroy-namespace all --force
sudo ndctl create-namespace -m devdax
