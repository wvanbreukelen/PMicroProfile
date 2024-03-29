/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _LINUX_MMIOTRACE_H
#define _LINUX_MMIOTRACE_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/atomic.h>
#include <linux/sched.h>

struct kmmio_probe;
struct pt_regs;

typedef void (*kmmio_pre_handler_t)(struct kmmio_probe *,
				struct pt_regs *, unsigned long addr, unsigned long hw_error_code);
typedef void (*kmmio_post_handler_t)(struct kmmio_probe *,
				unsigned long condition, struct pt_regs *);

struct kmmio_probe {
	/* kmmio internal list: */
	struct list_head	list;
	/* start location of the probe point: */
	unsigned long		addr;
	/* length of the probe region: */
	unsigned long		len;
	/* is user space probe */
	int 		user_task_pid;
	/* Called before addr is executed: */
	kmmio_pre_handler_t	pre_handler;
	/* Called after addr is executed: */
	kmmio_post_handler_t	post_handler;
	void			*private;
};

extern unsigned int kmmio_count;

extern int register_kmmio_probe(struct kmmio_probe *p);
extern int unregister_kmmio_probe(struct kmmio_probe *p, int dirty);
extern int kmmio_init(void);
extern void kmmio_cleanup(void);



#define KMMIO_HERTZ 50

#ifdef CONFIG_MMIOTRACE

extern atomic_t kmmio_miss_counter;


/* Called from page fault handler. */
extern int kmmio_handler(struct pt_regs *regs, unsigned long addr, unsigned long hw_error_code);

unsigned long get_kmmio_stepping_time(void);
void reset_kmmio_stepping_time(void);

/* Called from ioremap.c */
extern void mmiotrace_ioremap(resource_size_t offset, unsigned long size,
							void __iomem *addr, struct task_struct* user_task, unsigned int defer);
extern void mmiotrace_iounmap(volatile void __iomem *addr, volatile void __iomem *size, struct task_struct *task);

extern void mmiotrace_disarm_trace_probe(volatile void __iomem *addr);
extern void mmiotrace_attach_user_probes(void);
extern void mmiotrace_detach_user_probes(void);
extern void mmiotrace_task_exit(struct task_struct *task);

extern void enable_mmiotrace_soft(void);
extern void disable_mmiotrace_soft(void);

extern bool mmiotrace_is_enabled(void);
// extern bool mmiotrace_rrobes_enabled(void);
extern bool mmiotrace_probes_enabled(void);

/* For anyone to insert markers. Remember trailing newline. */
extern __printf(1, 2) int mmiotrace_printk(const char *fmt, ...);
#else /* !CONFIG_MMIOTRACE: */
static inline int is_kmmio_active(void)
{
	return 0;
}

static inline int kmmio_handler(struct pt_regs *regs, unsigned long addr)
{
	return 0;
}

static inline void mmiotrace_ioremap(resource_size_t offset,
					unsigned long size, void __iomem *addr, unsigned int defer)
{
}

static inline void mmiotrace_iounmap(volatile void __iomem *addr, volatile void __iomem *size, struct task_struct *task)
{
}

static inline __printf(1, 2) int mmiotrace_printk(const char *fmt, ...)
{
	return 0;
}
#endif /* CONFIG_MMIOTRACE */

enum mm_io_opcode {
	MMIO_READ	= 0x1,	/* struct mmiotrace_rw */
	MMIO_WRITE	= 0x2,	/* struct mmiotrace_rw */
	MMIO_PROBE	= 0x3,	/* struct mmiotrace_map */
	MMIO_UNPROBE	= 0x4,	/* struct mmiotrace_map */
	MMIO_UNKNOWN_OP = 0x5,	/* struct mmiotrace_rw */
	MMIO_CLFLUSH = 0x6,
	MMIO_MFENCE = 0x7,
	MMIO_SFENCE = 0x8,
	MMIO_LFENCE = 0x9
};

struct mmiotrace_rw {
	resource_size_t	phys;	/* PCI address of register */
	unsigned long long	value;
	unsigned long	pc;	/* optional program counter */
	int		map_id;
	unsigned int 	opcode_cpu;
	unsigned char	opcode;	/* one of MMIO_{READ,WRITE,UNKNOWN_OP} */
	unsigned char	width;	/* size of register access in bytes */
	unsigned int 	syscall_nr;
};

struct mmiotrace_map {
	resource_size_t	phys;	/* base address in PCI space */
	unsigned long	virt;	/* base virtual address */
	unsigned long	len;	/* mapping size */
	int		map_id;
	unsigned char	opcode;	/* MMIO_PROBE or MMIO_UNPROBE */
};

/* in kernel/trace/trace_mmiotrace.c */
extern void enable_mmiotrace(void);
extern void disable_mmiotrace(void);

extern int enable_pmemtrace_sampler(unsigned int freq, unsigned int duty_cycle, unsigned int is_time_triggered);
extern int enable_pmemtrace_sampler_default_settings(void);
extern int disable_pmemtrace_sampler(void);
extern int set_pmemtrace_multicore(unsigned int is_on);

extern void mmio_trace_rw(struct mmiotrace_rw *rw);
extern void mmio_trace_mapping(struct mmiotrace_map *map);
extern __printf(1, 0) int mmio_trace_printk(const char *fmt, va_list args);

extern atomic_t faults_counter;
extern atomic_t faults_captured;

#ifdef CONFIG_MMIOTRACE
static inline int is_kmmio_active(void)
{
	atomic_inc(&faults_counter);

	return kmmio_count;
}

#endif

#endif /* _LINUX_MMIOTRACE_H */
