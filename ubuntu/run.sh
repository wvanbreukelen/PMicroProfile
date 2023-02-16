qemu-system-x86_64 \
	-cpu host \
	-smp cores=4 \
	-drive file=ubuntu.img.qcow2,format=qcow2 \
	-kernel $1/arch/x86/boot/bzImage \
	-append "console=ttyS0 root=/dev/sda5 earlyprintk=serial net.ifnames=0 nokaslr memmap=4G!4G" \
	-machine pc,accel=kvm,nvdimm=on \
	-m 8G,slots=2,maxmem=16G \
	-net user,host=10.0.2.10,hostfwd=tcp:127.0.0.1:2222-:22 \
	-net nic,model=e1000 \
	-object memory-backend-file,id=mem1,share,mem-path=f27nvdimm0,size=4G \
	-device nvdimm,memdev=mem1,id=nv1,label-size=2M \
	-object memory-backend-file,id=mem2,share,mem-path=f27nvdimm1,size=4G \
	-device nvdimm,memdev=mem2,id=nv2,label-size=2M \
    -vga virtio \
	-pidfile vm.pid
    -cdrom "ubuntu-20.04.5-desktop-amd64.iso" \
	# -object memory-backend-file,id=mem2,share=on,mem-path=f27nvdimm1,size=4G,pmem=on \
	# -device nvdimm,memdev=mem2,id=nv2,label-size=2M \
	# -nographic \
	# -enable-kvm \
  	# -device nvdimm,memdev=mem1,id=nv1,label-size=2M \
	# -initrd ramdisk.img \

