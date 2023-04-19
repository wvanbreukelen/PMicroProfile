// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *
 * Copyright (C) IBM Corporation, 2005
 *               Jeff Muizelaar, 2006, 2007
 *               Pekka Paalanen, 2008 <pq@iki.fi>
 *
 * Derived from the read-mod example from relay-examples by Tom Zanussi.
 */

#define pr_fmt(fmt) "mmiotrace: " fmt

#define DEBUG 1


#include <linux/moduleparam.h>
#include <linux/debugfs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <asm/pgtable.h>
#include <linux/mmiotrace.h>
#include <asm/e820/api.h> /* for ISA_START_ADDRESS */
#include <linux/atomic.h>
#include <linux/percpu.h>
#include <linux/cpu.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <asm/traps.h>			/* dotraplinkage, ...		*/

#include "pf_in.h"

struct trap_reason {
	unsigned long addr;
	unsigned long ip;
	enum reason_type type;
	int active_traces;
};

struct remap_trace {
	struct list_head list;
	struct kmmio_probe probe;
	resource_size_t phys;
	unsigned long id;
	unsigned int enabled;
};

struct sampling_thread_data {
	unsigned int freq;
	unsigned int duty_cycle;
	unsigned int is_time_triggered;
	//unsigned int period_off;
};

/* Accessed per-cpu. */
static DEFINE_PER_CPU(struct trap_reason, pf_reason);
static DEFINE_PER_CPU(struct mmiotrace_rw, cpu_trace);

static DEFINE_MUTEX(mmiotrace_mutex);
static DEFINE_SPINLOCK(trace_lock);
static atomic_t mmiotrace_enabled;
static LIST_HEAD(trace_list);		/* struct remap_trace */
static LIST_HEAD(trace_list_soft);

struct task_struct *kth = NULL;
static struct sampling_thread_data sampling_thread_da = {0};
static atomic_t is_pmemtrace_multicore = {0};
atomic_t fault_counter = {0};
/*
 * Locking in this file:
 * - mmiotrace_mutex enforces enable/disable_mmiotrace() critical sections.
 * - mmiotrace_enabled may be modified only when holding mmiotrace_mutex
 *   and trace_lock.
 * - Routines depending on is_enabled() must take trace_lock.
 * - trace_list users must hold trace_lock.
 * - is_enabled() guarantees that mmio_trace_{rw,mapping} are allowed.
 * - pre/post callbacks assume the effect of is_enabled() being true.
 */

/* module parameters */
static unsigned long	filter_offset;
static bool		nommiotrace;
static bool		trace_pc;
atomic_t kmmio_miss_counter;

module_param(filter_offset, ulong, 0);
module_param(nommiotrace, bool, 0);
module_param(trace_pc, bool, 0);

MODULE_PARM_DESC(filter_offset, "Start address of traced mappings.");
MODULE_PARM_DESC(nommiotrace, "Disable actual MMIO tracing.");
MODULE_PARM_DESC(trace_pc, "Record address of faulting instructions.");

static bool is_enabled(void)
{
	return atomic_read(&mmiotrace_enabled);
}

bool mmiotrace_is_enabled(void)
{
	return atomic_read(&mmiotrace_enabled);
}

static void print_pte(unsigned long address)
{
	unsigned int level;
	pte_t *pte = lookup_address(address, &level);

	if (!pte) {
		pr_err("Error in %s: no pte for page 0x%08lx\n",
		       __func__, address);
		return;
	}

	if (level == PG_LEVEL_2M) {
		pr_emerg("4MB pages are not currently supported: 0x%08lx\n",
			 address);
		BUG();
	}
	pr_info("pte for 0x%lx: 0x%llx 0x%llx\n",
		address,
		(unsigned long long)pte_val(*pte),
		(unsigned long long)pte_val(*pte) & _PAGE_PRESENT);
}

/*
 * For some reason the pre/post pairs have been called in an
 * unmatched order. Report and die.
 */
