// SPDX-License-Identifier: GPL-2.0
/* Support for MMIO probes.
 * Benfit many code from kprobes
 * (C) 2002 Louis Zhuang <louis.zhuang@intel.com>.
 *     2007 Alexander Eichner
 *     2008 Pekka Paalanen <pq@iki.fi>
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/list.h>
#include <linux/rculist.h>
#include <linux/spinlock.h>
#include <linux/hash.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/ptrace.h>
#include <linux/preempt.h>
#include <linux/percpu.h>
#include <linux/kdebug.h>
#include <linux/mutex.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>
#include <asm/tlbflush.h>
#include <linux/errno.h>
#include <asm/debugreg.h>
#include <linux/mmiotrace.h>
#include <asm/mmu_context.h>


#define KMMIO_PAGE_HASH_BITS 4
#define KMMIO_PAGE_TABLE_SIZE (1 << KMMIO_PAGE_HASH_BITS)

struct kmmio_fault_page {
	struct list_head list;
	struct kmmio_fault_page *release_next;
	unsigned long addr; /* the requested address */
	pteval_t old_presence; /* page presence prior to arming */
	bool armed;

	/*
	 * Number of times this page has been registered as a part
	 * of a probe. If zero, page is disarmed and this may be freed.
	 * Used only by writers (RCU) and post_kmmio_handler().
	 * Protected by kmmio_lock, when linked into kmmio_page_table.
	 */
	int count;

	bool scheduled_for_release;
};

struct kmmio_delayed_release {
	struct rcu_head rcu;
	struct kmmio_fault_page *release_list;
};

struct kmmio_context {
	struct kmmio_fault_page *fpage;
	struct kmmio_probe *probe;
	struct mm_struct *old_mm;
	unsigned long saved_flags;
	unsigned long addr;
	int active;
};

static DEFINE_SPINLOCK(kmmio_lock);

/* Protected by kmmio_lock */
unsigned int kmmio_count;

/* Read-protected by RCU, write-protected by kmmio_lock. */
static struct list_head kmmio_page_table[KMMIO_PAGE_TABLE_SIZE];
static LIST_HEAD(kmmio_probes);

static pte_t *lookup_user_address(unsigned long addr, unsigned int* level, struct mm_struct *mm)
{
	pgd_t *pgd;
	p4d_t *p4d;
	pud_t *pud;
	pmd_t *pmd;

	*level = PG_LEVEL_NONE;

	pgd = pgd_offset(mm, addr);

	if (pgd_none(*pgd))
		return NULL;

	p4d = p4d_offset(pgd, addr);
	if (p4d_none(*p4d))
		return NULL;

	*level = PG_LEVEL_512G;
	if (p4d_large(*p4d) || !p4d_present(*p4d))
		return (pte_t *)p4d;

	pud = pud_offset(p4d, addr);
	if (pud_none(*pud))
		return NULL;

	*level = PG_LEVEL_1G;
	if (pud_large(*pud) || !pud_present(*pud))
		return (pte_t *)pud;

	pmd = pmd_offset(pud, addr);
	if (pmd_none(*pmd))
		return NULL;

	*level = PG_LEVEL_2M;
	if (pmd_large(*pmd) || !pmd_present(*pmd))
		return (pte_t *)pmd;

	*level = PG_LEVEL_4K;

	return pte_offset_kernel(pmd, addr);
}

static struct list_head *kmmio_page_list(unsigned long addr)
{
	unsigned int l;
	pte_t *pte = lookup_address(addr, &l);

	if (!pte) {
		pte = lookup_user_address(addr, &l, current->mm);

		if (!pte)
			return NULL;

		//pr_info("kmmio_page_list: found user page\n");
	}
		
	addr &= page_level_mask(l);

	return &kmmio_page_table[hash_long(addr, KMMIO_PAGE_HASH_BITS)];
}

/* Accessed per-cpu */
static DEFINE_PER_CPU(struct kmmio_context, kmmio_ctx);

/*
 * this is basically a dynamic stabbing problem:
 * Could use the existing prio tree code or
 * Possible better implementations:
 * The Interval Skip List: A Data Structure for Finding All Intervals That
 * Overlap a Point (might be simple)
 * Space Efficient Dynamic Stabbing with Fast Queries - Mikkel Thorup
 */
