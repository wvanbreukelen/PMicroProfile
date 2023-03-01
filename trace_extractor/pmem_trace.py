from __future__ import print_function
from bcc import BPF
from bcc.utils import printb

import binascii

b = BPF(text="""
#include <uapi/linux/ptrace.h>
#include <linux/blk-mq.h>
#include <uapi/linux/virtio_pmem.h>
#include <linux/libnvdimm.h>
#include <linux/dax.h>
#include <linux/types.h>

struct access_t {
	void* pmem_addr;
	unsigned int off;
};


void trace_write(struct pt_regs *ctx, void *pmem_addr, struct page *page, unsigned int off) {
	bpf_trace_printk("W %p %u\\n", pmem_addr, off);
}

void trace_read(struct pt_regs *ctx, struct page *page, unsigned int off, void* pmem_addr) {
	bpf_trace_printk("R %p %u\\n", pmem_addr, off);
}

void trace_pmem_req(struct pt_regs *ctx, void* pmem_device, struct page *page, unsigned int len, unsigned int off, unsigned int op, sector_t sector) {
	bpf_trace_printk("REQ %u %lu %u\\n", op, sector, len);
}

//void trace_dax_request(struct pt_regs *ctx, struct dax_device *dax_dev, pgoff_t pgoff, long nr_pages, void **kaddr) {
void trace_dax_request(struct pt_regs *ctx, void** kaddr, void* addr, long nr_pages) {	
	bpf_trace_printk("DAX %p %lu\\n", addr, nr_pages);
}
""")

#if BPF.get_kprobe_functions(b'write_pmem'):
#	b.attach_kprobe(event="write_pmem", fn_name="trace_write")
#	print("Found write_pmem function!")

#if BPF.get_kprobe_functions(b'read_pmem'):
	# NOTE: we make the pmem.c:read_pmem(...) function non-static, otherwise, it will not be detected by eBPF as the
	# function is not included in the kernel map.
#	b.attach_kprobe(event="read_pmem", fn_name="trace_read")
#	print("Found read_pmem function!")

#if BPF.get_kprobe_functions(b'pmem_dax_direct_access'):#
#	b.attach_kprobe(event="pmem_dax_direct_access", fn_name="trace_dax_request")
#	print("Found pmem_dax_direct_access function!")


if BPF.get_kprobe_functions(b'set_dax_kaddr'):
	b.attach_kprobe(event="set_dax_kaddr", fn_name="trace_dax_request")
	print("Found set_dax_kaddr function!")

if BPF.get_kprobe_functions(b'pmem_do_bvec'):
	b.attach_kprobe(event="pmem_do_bvec", fn_name="trace_pmem_req")
	print("Found pmem_do_bvec function!")

while 1:
	try:
		try:
			(task, pid, cpu, flags, ts, msg) = b.trace_fields()
		except ValueError:
			print("Skipping...")
		#(op, pmem_addr, off, ) = msg.split()
		op = msg.split()[0]

		if op == b'W':
			(op, pmem_addr, off) = msg.split()
			print(f"WRITE {cpu} {pmem_addr} {off}")
		elif op == b'R':
			(op, pmem_addr, off) = msg.split()
			print(f"READ {cpu} {pmem_addr} {off}")
		elif op == b'DAX':
			(op, map_addr, num_pages) = msg.split()
			print(f"DAX {cpu} {map_addr} {num_pages}")
		elif op == b'REQ':
			(op, op_r_or_w, sector, length) = msg.split()
			#if op_r_or_w == b'0':
			#	print(f"READ {cpu} {sector} {length}")
			#else:
			#	print(f"WRITE {cpu} {sector} {length}")
		else:
			print(f"UNKNOWN OP {op}")
	except KeyboardInterrupt:
		exit()