static void die_kmmio_nesting_error(struct pt_regs *regs, unsigned long addr)
{
	const struct trap_reason *my_reason = &get_cpu_var(pf_reason);
	pr_emerg("unexpected fault for address: 0x%08lx, last fault for address: 0x%08lx\n",
		 addr, my_reason->addr);
	print_pte(addr);
	pr_emerg("faulting IP is at %pS\n", (void *)regs->ip);
	pr_emerg("last faulting IP was at %pS\n", (void *)my_reason->ip);
#ifdef __i386__
	pr_emerg("eax: %08lx   ebx: %08lx   ecx: %08lx   edx: %08lx\n",
		 regs->ax, regs->bx, regs->cx, regs->dx);
	pr_emerg("esi: %08lx   edi: %08lx   ebp: %08lx   esp: %08lx\n",
		 regs->si, regs->di, regs->bp, regs->sp);
#else
	pr_emerg("rax: %016lx   rcx: %016lx   rdx: %016lx\n",
		 regs->ax, regs->cx, regs->dx);
	pr_emerg("rsi: %016lx   rdi: %016lx   rbp: %016lx   rsp: %016lx\n",
		 regs->si, regs->di, regs->bp, regs->sp);
#endif
	put_cpu_var(pf_reason);
	BUG();
}

static void pre(struct kmmio_probe *p, struct pt_regs *regs,
						unsigned long addr, unsigned long hw_error_code)
{
	struct trap_reason *my_reason = &get_cpu_var(pf_reason);
	struct mmiotrace_rw *my_trace = &get_cpu_var(cpu_trace);
	const unsigned long instptr = instruction_pointer(regs);
	//pr_info("pre faulting address: 0x%lx instptr: 0x%lx\n", addr, instptr);
	const enum reason_type type = get_ins_type(instptr);
	//const enum reason_type type = OTHERS;
	struct remap_trace *trace = p->private;
	unsigned char *ip;

	/* it doesn't make sense to have more than one active trace per cpu */
	if (my_reason->active_traces)
		die_kmmio_nesting_error(regs, addr);
	else
		my_reason->active_traces++;

	my_reason->type = type;
	my_reason->addr = addr;
	my_reason->ip = instptr;

	my_trace->phys = addr - trace->probe.addr + trace->phys;
	my_trace->map_id = trace->id;
	

	/*
	 * Only record the program counter when requested.
	 * It may taint clean-room reverse engineering.
	 */
	if (trace_pc)
		my_trace->pc = instptr;
	else
		my_trace->pc = 0;

	/*
	 * XXX: the timestamp recorded will be *after* the tracing has been
	 * done, not at the time we hit the instruction. SMP implications
	 * on event ordering?
	 */

	

	my_trace->opcode_cpu = get_ins_opcode(instptr);

	ip = (unsigned char *)instptr;

	if (type == REG_READ) {
		my_trace->opcode = MMIO_READ;
		my_trace->width = get_ins_mem_width(instptr);
	} else if (type == REG_WRITE) {
		my_trace->opcode = MMIO_WRITE;
		my_trace->width = get_ins_mem_width(instptr);
		my_trace->value = get_ins_reg_val(instptr, regs);
	} else if (type == INS_CACHE_OP) {
		my_trace->opcode = MMIO_CLFLUSH;
	} else { // (hw_error_code & X86_PF_WRITE) 
		//pr_info("Unknown instruction: %x\n", my_trace->opcode_cpu);
		pr_info("Unknown instruction\n");
	}
	// 	my_trace->opcode = MMIO_WRITE;
	// 	my_trace->width = get_ins_mem_width(instptr); // Don't know if the can fetch the width, probably not...
	// 	my_trace->value = (*ip) << 16 | *(ip + 1) << 8 |
	// 							*(ip + 2);
	// 	//my_trace->value = get_ins_imm_val(instptr);
	// } else {
	// 	my_trace->opcode = MMIO_READ;
	// 	my_trace->width = get_ins_mem_width(instptr); // Don't know if the can fetch the width, probably not...
	// }

	// switch (type) {
	// case REG_READ:
	// 	my_trace->opcode = MMIO_READ;
	// 	my_trace->width = get_ins_mem_width(instptr);
	// 	break;
	// case REG_WRITE:
	// 	my_trace->opcode = MMIO_WRITE;
	// 	my_trace->width = get_ins_mem_width(instptr);
	// 	my_trace->value = get_ins_reg_val(instptr, regs);
	// 	break;
	// case IMM_WRITE:
	// 	my_trace->opcode = MMIO_WRITE;
	// 	my_trace->width = get_ins_mem_width(instptr);
	// 	my_trace->value = get_ins_imm_val(instptr);
	// 	break;
	// default:
	// 	{
	// 		unsigned char *ip = (unsigned char *)instptr;
	// 		my_trace->opcode = MMIO_UNKNOWN_OP;
	// 		my_trace->width = 0;
	// 		my_trace->value = (*ip) << 16 | *(ip + 1) << 8 |
	// 							*(ip + 2);
	// 	}
	// }
	put_cpu_var(cpu_trace);
	put_cpu_var(pf_reason);
}