/* Get the kmmio at this addr (if any). You must be holding RCU read lock. */
static struct kmmio_probe *get_kmmio_probe(unsigned long addr)
{
	struct kmmio_probe *p;
	list_for_each_entry_rcu(p, &kmmio_probes, list) {
		if (addr >= p->addr && addr < (p->addr + p->len))
			return p;
	}
	return NULL;
}

/* You must be holding RCU read lock. */
static struct kmmio_fault_page *get_kmmio_fault_page(unsigned long addr)
{
	struct list_head *head;
	struct kmmio_fault_page *f;
	unsigned int l;
	pte_t *pte = lookup_address(addr, &l);

	if (!pte) {
		if (current && current->mm)
			pte = lookup_user_address(addr, &l, current->mm);

		if (!pte) {
			return NULL;
		}
		//pr_info("get_kmmio_fault_page: found user page\n");
	}
		
	addr &= page_level_mask(l);
	head = kmmio_page_list(addr);
	list_for_each_entry_rcu(f, head, list) {
		if (f->addr == addr)
			return f;
	}
	return NULL;
}

static void clear_pud_presence(pud_t *pud, bool clear, pudval_t *old)
{
	pud_t new_pud;
	pudval_t v = pud_val(*pud);
	if (clear) {
		*old = v;
		new_pud = pud_mknotpresent(*pud);
	} else {
		/* Presume this has been called with clear==true previously */
		new_pud = __pud(*old);
	}
	set_pud(pud, new_pud);
}

static void clear_pmd_presence(pmd_t *pmd, bool clear, pmdval_t *old)
{
	pmd_t new_pmd;
	pmdval_t v = pmd_val(*pmd);
	if (clear) {
		*old = v;
		new_pmd = pmd_mknotpresent(*pmd);
	} else {
		/* Presume this has been called with clear==true previously */
		new_pmd = __pmd(*old);
	}
	set_pmd(pmd, new_pmd);
}

static void clear_pte_presence(pte_t *pte, bool clear, pteval_t *old)
{
	pteval_t v = pte_val(*pte);
	if (clear) {
		*old = v;
		/* Nothing should care about address */
		pte_clear(&init_mm, 0, pte);
	} else {
		/* Presume this has been called with clear==true previously */
		set_pte_atomic(pte, __pte(*old));
	}
}

static int clear_page_presence(struct kmmio_fault_page *f, bool clear)
{
	unsigned int level;
	pte_t *pte = lookup_address(f->addr, &level);

	if (!pte) {
		pte = lookup_user_address(f->addr, &level, current->mm);

		if (!pte) {
			pr_err("no pte for addr 0x%08lx\n", f->addr);
			return -1;
		}
	}

	switch (level) {
	case PG_LEVEL_1G:
		clear_pud_presence((pud_t *)pte, clear, &f->old_presence);
		break;
	case PG_LEVEL_2M:
		clear_pmd_presence((pmd_t *)pte, clear, &f->old_presence);
		break;
	case PG_LEVEL_4K:
		clear_pte_presence(pte, clear, &f->old_presence);
		break;
	default:
		pr_err("unexpected page level 0x%x.\n", level);
		return -1;
	}

	__flush_tlb_one_kernel(f->addr);
	return 0;
}

/*
 * Mark the given page as not present. Access to it will trigger a fault.
 *
 * Struct kmmio_fault_page is protected by RCU and kmmio_lock, but the
 * protection is ignored here. RCU read lock is assumed held, so the struct
 * will not disappear unexpectedly. Furthermore, the caller must guarantee,
 * that double arming the same virtual address (page) cannot occur.
 *
 * Double disarming on the other hand is allowed, and may occur when a fault
 * and mmiotrace shutdown happen simultaneously.
 */
static int arm_kmmio_fault_page(struct kmmio_fault_page *f)
{
	int ret;
	WARN_ONCE(f->armed, KERN_ERR pr_fmt("kmmio page already armed.\n"));
	if (f->armed) {
		pr_warning("double-arm: addr 0x%08lx, ref %d, old %d\n",
			   f->addr, f->count, !!f->old_presence);
		//return 0;
	}
	ret = clear_page_presence(f, true);
	WARN_ONCE(ret < 0, KERN_ERR pr_fmt("arming at 0x%08lx failed.\n"),
		  f->addr);
	f->armed = true;
	return ret;
}

