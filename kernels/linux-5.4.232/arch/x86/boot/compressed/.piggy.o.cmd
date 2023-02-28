cmd_arch/x86/boot/compressed/piggy.o := gcc -Wp,-MD,arch/x86/boot/compressed/.piggy.o.d -nostdinc -isystem /usr/lib/gcc/x86_64-linux-gnu/11/include -I./arch/x86/include -I./arch/x86/include/generated  -I./include -I./arch/x86/include/uapi -I./arch/x86/include/generated/uapi -I./include/uapi -I./include/generated/uapi -include ./include/linux/kconfig.h -D__KERNEL__ -m64 -O2 -fno-strict-aliasing -fPIE -DDISABLE_BRANCH_PROFILING -mcmodel=small -mno-mmx -mno-sse -ffreestanding -fno-stack-protector -Wno-address-of-packed-member -Wno-pointer-sign -Wa,-mrelax-relocations=no -D__ASSEMBLY__    -c -o arch/x86/boot/compressed/piggy.o arch/x86/boot/compressed/piggy.S

source_arch/x86/boot/compressed/piggy.o := arch/x86/boot/compressed/piggy.S

deps_arch/x86/boot/compressed/piggy.o := \
  include/linux/kconfig.h \
    $(wildcard include/config/cpu/big/endian.h) \
    $(wildcard include/config/booger.h) \
    $(wildcard include/config/foo.h) \

arch/x86/boot/compressed/piggy.o: $(deps_arch/x86/boot/compressed/piggy.o)

$(deps_arch/x86/boot/compressed/piggy.o):