static void post(struct kmmio_probe *p, unsigned long condition,
							struct pt_regs *regs)
{
	struct trap_reason *my_reason = &get_cpu_var(pf_reason);
	struct mmiotrace_rw *my_trace = &get_cpu_var(cpu_trace);

	/* this should always return the active_trace count to 0 */
	my_reason->active_traces--;
	if (my_reason->active_traces) {
		pr_emerg("unexpected post handler");
		BUG();
	}

	switch (my_reason->type) {
	case REG_READ:
		my_trace->value = get_ins_reg_val(my_reason->ip, regs);
		break;
	default:
		break;
	}

	mmio_trace_rw(my_trace);
	put_cpu_var(cpu_trace);
	put_cpu_var(pf_reason);
}

static void ioremap_trace_core(resource_size_t offset, unsigned long size,
							void __iomem *addr, struct task_struct* _user_task)
{
	static atomic_t next_id;
	struct remap_trace *trace_in_list;
	struct remap_trace *trace = kmalloc(sizeof(*trace), GFP_KERNEL);
	/* These are page-unaligned. */
	struct mmiotrace_map map = {
		.phys = offset,
		.virt = (unsigned long)addr,
		.len = size,
		.opcode = MMIO_PROBE
	};

	if (!trace) {
		pr_err("kmalloc failed in ioremap\n");
		return;
	}

	*trace = (struct remap_trace) {
		.probe = {
			.addr = (unsigned long)addr,
			.len = size,
			.user_task = _user_task,
			.pre_handler = pre,
			.post_handler = post,
			.private = trace
		},
		.phys = offset,
		.id = atomic_inc_return(&next_id)
	};
	map.map_id = trace->id;

	spin_lock_irq(&trace_lock);
	if (!is_enabled()) {
		kfree(trace);
		goto not_enabled;
	}

	list_for_each_entry(trace_in_list, &trace_list, list) {
		if ((unsigned long)addr == trace_in_list->probe.addr) {
			pr_debug("Already mapped, skipping %p...\n", addr);
			goto not_enabled;
		}
	}

	mmio_trace_mapping(&map);
	list_add_tail(&trace->list, &trace_list);
	if (!nommiotrace) {
		int ret;
		if ((ret = register_kmmio_probe(&trace->probe)) < 0) {
			pr_warn("Unable to map probe at address 0x%lx!, errcode: %d\n", addr, ret);
		}
		trace->enabled = 1;
	}

not_enabled:
	spin_unlock_irq(&trace_lock);
}

void mmiotrace_ioremap(resource_size_t offset, unsigned long size,
						void __iomem *addr, struct task_struct* user_task)
{
	if (!is_enabled()) /* recheck and proper locking in *_core() */
		return;

	//pr_debug("ioremap_*(0x%llx, 0x%lx) = %p\n",
		 //(unsigned long long)offset, size, addr);
	if ((filter_offset) && (offset != filter_offset)) {
		printk("Filter_offset skip.\n");
		return;
	}
		
	ioremap_trace_core(offset, size, addr, user_task);
}