/** Restore the given page to saved presence state. */
static void disarm_kmmio_fault_page(struct kmmio_fault_page *f)
{
	int ret = clear_page_presence(f, false);
	WARN_ONCE(ret < 0,
			KERN_ERR "kmmio disarming at 0x%08lx failed.\n", f->addr);
	f->armed = false;
}

/*
 * This is being called from do_page_fault().
 *
 * We may be in an interrupt or a critical section. Also prefecthing may
 * trigger a page fault. We may be in the middle of process switch.
 * We cannot take any locks, because we could be executing especially
 * within a kmmio critical section.
 *
 * Local interrupts are disabled, so preemption cannot happen.
 * Do not enable interrupts, do not sleep, and watch out for other CPUs.
 */
/*
 * Interrupts are disabled on entry as trap3 is an interrupt gate
 * and they remain disabled throughout this function.
 */
int kmmio_handler(struct pt_regs *regs, unsigned long addr, unsigned long hw_error_code)
{
	struct kmmio_context *ctx;
	struct kmmio_fault_page *faultpage;
	int ret = 0; /* default to fault not handled */
	unsigned long page_base = addr;
	unsigned int l;
	struct mm_struct *old_mm = NULL;
	struct kmmio_probe *p = NULL;
	unsigned long flags;

	/*
	 * Preemption is now disabled to prevent process switch during
	 * single stepping. We can only handle one active kmmio trace
	 * per cpu, so ensure that we finish it before something else
	 * gets to run. We also hold the RCU read lock over single
	 * stepping to avoid looking up the probe and kmmio_fault_page
	 * again.
	 */

	preempt_disable();
	rcu_read_lock();

	pte_t *pte = lookup_address(addr, &l);
	if (!pte) {
		
		p = get_kmmio_probe(page_base);

		if (p && p->user_task) {
			old_mm = current->active_mm;
			switch_mm(old_mm, p->user_task->mm, current);

			pte = lookup_user_address(addr, &l, p->user_task->mm);
		}

		if (!pte) {
			//pr_warn("Lookup address failed in kmmio_handler!!!!\n");
			ret = -EINVAL;
			
			goto no_kmmio;
		}
	}
		
	page_base &= page_level_mask(l);



	faultpage = get_kmmio_fault_page(page_base);
	if (!faultpage) {
		/*
		 * Either this page fault is not caused by kmmio, or
		 * another CPU just pulled the kmmio probe from under
		 * our feet. The latter case should not be possible.
		 */

		//pr_warn("kmmio_handler: unable to get fauling page. Lock still held?\n");

		goto no_kmmio_switch_mm;
	}

	ctx = &get_cpu_var(kmmio_ctx);
	if (ctx->active) {
		if (page_base == ctx->addr) {
			/*
			 * A second fault on the same page means some other
			 * condition needs handling by do_page_fault(), the
			 * page really not being present is the most common.
			 */
			pr_debug("secondary hit for 0x%08lx CPU %d.\n",
				 addr, smp_processor_id());

			if (!faultpage->old_presence)
				pr_info("unexpected secondary hit for address 0x%08lx on CPU %d.\n",
					addr, smp_processor_id());
		} else {
			/*
			 * Prevent overwriting already in-flight context.
			 * This should not happen, let's hope disarming at
			 * least prevents a panic.
			 */
			pr_emerg("recursive probe hit on CPU %d, for address 0x%08lx. Ignoring.\n",
				 smp_processor_id(), addr);
			pr_emerg("previous hit was at 0x%08lx.\n", ctx->addr);
			disarm_kmmio_fault_page(faultpage);
		}
		goto no_kmmio_ctx;
	}
	ctx->active++;

	ctx->fpage = faultpage;
	ctx->probe = get_kmmio_probe(page_base);
	ctx->saved_flags = (regs->flags & (X86_EFLAGS_TF | X86_EFLAGS_IF));
	ctx->addr = page_base;

	ctx->old_mm = (old_mm) ? old_mm : NULL;

	if (ctx->probe && ctx->probe->pre_handler)
		ctx->probe->pre_handler(ctx->probe, regs, addr, hw_error_code);

	/*
	 * Enable single-stepping and disable interrupts for the faulting
	 * context. Local interrupts must not get enabled during stepping.
	 */
	regs->flags |= X86_EFLAGS_TF;
	regs->flags &= ~X86_EFLAGS_IF;

	/* Now we set present bit in PTE and single step. */
	disarm_kmmio_fault_page(ctx->fpage);

	/*
	 * If another cpu accesses the same page while we are stepping,
	 * the access will not be caught. It will simply succeed and the
	 * only downside is we lose the event. If this becomes a problem,
	 * the user should drop to single cpu before tracing.
	 */

	if (p && p->user_task && old_mm) {
		struct mm_struct* prev_temp;

		switch_mm(prev_temp, old_mm, current);

		// We are still holding the rcu read lock. To avoid locking errors while in single stepping mode we manually decrease the lock count by one.
		// if (current->lockdep_depth > 0)
		// 	current->lockdep_depth--;
	}
		

	put_cpu_var(kmmio_ctx);
	
	return 1; /* fault handled */

no_kmmio_ctx:
	put_cpu_var(kmmio_ctx);
no_kmmio_switch_mm:
	if (p && p->user_task && old_mm) {
		struct mm_struct* prev_temp;

		switch_mm(prev_temp, old_mm, current);
		old_mm = NULL;
	}
no_kmmio:
	//pr_info("releasing rcu_read_lock\n");
	rcu_read_unlock();
	preempt_enable_no_resched();
	return ret;
}

