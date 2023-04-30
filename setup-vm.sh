#!/bin/bash

# Define output colors and unicode symbols
GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'
CHECKMARK='\u2713'
XMARK='\u2717'

DO_CLEANUP_BEFORE_BUILD=false

ISO_NAME="ubuntu.iso"
CUR_PATH=$(readlink -f .)

log_cmd() {
    output=""
    printf "Running command: %s\n" "$1"
    if [ "$4" = true ]; then
        output=$({ eval "$1" ; } 2>&1)
    else
        eval "$1" ;
    fi

    ret=$?

    if [ $ret -eq 0 ]; then
        printf "${GREEN}${CHECKMARK} %s${NC}\n" "$2"
    else
        printf "${RED}${XMARK} %s${NC}\n" "$3"
        if [ "$4" = true ]; then
            printf "\n%s\n" "$output"
        fi
        cd $CUR_PATH
        exit 1
    fi
}

ISO_URL="https://releases.ubuntu.com/20.04/ubuntu-20.04.6-desktop-amd64.iso"

if ! test -f "$CUR_PATH/vm/$ISO_NAME"; then
log_cmd "wget --progress=bar:force --no-clobber --output-document=$CUR_PATH/vm/$ISO_NAME $ISO_URL" \
    "Ubuntu iso download successful." \
    "Ubuntu iso download failed." false
fi

# Install dependencies using apt.
log_cmd "sudo apt-get update && sudo apt-get install -y build-essential gcc make libncurses-dev bison flex libssl-dev libelf-dev qemu-kvm qemu-system-x86 virt-manager virtinst libvirt-clients libvirt-daemon-system" \
    "Installed package dependencies." \
    "Dependencies installation failed." true

# Setup custom kernel.
cd $CUR_PATH/kernels/linux-5.4.232/ 

if [ "$DO_CLEANUP_BEFORE_BUILD" = true ]; then
log_cmd "make mrproper && cp $CUR_PATH/vm/.config.backup .config && make olddefconfig" \
    "Configured kernel." \
    "Failed to configure kernel." true
fi

log_cmd "nice make -j $(nproc)" \
    "Built kernel." \
    "Failed to built kernel." true

# Setup QEMU
cd $CUR_PATH
log_cmd "qemu-img create -f qcow2 $CUR_PATH/vm/disk.qcow 30G" \
    "Created QEMU disk image." \
    "Failed to create QEMU disk image." true


printf "${GREEN}${CHECKMARK} Setup Finished! Execute './run_kvm_iso.sh' command to launch Ubuntu Installer.${NC}\n"
printf "After completing the Ubuntu installation, perform the following actions:\n"
printf "1. Open a terminal inside the VM and execute the following command: ''\n"
printf "2. Copy the mount path of the root file system (/) and update the kernel boot parameters in the $CUR_PATH/vm/run_kvm.sh file accordingly\n"
printf "3. Now, boot the VM using the custom kernel by running './vm/run_kvm.sh' \n"