void mmiotrace_disarm_trace_probe(volatile void __iomem *addr)
{
	struct remap_trace *trace;
	struct remap_trace *tmp;
	struct remap_trace *found_trace = NULL;

	spin_lock_irq(&trace_lock);
	if (!is_enabled())
		goto not_enabled;

	list_for_each_entry_safe(trace, tmp, &trace_list, list) {
		if ((unsigned long)addr == trace->probe.addr) {
			if (!nommiotrace && trace->enabled) {
				//pr_debug("Unmapping %p.\n", addr);
				unregister_kmmio_probe(&trace->probe);
			}
			
			found_trace = trace;
			break;
		}
	}

not_enabled:
	spin_unlock_irq(&trace_lock);
	if (found_trace) {
		synchronize_rcu(); /* unregister_kmmio_probe() requirement */
		kfree(found_trace);
	}
}

static void iounmap_trace_core(volatile void __iomem *addr)
{
	struct mmiotrace_map map = {
		.phys = 0,
		.virt = (unsigned long)addr,
		.len = 0,
		.opcode = MMIO_UNPROBE
	};
	struct remap_trace *trace;
	struct remap_trace *tmp;
	struct remap_trace *found_trace = NULL;

	//pr_debug("Unmapping %p.\n", addr);

	spin_lock_irq(&trace_lock);
	if (!is_enabled())
		goto not_enabled;

	list_for_each_entry_safe(trace, tmp, &trace_list, list) {
		if ((unsigned long)addr == trace->probe.addr) {
			if (!nommiotrace && trace->enabled)
				unregister_kmmio_probe(&trace->probe);
			list_del(&trace->list);
			found_trace = trace;
			break;
			//pr_debug("Skipping %p, already mapped...\n", addr);
			//goto not_enabled;
		}
	}
	map.map_id = (found_trace) ? found_trace->id : -1;
	mmio_trace_mapping(&map);

not_enabled:
	spin_unlock_irq(&trace_lock);
	if (found_trace) {
		synchronize_rcu(); /* unregister_kmmio_probe() requirement */
		kfree(found_trace);
	}
}

void mmiotrace_iounmap(volatile void __iomem *addr)
{
	might_sleep();
	if (is_enabled()) /* recheck and proper locking in *_core() */
		iounmap_trace_core(addr);
}

int mmiotrace_printk(const char *fmt, ...)
{
	int ret = 0;
	va_list args;
	unsigned long flags;
	va_start(args, fmt);

	spin_lock_irqsave(&trace_lock, flags);
	if (is_enabled())
		ret = mmio_trace_printk(fmt, args);
	spin_unlock_irqrestore(&trace_lock, flags);

	va_end(args);
	return ret;
}
EXPORT_SYMBOL(mmiotrace_printk);

static void clear_trace_list(void)
{
	struct remap_trace *trace;
	struct remap_trace *tmp;

	/*
	 * No locking required, because the caller ensures we are in a
	 * critical section via mutex, and is_enabled() is false,
	 * i.e. nothing can traverse or modify this list.
	 * Caller also ensures is_enabled() cannot change.
	 */
	list_for_each_entry(trace, &trace_list, list) {
		pr_notice("purging non-iounmapped trace @0x%08lx, size 0x%lx.\n",
			  trace->probe.addr, trace->probe.len);
		if (!nommiotrace && trace->enabled) {
			unregister_kmmio_probe(&trace->probe);
			trace->enabled = 0;
		}
	}
	synchronize_rcu(); /* unregister_kmmio_probe() requirement */

	list_for_each_entry_safe(trace, tmp, &trace_list, list) {
		list_del(&trace->list);
		kfree(trace);
	}
}

#ifdef CONFIG_HOTPLUG_CPU
static cpumask_var_t downed_cpus;