/*
 * Interrupts are disabled on entry as trap1 is an interrupt gate
 * and they remain disabled throughout this function.
 * This must always get called as the pair to kmmio_handler().
 */
static int post_kmmio_handler(unsigned long condition, struct pt_regs *regs)
{
	int ret = 0;
	struct kmmio_context *ctx = &get_cpu_var(kmmio_ctx);

	if (!ctx->active) {
		/*
		 * debug traps without an active context are due to either
		 * something external causing them (f.e. using a debugger while
		 * mmio tracing enabled), or erroneous behaviour
		 */
		pr_warning("unexpected debug trap on CPU %d.\n",
			   smp_processor_id());
		goto out;
	}

	if (ctx->probe && ctx->probe->post_handler)
		ctx->probe->post_handler(ctx->probe, condition, regs);

	/* Prevent racing against release_kmmio_fault_page(). */
	spin_lock(&kmmio_lock);
	if (ctx->fpage->count)
		arm_kmmio_fault_page(ctx->fpage);
	spin_unlock(&kmmio_lock);

	regs->flags &= ~X86_EFLAGS_TF;
	regs->flags |= ctx->saved_flags;

	/* These were acquired in kmmio_handler(). */
	ctx->active--;
	BUG_ON(ctx->active);
	
	if (ctx->old_mm) {
		struct mm_struct* prev_temp;

		switch_mm(prev_temp, ctx->old_mm, current);
		// We are still holding the rcu read lock. To avoid locking errors while in single stepping mode we manually decrease the lock count by one.
		//current->lockdep_depth++;
	}

	//pr_info("releasing rcu_read_lock\n");
	
	rcu_read_unlock();
	preempt_enable_no_resched();

	/*
	 * if somebody else is singlestepping across a probe point, flags
	 * will have TF set, in which case, continue the remaining processing
	 * of do_debug, as if this is not a probe hit.
	 */
	if (!(regs->flags & X86_EFLAGS_TF))
		ret = 1;
out:
	put_cpu_var(kmmio_ctx);
	return ret;
}

/* You must be holding kmmio_lock. */
static int add_kmmio_fault_page(unsigned long addr)
{
	struct kmmio_fault_page *f;

	f = get_kmmio_fault_page(addr);
	if (f) {
		if (!f->count)
			arm_kmmio_fault_page(f);
		f->count++;
		return 0;
	}

	f = kzalloc(sizeof(*f), GFP_ATOMIC);
	if (!f)
		return -1;

	f->count = 1;
	f->addr = addr;

	if (arm_kmmio_fault_page(f)) {
		kfree(f);
		return -1;
	}

	list_add_rcu(&f->list, kmmio_page_list(f->addr));

	return 0;
}

