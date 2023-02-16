sudo qemu-system-x86_64 -drive file=fedora27.raw,format=raw,index=0,media=disk \
  -m 4G,slots=4,maxmem=16G \
  -smp 4 \
  -cpu host \
  -machine pc,accel=kvm,nvdimm=on \
  -enable-kvm \
  -vnc :0 \
  -net nic \
  -net user,hostfwd=tcp::2222-:22 \
  -object memory-backend-file,id=mem1,share=on,mem-path=./f27nvdimm0,size=4G \
  -device nvdimm,memdev=mem1,id=nv1,label-size=2M \
  -object memory-backend-file,id=mem2,share=on,mem-path=./f27nvdimm1,size=4G \
  -device nvdimm,memdev=mem2,id=nv2,label-size=2M

