cmd_arch/x86/entry/entry_64.o := gcc -Wp,-MD,arch/x86/entry/.entry_64.o.d -nostdinc -isystem /usr/lib/gcc/x86_64-linux-gnu/11/include -I./arch/x86/include -I./arch/x86/include/generated  -I./include -I./arch/x86/include/uapi -I./arch/x86/include/generated/uapi -I./include/uapi -I./include/generated/uapi -include ./include/linux/kconfig.h -D__KERNEL__ -D__ASSEMBLY__ -fno-PIE -m64 -DCONFIG_AS_CFI=1 -DCONFIG_AS_CFI_SIGNAL_FRAME=1 -DCONFIG_AS_CFI_SECTIONS=1 -DCONFIG_AS_SSSE3=1 -DCONFIG_AS_AVX=1 -DCONFIG_AS_AVX2=1 -DCONFIG_AS_AVX512=1 -DCONFIG_AS_SHA1_NI=1 -DCONFIG_AS_SHA256_NI=1 -Wa,-gdwarf-2 -DCC_USING_FENTRY    -c -o arch/x86/entry/entry_64.o arch/x86/entry/entry_64.S

source_arch/x86/entry/entry_64.o := arch/x86/entry/entry_64.S

deps_arch/x86/entry/entry_64.o := \
    $(wildcard include/config/paravirt.h) \
    $(wildcard include/config/trace/irqflags.h) \
    $(wildcard include/config/dynamic/ftrace.h) \
    $(wildcard include/config/x86/5level.h) \
    $(wildcard include/config/stackprotector.h) \
    $(wildcard include/config/debug/entry.h) \
    $(wildcard include/config/x86/l1/cache/shift.h) \
    $(wildcard include/config/preemption.h) \
    $(wildcard include/config/x86/espfix64.h) \
    $(wildcard include/config/smp.h) \
    $(wildcard include/config/x86/uv.h) \
    $(wildcard include/config/have/kvm.h) \
    $(wildcard include/config/x86/mce/threshold.h) \
    $(wildcard include/config/x86/mce/amd.h) \
    $(wildcard include/config/x86/thermal/vector.h) \
    $(wildcard include/config/irq/work.h) \
    $(wildcard include/config/xen/pv.h) \
    $(wildcard include/config/xen/pvhvm.h) \
    $(wildcard include/config/hyperv.h) \
    $(wildcard include/config/acrn/guest.h) \
    $(wildcard include/config/kvm/guest.h) \
    $(wildcard include/config/x86/mce.h) \
    $(wildcard include/config/ia32/emulation.h) \
  include/linux/kconfig.h \
    $(wildcard include/config/cpu/big/endian.h) \
    $(wildcard include/config/booger.h) \
    $(wildcard include/config/foo.h) \
  include/linux/linkage.h \
  include/linux/compiler_types.h \
    $(wildcard include/config/have/arch/compiler/h.h) \
    $(wildcard include/config/enable/must/check.h) \
    $(wildcard include/config/optimize/inlining.h) \
    $(wildcard include/config/cc/has/asm/inline.h) \
  include/linux/stringify.h \
  include/linux/export.h \
    $(wildcard include/config/modversions.h) \
    $(wildcard include/config/module/rel/crcs.h) \
    $(wildcard include/config/have/arch/prel32/relocations.h) \
    $(wildcard include/config/modules.h) \
    $(wildcard include/config/trim/unused/ksyms.h) \
    $(wildcard include/config/unused/symbols.h) \
  arch/x86/include/asm/linkage.h \
    $(wildcard include/config/x86/32.h) \
    $(wildcard include/config/x86/64.h) \
    $(wildcard include/config/x86/alignment/16.h) \
  arch/x86/include/asm/segment.h \
    $(wildcard include/config/paravirt/xxl.h) \
    $(wildcard include/config/x86/32/lazy/gs.h) \
  include/linux/const.h \
  include/uapi/linux/const.h \
  arch/x86/include/asm/alternative.h \
  arch/x86/include/asm/cache.h \
    $(wildcard include/config/x86/internode/cache/shift.h) \
    $(wildcard include/config/x86/vsmp.h) \
  arch/x86/include/generated/uapi/asm/errno.h \
  include/uapi/asm-generic/errno.h \
  include/uapi/asm-generic/errno-base.h \
  arch/x86/include/asm/asm-offsets.h \
  include/generated/asm-offsets.h \
  arch/x86/include/asm/msr.h \
    $(wildcard include/config/tracepoints.h) \
  arch/x86/include/asm/msr-index.h \
  include/linux/bits.h \
  arch/x86/include/uapi/asm/bitsperlong.h \
  include/asm-generic/bitsperlong.h \
    $(wildcard include/config/64bit.h) \
  include/uapi/asm-generic/bitsperlong.h \
  arch/x86/include/asm/unistd.h \
  arch/x86/include/uapi/asm/unistd.h \
  arch/x86/include/generated/uapi/asm/unistd_64.h \
  arch/x86/include/generated/asm/unistd_64_x32.h \
  arch/x86/include/asm/thread_info.h \
    $(wildcard include/config/vm86.h) \
    $(wildcard include/config/frame/pointer.h) \
    $(wildcard include/config/compat.h) \
  include/linux/compiler.h \
    $(wildcard include/config/trace/branch/profiling.h) \
    $(wildcard include/config/profile/all/branches.h) \
    $(wildcard include/config/stack/validation.h) \
    $(wildcard include/config/kasan.h) \
  arch/x86/include/asm/page.h \
  include/linux/types.h \
    $(wildcard include/config/have/uid16.h) \
    $(wildcard include/config/uid16.h) \
    $(wildcard include/config/arch/dma/addr/t/64bit.h) \
    $(wildcard include/config/phys/addr/t/64bit.h) \
  include/uapi/linux/types.h \
  arch/x86/include/generated/uapi/asm/types.h \
  include/uapi/asm-generic/types.h \
  include/asm-generic/int-ll64.h \
  include/uapi/asm-generic/int-ll64.h \
  arch/x86/include/asm/page_types.h \
    $(wildcard include/config/physical/start.h) \
    $(wildcard include/config/physical/align.h) \
    $(wildcard include/config/dynamic/physical/mask.h) \
  include/linux/mem_encrypt.h \
    $(wildcard include/config/arch/has/mem/encrypt.h) \
    $(wildcard include/config/amd/mem/encrypt.h) \
  arch/x86/include/asm/page_64_types.h \
    $(wildcard include/config/dynamic/memory/layout.h) \
    $(wildcard include/config/randomize/base.h) \
  arch/x86/include/asm/page_64.h \
    $(wildcard include/config/debug/virtual.h) \
    $(wildcard include/config/flatmem.h) \
    $(wildcard include/config/x86/vsyscall/emulation.h) \
  include/asm-generic/memory_model.h \
    $(wildcard include/config/discontigmem.h) \
    $(wildcard include/config/sparsemem/vmemmap.h) \
    $(wildcard include/config/sparsemem.h) \
  include/linux/pfn.h \
  include/asm-generic/getorder.h \
  arch/x86/include/asm/percpu.h \
    $(wildcard include/config/x86/64/smp.h) \
    $(wildcard include/config/x86/cmpxchg64.h) \
  arch/x86/include/asm/hw_irq.h \
    $(wildcard include/config/x86/local/apic.h) \
    $(wildcard include/config/hpet/timer.h) \
    $(wildcard include/config/pci/msi.h) \
    $(wildcard include/config/x86/io/apic.h) \
    $(wildcard include/config/dmar/table.h) \
    $(wildcard include/config/vmd.h) \
    $(wildcard include/config/tracing.h) \
  arch/x86/include/asm/irq_vectors.h \
  include/linux/threads.h \
    $(wildcard include/config/nr/cpus.h) \
    $(wildcard include/config/base/small.h) \
  arch/x86/include/asm/irqflags.h \
    $(wildcard include/config/debug/lock/alloc.h) \
  arch/x86/include/asm/processor-flags.h \
    $(wildcard include/config/page/table/isolation.h) \
  arch/x86/include/uapi/asm/processor-flags.h \
  arch/x86/include/asm/paravirt.h \
    $(wildcard include/config/pgtable/levels.h) \
    $(wildcard include/config/x86/pae.h) \
    $(wildcard include/config/paravirt/spinlocks.h) \
  arch/x86/include/asm/pgtable_types.h \
    $(wildcard include/config/x86/intel/memory/protection/keys.h) \
    $(wildcard include/config/mem/soft/dirty.h) \
    $(wildcard include/config/proc/fs.h) \
  arch/x86/include/asm/pgtable_64_types.h \
  arch/x86/include/asm/sparsemem.h \
  arch/x86/include/asm/asm.h \
  arch/x86/include/asm/nospec-branch.h \
    $(wildcard include/config/retpoline.h) \
  include/linux/static_key.h \
  include/linux/jump_label.h \
    $(wildcard include/config/jump/label.h) \
    $(wildcard include/config/have/arch/jump/label/relative.h) \
  arch/x86/include/asm/jump_label.h \
  arch/x86/include/asm/nops.h \
    $(wildcard include/config/mk7.h) \
    $(wildcard include/config/x86/p6/nop.h) \
  include/linux/frame.h \
  arch/x86/include/asm/alternative-asm.h \
  arch/x86/include/asm/cpufeatures.h \
  arch/x86/include/asm/required-features.h \
    $(wildcard include/config/x86/minimum/cpu/family.h) \
    $(wildcard include/config/math/emulation.h) \
    $(wildcard include/config/x86/cmov.h) \
    $(wildcard include/config/x86/use/3dnow.h) \
    $(wildcard include/config/matom.h) \
  arch/x86/include/asm/disabled-features.h \
    $(wildcard include/config/x86/intel/mpx.h) \
    $(wildcard include/config/x86/smap.h) \
    $(wildcard include/config/x86/intel/umip.h) \
  arch/x86/include/asm/msr-index.h \
  arch/x86/include/asm/unwind_hints.h \
  arch/x86/include/asm/orc_types.h \
  arch/x86/include/asm/paravirt_types.h \
    $(wildcard include/config/paravirt/debug.h) \
  arch/x86/include/asm/smap.h \
  arch/x86/include/generated/asm/export.h \
  include/asm-generic/export.h \
  arch/x86/include/asm/frame.h \
  include/linux/err.h \
  arch/x86/entry/calling.h \
    $(wildcard include/config/gcc/plugin/stackleak.h) \
    $(wildcard include/config/context/tracking.h) \

arch/x86/entry/entry_64.o: $(deps_arch/x86/entry/entry_64.o)

$(deps_arch/x86/entry/entry_64.o):