static void enter_uniprocessor(void)
{
	int cpu;
	int err;

	if (!cpumask_available(downed_cpus) &&
	    !alloc_cpumask_var(&downed_cpus, GFP_KERNEL)) {
		pr_notice("Failed to allocate mask\n");
		goto out;
	}

	get_online_cpus();
	cpumask_copy(downed_cpus, cpu_online_mask);
	cpumask_clear_cpu(cpumask_first(cpu_online_mask), downed_cpus);
	if (num_online_cpus() > 1)
		pr_notice("Disabling non-boot CPUs...\n");
	put_online_cpus();

	for_each_cpu(cpu, downed_cpus) {
		err = cpu_down(cpu);
		if (!err)
			pr_info("CPU%d is down.\n", cpu);
		else
			pr_err("Error taking CPU%d down: %d\n", cpu, err);
	}
out:
	if (num_online_cpus() > 1)
		pr_warning("multiple CPUs still online, may miss events.\n");
}

static void leave_uniprocessor(void)
{
	int cpu;
	int err;

	if (!cpumask_available(downed_cpus) || cpumask_weight(downed_cpus) == 0)
		return;
	pr_notice("Re-enabling CPUs...\n");
	for_each_cpu(cpu, downed_cpus) {
		err = cpu_up(cpu);
		if (!err)
			pr_info("enabled CPU%d.\n", cpu);
		else
			pr_err("cannot re-enable CPU%d: %d\n", cpu, err);
	}
}

#else /* !CONFIG_HOTPLUG_CPU */
static void enter_uniprocessor(void)
{
	if (num_online_cpus() > 1)
		pr_warning("multiple CPUs are online, may miss events. "
			   "Suggest booting with maxcpus=1 kernel argument.\n");
}

static void leave_uniprocessor(void)
{
}
#endif

static void enable_mmiotrace_soft(void)
{
	struct remap_trace *trace;
	struct remap_trace *tmp;
	int ret;
	unsigned long flags;

	mutex_lock(&mmiotrace_mutex);
	spin_lock_irqsave(&trace_lock, flags);
	if (!is_enabled())
		goto out;

	if (nommiotrace)
		pr_info("MMIO tracing disabled.\n");
	//enter_uniprocessor();

	list_for_each_entry(trace, &trace_list, list) {
		if (!nommiotrace && !trace->enabled) {
			if ((ret = register_kmmio_probe(&trace->probe)) < 0) {
				pr_warn("Unable to map probe!\n");
			}
			trace->enabled = 1;
		}
	}

	//pr_info("enabled soft.\n");
out:
	spin_unlock_irqrestore(&trace_lock, flags);
	mutex_unlock(&mmiotrace_mutex);
}

static void disable_mmiotrace_soft(void)
{
	struct remap_trace *trace;
	struct remap_trace *tmp;
	unsigned long flags;

	mutex_lock(&mmiotrace_mutex);
	spin_lock_irqsave(&trace_lock, flags);

	if (!is_enabled())
		goto out;

	/*
	 * No locking required, because the caller ensures we are in a
	 * critical section via mutex, and is_enabled() is false,
	 * i.e. nothing can traverse or modify this list.
	 * Caller also ensures is_enabled() cannot change.
	 */
	list_for_each_entry_safe(trace, tmp, &trace_list, list) {
		// pr_notice("purging non-iounmapped trace @0x%08lx, size 0x%lx.\n",
		// 	  trace->probe.addr, trace->probe.len);
		if (!nommiotrace && trace->enabled) {
			unregister_kmmio_probe(&trace->probe);
			trace->enabled = 0;
			//list_del(&trace->list);
		}
	}

	//leave_uniprocessor();
	//kmmio_cleanup();
	//pr_info("disabled soft.\n");
out:
	spin_unlock_irqrestore(&trace_lock, flags);
	mutex_unlock(&mmiotrace_mutex);
	
	synchronize_rcu(); /* unregister_kmmio_probe() requirement */
}