/* You must be holding kmmio_lock. */
static void release_kmmio_fault_page(unsigned long addr,
				struct kmmio_fault_page **release_list)
{
	struct kmmio_fault_page *f;


	f = get_kmmio_fault_page(addr);

	if (!f)
		return;

	f->count--;
	BUG_ON(f->count < 0);
	if (!f->count) {
		disarm_kmmio_fault_page(f);
		if (!f->scheduled_for_release) {
			f->release_next = *release_list;
			*release_list = f;
			f->scheduled_for_release = true;
		}
	}
}



/*
 * With page-unaligned ioremaps, one or two armed pages may contain
 * addresses from outside the intended mapping. Events for these addresses
 * are currently silently dropped. The events may result only from programming
 * mistakes by accessing addresses before the beginning or past the end of a
 * mapping.
 */
int register_kmmio_probe(struct kmmio_probe *p)
{
	unsigned long flags;
	int ret = 0;
	unsigned long size = 0;
	unsigned long addr = p->addr & PAGE_MASK;
	const unsigned long size_lim = p->len + (p->addr & ~PAGE_MASK);
	unsigned int l;
	pte_t *pte;

	spin_lock_irqsave(&kmmio_lock, flags);
	if (get_kmmio_probe(addr)) {
		ret = -EEXIST;
		goto out;
	}

	pr_info("register_kmmio_probe: looking up info for address 0x%lx\n", addr);

	pte = lookup_address(addr, &l);

	if (!pte) {
		if (p->user_task) {
			// Check if the address can be found in the user space area.
			
			pte = lookup_user_address(addr, &l, current->mm);
		}

		if (!pte) {
			ret = -EINVAL;
			goto out;
		}
	}

	//pr_info("pte for address %p: %p\n", addr, pte);

	kmmio_count++;
	list_add_rcu(&p->list, &kmmio_probes);
	while (size < size_lim) {
		if (add_kmmio_fault_page(addr + size))
			pr_err("Unable to set page fault.\n");
		size += page_level_size(l);
	}
out:
	spin_unlock_irqrestore(&kmmio_lock, flags);
	/*
	 * XXX: What should I do here?
	 * Here was a call to global_flush_tlb(), but it does not exist
	 * anymore. It seems it's not needed after all.
	 */
	return ret;
}
EXPORT_SYMBOL(register_kmmio_probe);

static void rcu_free_kmmio_fault_pages(struct rcu_head *head)
{
	struct kmmio_delayed_release *dr = container_of(
						head,
						struct kmmio_delayed_release,
						rcu);
	struct kmmio_fault_page *f = dr->release_list;
	while (f) {
		struct kmmio_fault_page *next = f->release_next;
		BUG_ON(f->count);
		kfree(f);
		f = next;
	}
	kfree(dr);
}

