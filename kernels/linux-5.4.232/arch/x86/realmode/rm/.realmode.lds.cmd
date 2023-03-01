cmd_arch/x86/realmode/rm/realmode.lds := gcc -E -Wp,-MD,arch/x86/realmode/rm/.realmode.lds.d -nostdinc -isystem /usr/lib/gcc/x86_64-linux-gnu/11/include -I./arch/x86/include -I./arch/x86/include/generated  -I./include -I./arch/x86/include/uapi -I./arch/x86/include/generated/uapi -I./include/uapi -I./include/generated/uapi -include ./include/linux/kconfig.h -D__KERNEL__  -P -C -I./arch/x86/realmode/rm -P -Ux86 -D__ASSEMBLY__ -DLINKER_SCRIPT -o arch/x86/realmode/rm/realmode.lds arch/x86/realmode/rm/realmode.lds.S

source_arch/x86/realmode/rm/realmode.lds := arch/x86/realmode/rm/realmode.lds.S

deps_arch/x86/realmode/rm/realmode.lds := \
  include/linux/kconfig.h \
    $(wildcard include/config/cpu/big/endian.h) \
    $(wildcard include/config/booger.h) \
    $(wildcard include/config/foo.h) \
  arch/x86/include/asm/page_types.h \
    $(wildcard include/config/physical/start.h) \
    $(wildcard include/config/physical/align.h) \
    $(wildcard include/config/x86/64.h) \
    $(wildcard include/config/dynamic/physical/mask.h) \
  include/linux/const.h \
  include/uapi/linux/const.h \
  include/linux/types.h \
    $(wildcard include/config/have/uid16.h) \
    $(wildcard include/config/uid16.h) \
    $(wildcard include/config/arch/dma/addr/t/64bit.h) \
    $(wildcard include/config/phys/addr/t/64bit.h) \
    $(wildcard include/config/64bit.h) \
  include/uapi/linux/types.h \
  arch/x86/include/generated/uapi/asm/types.h \
  include/uapi/asm-generic/types.h \
  include/asm-generic/int-ll64.h \
  include/uapi/asm-generic/int-ll64.h \
  arch/x86/include/uapi/asm/bitsperlong.h \
  include/asm-generic/bitsperlong.h \
  include/uapi/asm-generic/bitsperlong.h \
  include/linux/mem_encrypt.h \
    $(wildcard include/config/arch/has/mem/encrypt.h) \
    $(wildcard include/config/amd/mem/encrypt.h) \
  arch/x86/include/asm/page_64_types.h \
    $(wildcard include/config/kasan.h) \
    $(wildcard include/config/dynamic/memory/layout.h) \
    $(wildcard include/config/x86/5level.h) \
    $(wildcard include/config/randomize/base.h) \
  arch/x86/realmode/rm/pasyms.h \

arch/x86/realmode/rm/realmode.lds: $(deps_arch/x86/realmode/rm/realmode.lds)

$(deps_arch/x86/realmode/rm/realmode.lds):