static int pmemtrace_sampler(void *data)
{
	struct sampling_thread_data* thread_data = (struct sampling_thread_data*) data;
	unsigned int toggle = 1;


	if (thread_data->freq == 0)
		return -1;
	
	const unsigned int period = 1000000 / thread_data->freq;
	const unsigned int period_on = (period * (thread_data->duty_cycle)) / 100;
	const unsigned int period_off = (period * ((100 - thread_data->duty_cycle))) / 100;

	//const unsigned int period = 1000 / thread_data->freq;


	if (thread_data->is_time_triggered) {
		pr_info("period on: %u period off: %u\n", period_on, period_off);
		while (!kthread_should_stop()) {
			if (toggle) {
				//preempt_disable();
				enable_mmiotrace_soft();
				//preempt_enable_no_resched();
				//udelay(period_on);
				
				usleep_range(period_on, period_on + 10);
			} else {
				if (kmmio_count)
					disable_mmiotrace_soft();
				//udelay(period_off);
				usleep_range(period_off, period_off + 10);
			}

			toggle = !(toggle);
		}
	} else {
		pr_info("toggle probes every %u page faults.\n", thread_data->freq);
		while (!kthread_should_stop()) {
			//pr_info("%u\n", atomic_read(&fault_counter));
			if (atomic_read(&fault_counter) > thread_data->freq) {
				pr_info("toggle\n");
				if (toggle) {
					enable_mmiotrace_soft();
				} else {
					if (kmmio_count)
						disable_mmiotrace_soft();
				}

				atomic_set(&fault_counter, 0);
				toggle = !(toggle);
			}

			usleep_range(50, 100);
		}
	}

	return 0;
}

int enable_pmemtrace_sampler(unsigned int freq, unsigned int duty_cycle, unsigned int is_time_triggered)
{
	if (kth) {
		pr_warn("kth pointer is not null!\n");
		return -1;
	}



	// sampling_thread_da.period_on = period_on;
	// sampling_thread_da.duty_cycle = period_off;
	sampling_thread_da.freq = freq;
	sampling_thread_da.duty_cycle = duty_cycle;
	sampling_thread_da.is_time_triggered = is_time_triggered;
	

	if (is_enabled() && sampling_thread_da.freq > 0) {
		kth = kthread_create_on_cpu(pmemtrace_sampler, &sampling_thread_da, get_cpu(), "pmemtrace_sampler");

		if (kth != NULL) {
			wake_up_process(kth);
			pr_info("pmemtrace sampler enabled.\n");

			return 0;
		}

		return -1;
	}

	return 0;
}

int disable_pmemtrace_sampler(void)
{
	int ret;
	if (!kth) {
		pr_warn("kth pointer is null!\n");
		return -1;
	}

	ret = kthread_stop(kth);
	kth = NULL;
	return ret;
}

int set_pmemtrace_multicore(unsigned int is_on)
{
	if (is_enabled())
		return -EFAULT;
	atomic_set(&is_pmemtrace_multicore, (is_on > 0) ? 1 : 0);

	return 0;
}

void enable_mmiotrace(void)
{

	mutex_lock(&mmiotrace_mutex);
	if (is_enabled())
		goto out;

	if (nommiotrace)
		pr_info("MMIO tracing disabled.\n");
	kmmio_init();

	if (atomic_read(&is_pmemtrace_multicore) == 0)
		enter_uniprocessor();
	
	spin_lock_irq(&trace_lock);
	atomic_inc(&mmiotrace_enabled);
	spin_unlock_irq(&trace_lock);

	// The sampler might still be running, kill it just to be sure.
	disable_pmemtrace_sampler();

	enable_pmemtrace_sampler(0, 0, 1);

	pr_info("enabled.\n");
out:
	mutex_unlock(&mmiotrace_mutex);
}

void disable_mmiotrace(void)
{
	if (disable_pmemtrace_sampler() < 0)
		pr_warn("Could not stop sampler!\n");

	mutex_lock(&mmiotrace_mutex);
	if (!is_enabled())
		goto out;

	spin_lock_irq(&trace_lock);
	atomic_dec(&mmiotrace_enabled);
	BUG_ON(is_enabled());
	spin_unlock_irq(&trace_lock);

	clear_trace_list(); /* guarantees: no more kmmio callbacks */

	if (atomic_read(&is_pmemtrace_multicore) == 0)
		leave_uniprocessor();
	kmmio_cleanup();
	pr_info("disabled.\n");
out:
	mutex_unlock(&mmiotrace_mutex);
}
