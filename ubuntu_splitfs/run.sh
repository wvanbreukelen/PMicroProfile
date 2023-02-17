qemu-system-x86_64 \
	-cpu Cascadelake-Server\
	-smp cores=4 \
	-drive file=ubuntu.img.qcow2,format=qcow2 \
    -cdrom "ubuntu-18.04.6-desktop-amd64.iso" \
	-machine pc,accel=kvm,nvdimm=on,nvdimm-persistence=cpu \
	-m 16G,slots=2,maxmem=32G \
	-object memory-backend-file,id=mem1,mem-path=f27nvdimm0,share=off,size=16G,align=2M \
	-device nvdimm,memdev=mem1,id=nv1,label-size=256K \
	-net user,host=10.0.2.10,hostfwd=tcp:127.0.0.1:2222-:22 \
	-net nic,model=e1000 \
    -vga virtio \
	-pidfile vm.pid
	# -object memory-backend-file,id=mem2,share=on,mem-path=f27nvdimm1,size=4G,pmem=on \
	# -device nvdimm,memdev=mem2,id=nv2,label-size=2M \
	# -nographic \
	# -enable-kvm \
  	# -device nvdimm,memdev=mem1,id=nv1,label-size=2M \
	# -initrd ramdisk.img \