/**
 * 
 * GNU gdb (Ubuntu 12.1-0ubuntu1~22.04) 12.1
Copyright (C) 2022 Free Software Foundation, Inc.
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>
This is free software: you are free to change and redistribute it.
There is NO WARRANTY, to the extent permitted by law.
Type "show copying" and "show warranty" for details.
This GDB was configured as "x86_64-linux-gnu".
Type "show configuration" for configuration details.
For bug reporting instructions, please see:
<https://www.gnu.org/software/gdb/bugs/>.
Find the GDB manual and other documentation resources online at:
    <http://www.gnu.org/software/gdb/documentation/>.
--Type <RET> for more, q to quit, c to continue without paging--c

For help, type "help".
Type "apropos word" to search for commands related to "word"...
Reading symbols from vmlinux...
(gdb) target remote :1234
Remote debugging using :1234
default_idle () at arch/x86/kernel/process.c:573
573             trace_cpu_idle_rcuidle(PWR_EVENT_EXIT, smp_processor_id());
(gdb) c
Continuing.
^C
Thread 1 received signal SIGINT, Interrupt.
0xffffffff810c7113 in halt () at ./arch/x86/include/asm/irqflags.h:113
113     }
(gdb) bt
#0  0xffffffff810c7113 in halt ()
    at ./arch/x86/include/asm/irqflags.h:113
#1  kvm_wait (val=<optimized out>, ptr=<optimized out>)
    at arch/x86/kernel/kvm.c:847
#2  kvm_wait (ptr=0xffffffff83271f60 <kmmio_lock> "\003", val=3 '\003')
    at arch/x86/kernel/kvm.c:829
#3  0xffffffff81129cc6 in pv_wait (val=<optimized out>, 
    ptr=<optimized out>) at ./arch/x86/include/asm/paravirt.h:652
#4  pv_wait_head_or_lock (node=<optimized out>, lock=<optimized out>)
    at kernel/locking/qspinlock_paravirt.h:470
#5  __pv_queued_spin_lock_slowpath (
    lock=0xffffffff83271f60 <kmmio_lock>, val=<optimized out>)
    at kernel/locking/qspinlock.c:507
--Type <RET> for more, q to quit, c to continue without paging--c
#6  0xffffffff81c73b45 in pv_queued_spin_lock_slowpath (val=<optimized out>, lock=<optimized out>) at ./arch/x86/include/asm/paravirt.h:642
#7  queued_spin_lock_slowpath (val=<optimized out>, lock=<optimized out>) at ./arch/x86/include/asm/qspinlock.h:50
#8  queued_spin_lock (lock=<optimized out>) at ./include/asm-generic/qspinlock.h:81
#9  do_raw_spin_lock_flags (flags=<optimized out>, lock=<optimized out>) at ./include/linux/spinlock.h:193
#10 __raw_spin_lock_irqsave (lock=<optimized out>) at ./include/linux/spinlock_api_smp.h:119
#11 _raw_spin_lock_irqsave (lock=0xffffffff83271f60 <kmmio_lock>) at kernel/locking/spinlock.c:159
#12 0xffffffff810d5640 in remove_kmmio_fault_pages (head=0xffff8880afaf2b40) at ./include/linux/spinlock.h:327
#13 0xffffffff8114a32c in __rcu_reclaim (head=<optimized out>, rn=<optimized out>) at kernel/rcu/rcu.h:222
#14 rcu_do_batch (rdp=<optimized out>) at kernel/rcu/tree.c:2165
#15 rcu_core () at kernel/rcu/tree.c:2385
#16 0xffffffff820000c7 in __do_softirq () at kernel/softirq.c:292
#17 0xffffffff810e7d1e in invoke_softirq () at kernel/softirq.c:373
#18 irq_exit () at kernel/softirq.c:413
#19 0xffffffff81e02458 in exiting_irq () at ./arch/x86/include/asm/apic.h:538
#20 smp_apic_timer_interrupt (regs=<optimized out>) at arch/x86/kernel/apic/apic.c:1150
#21 0xffffffff81e01b0f in apic_timer_interrupt () at arch/x86/entry/entry_64.S:834
#22 0xffffc900006dfe98 in ?? ()
*/

static void remove_kmmio_fault_pages(struct rcu_head *head)
{
	struct kmmio_delayed_release *dr =
		container_of(head, struct kmmio_delayed_release, rcu);
	struct kmmio_fault_page *f = dr->release_list;
	struct kmmio_fault_page **prevp = &dr->release_list;
	unsigned long flags;

	spin_lock_irqsave(&kmmio_lock, flags);
	while (f) {
		if (!f->count) {
			list_del_rcu(&f->list);
			prevp = &f->release_next;
		} else {
			*prevp = f->release_next;
			f->release_next = NULL;
			f->scheduled_for_release = false;
		}
		f = *prevp;
	}
	spin_unlock_irqrestore(&kmmio_lock, flags);

	/* This is the real RCU destroy call. */
	call_rcu(&dr->rcu, rcu_free_kmmio_fault_pages);
}

/*
 * Remove a kmmio probe. You have to synchronize_rcu() before you can be
 * sure that the callbacks will not be called anymore. Only after that
 * you may actually release your struct kmmio_probe.
 *
 * Unregistering a kmmio fault page has three steps:
 * 1. release_kmmio_fault_page()
 *    Disarm the page, wait a grace period to let all faults finish.
 * 2. remove_kmmio_fault_pages()
 *    Remove the pages from kmmio_page_table.
 * 3. rcu_free_kmmio_fault_pages()
 *    Actually free the kmmio_fault_page structs as with RCU.
 */
