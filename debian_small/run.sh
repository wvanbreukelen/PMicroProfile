qemu-system-x86_64 \
	-cpu host \
	-smp cores=4 \
	-kernel $1/arch/x86/boot/bzImage \
	-append "console=ttyS0 root=/dev/sda earlyprintk=serial net.ifnames=0 nokaslr memmap=4G!4G" \
	-drive file=image/bullseye.img,format=raw \
	-machine pc,accel=kvm,nvdimm=on \
	-m 8G,slots=2,maxmem=16G \
	-net user,host=10.0.2.10,hostfwd=tcp:127.0.0.1:2222-:22 \
	-net nic,model=e1000 \
	-object memory-backend-file,id=mem1,share,mem-path=f27nvdimm0,size=4G \
	-device nvdimm,memdev=mem1,id=nv1,label-size=2M \
	-object memory-backend-file,id=mem2,share,mem-path=f27nvdimm1,size=4G \
	-device nvdimm,memdev=mem2,id=nv2,label-size=2M \
	-enable-kvm \
	-nographic \
	-pidfile vm.pid
  	# -device nvdimm,memdev=mem1,id=nv1,label-size=2M \
	# -initrd ramdisk.img \

