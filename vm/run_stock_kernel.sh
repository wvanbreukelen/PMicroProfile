qemu-system-x86_64 \
	-cpu host\
	-enable-kvm \
	-smp cores=8 \
	-drive file=disk.qcow2,format=qcow2 \
	-machine pc,nvdimm=on \
	-m 8G,slots=2,maxmem=36G \
    -object memory-backend-file,id=mem1,mem-path=nvdimm0,share=off,pmem=on,size=28G,align=2M \
	-device nvdimm,memdev=mem1,id=nv1,label-size=256K \
	-net user,host=10.0.2.10,hostfwd=tcp:127.0.0.1:2222-:22 \
	-net nic,model=e1000 \
    -vga virtio \
	-pidfile vm.pid \
    # -cdrom "ubuntu.iso"

 