	.file	"bounds.c"
# GNU C89 (Ubuntu 11.3.0-1ubuntu1~22.04) version 11.3.0 (x86_64-linux-gnu)
#	compiled by GNU C version 11.3.0, GMP version 6.2.1, MPFR version 4.1.0, MPC version 1.2.1, isl version isl-0.24-GMP

# GGC heuristics: --param ggc-min-expand=100 --param ggc-min-heapsize=131072
# options passed: -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -mno-avx -m64 -mno-80387 -mno-fp-ret-in-387 -mpreferred-stack-boundary=3 -mskip-rax-setup -mtune=generic -mno-red-zone -mcmodel=kernel -mindirect-branch=thunk-extern -mindirect-branch-register -mrecord-mcount -mfentry -march=x86-64 -O2 -std=gnu90 -p -fno-strict-aliasing -fno-common -fshort-wchar -fno-PIE -fcf-protection=none -falign-jumps=1 -falign-loops=1 -fno-asynchronous-unwind-tables -fno-jump-tables -fno-delete-null-pointer-checks -fno-allow-store-data-races -fstack-protector-strong -fno-strict-overflow -fno-merge-all-constants -fmerge-constants -fstack-check=no -fconserve-stack -fstack-protector-strong -fstack-clash-protection
	.text
	.section	.text.startup,"ax",@progbits
	.p2align 4
	.globl	main
	.type	main, @function
main:
1:	call	__fentry__
	.section __mcount_loc, "a",@progbits
	.quad 1b
	.previous
# kernel/bounds.c:19: 	DEFINE(NR_PAGEFLAGS, __NR_PAGEFLAGS);
#APP
# 19 "kernel/bounds.c" 1
	
.ascii "->NR_PAGEFLAGS $23 __NR_PAGEFLAGS"	#
# 0 "" 2
# kernel/bounds.c:20: 	DEFINE(MAX_NR_ZONES, __MAX_NR_ZONES);
# 20 "kernel/bounds.c" 1
	
.ascii "->MAX_NR_ZONES $5 __MAX_NR_ZONES"	#
# 0 "" 2
# kernel/bounds.c:22: 	DEFINE(NR_CPUS_BITS, ilog2(CONFIG_NR_CPUS));
# 22 "kernel/bounds.c" 1
	
.ascii "->NR_CPUS_BITS $6 ilog2(CONFIG_NR_CPUS)"	#
# 0 "" 2
# kernel/bounds.c:24: 	DEFINE(SPINLOCK_SIZE, sizeof(spinlock_t));
# 24 "kernel/bounds.c" 1
	
.ascii "->SPINLOCK_SIZE $4 sizeof(spinlock_t)"	#
# 0 "" 2
# kernel/bounds.c:28: }
#NO_APP
	xorl	%eax, %eax	#
	ret	
	.size	main, .-main
	.ident	"GCC: (Ubuntu 11.3.0-1ubuntu1~22.04) 11.3.0"
	.section	.note.GNU-stack,"",@progbits