void unregister_kmmio_probe(struct kmmio_probe *p)
{
	unsigned long flags;
	unsigned long size = 0;
	unsigned long addr = p->addr & PAGE_MASK;
	const unsigned long size_lim = p->len + (p->addr & ~PAGE_MASK);
	struct kmmio_fault_page *release_list = NULL;
	struct kmmio_delayed_release *drelease;
	unsigned int l;
	pte_t *pte;

	pte = lookup_address(addr, &l);
	if (!pte) {
		if (p->user_task) {
			spin_lock_irqsave(&kmmio_lock, flags);

			while (size < size_lim) {
				release_kmmio_fault_page(addr + size, &release_list);
				size += page_level_size(l);
			}

			if ((&p->list))
				list_del_rcu(&p->list);
			kmmio_count--;
			spin_unlock_irqrestore(&kmmio_lock, flags);

			if (!release_list)
				return;

			drelease = kmalloc(sizeof(*drelease), GFP_ATOMIC);
			if (!drelease) {
				pr_crit("leaking kmmio_fault_page objects.\n");
				return;
			}
			drelease->release_list = release_list;

			call_rcu(&drelease->rcu, remove_kmmio_fault_pages);

			return;
			//pte = lookup_user_address(addr, &l, current->mm);
		}

		if (!pte)
			return;


	}
		
	spin_lock_irqsave(&kmmio_lock, flags);
	while (size < size_lim) {
		release_kmmio_fault_page(addr + size, &release_list);
		size += page_level_size(l);
	}

	if ((&p->list))
		list_del_rcu(&p->list);
	kmmio_count--;
	spin_unlock_irqrestore(&kmmio_lock, flags);

	if (!release_list)
		return;

	drelease = kmalloc(sizeof(*drelease), GFP_ATOMIC);
	if (!drelease) {
		pr_crit("leaking kmmio_fault_page objects.\n");
		return;
	}
	drelease->release_list = release_list;

	/*
	 * This is not really RCU here. We have just disarmed a set of
	 * pages so that they cannot trigger page faults anymore. However,
	 * we cannot remove the pages from kmmio_page_table,
	 * because a probe hit might be in flight on another CPU. The
	 * pages are collected into a list, and they will be removed from
	 * kmmio_page_table when it is certain that no probe hit related to
	 * these pages can be in flight. RCU grace period sounds like a
	 * good choice.
	 *
	 * If we removed the pages too early, kmmio page fault handler might
	 * not find the respective kmmio_fault_page and determine it's not
	 * a kmmio fault, when it actually is. This would lead to madness.
	 */
	call_rcu(&drelease->rcu, remove_kmmio_fault_pages);
}
EXPORT_SYMBOL(unregister_kmmio_probe);

static int
kmmio_die_notifier(struct notifier_block *nb, unsigned long val, void *args)
{
	struct die_args *arg = args;
	unsigned long* dr6_p = (unsigned long *)ERR_PTR(arg->err);

	if (val == DIE_DEBUG && (*dr6_p & DR_STEP))
		if (post_kmmio_handler(*dr6_p, arg->regs) == 1) {
			/*
			 * Reset the BS bit in dr6 (pointed by args->err) to
			 * denote completion of processing
			 */
			*dr6_p &= ~DR_STEP;
			return NOTIFY_STOP;
		}

	return NOTIFY_DONE;
}

static struct notifier_block nb_die = {
	.notifier_call = kmmio_die_notifier
};

int kmmio_init(void)
{
	int i;

	for (i = 0; i < KMMIO_PAGE_TABLE_SIZE; i++)
		INIT_LIST_HEAD(&kmmio_page_table[i]);

	return register_die_notifier(&nb_die);
}

void kmmio_cleanup(void)
{
	int i;

	unregister_die_notifier(&nb_die);
	for (i = 0; i < KMMIO_PAGE_TABLE_SIZE; i++) {
		WARN_ONCE(!list_empty(&kmmio_page_table[i]),
			KERN_ERR "kmmio_page_table not empty at cleanup, any further tracing will leak memory.\n");
	}
}
