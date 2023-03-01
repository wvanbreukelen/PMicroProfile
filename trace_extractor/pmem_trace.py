from __future__ import print_function
from bcc import BPF
from bcc.utils import printb

import binascii
import sys
import time
import datetime

trace_filename = ""
num_events = 0
print_sectors = False

if len(sys.argv) > 1:
	trace_filename = 'trace_' + str(sys.argv[1]) + '.trf'
else:
	current_time = datetime.datetime.now().strftime('%Y-%m-%d_%H-%M-%S')
	trace_filename = 'trace_' + current_time + '.trf'

trace_file = open(trace_filename, "w")

print(f"Writing trace to file {trace_filename} in CWD!")

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


void trace_write(struct pt_regs *ctx, void *pmem_addr, struct page *page, unsigned int off, unsigned int len) {
	bpf_trace_printk("W %p %u %u\\n", pmem_addr, off, len);
}

void trace_read(struct pt_regs *ctx, struct page *page, unsigned int off, void* pmem_addr, unsigned int len) {
	bpf_trace_printk("R %p %u %u\\n", pmem_addr, off, len);
}

void trace_pmem_req(struct pt_regs *ctx, void* pmem_device, struct page *page, unsigned int len, unsigned int off, unsigned int op, sector_t sector) {
	bpf_trace_printk("REQ %u %lu %u\\n", op, sector, len);
}

//void trace_dax_request(struct pt_regs *ctx, struct dax_device *dax_dev, pgoff_t pgoff, long nr_pages, void **kaddr) {
void trace_dax_request(struct pt_regs *ctx, void** kaddr, void* addr, long nr_pages) {	
	bpf_trace_printk("DAX %p %lu\\n", addr, nr_pages);
}
""")

if print_sectors:
	if BPF.get_kprobe_functions(b'pmem_do_bvec'):
		b.attach_kprobe(event="pmem_do_bvec", fn_name="trace_pmem_req")
		print("Found pmem_do_bvec function!")
else:
	if BPF.get_kprobe_functions(b'write_pmem'):
		b.attach_kprobe(event="write_pmem", fn_name="trace_write")
		print("Found write_pmem function!")

	if BPF.get_kprobe_functions(b'read_pmem'):
		# NOTE: we make the pmem.c:read_pmem(...) function non-static, otherwise, it will not be detected by eBPF as the
		# function is not included in the kernel map.
		b.attach_kprobe(event="read_pmem", fn_name="trace_read")
		print("Found read_pmem function!")

#if BPF.get_kprobe_functions(b'pmem_dax_direct_access'):#
#	b.attach_kprobe(event="pmem_dax_direct_access", fn_name="trace_dax_request")
#	print("Found pmem_dax_direct_access function!")


if BPF.get_kprobe_functions(b'set_dax_kaddr'):
	b.attach_kprobe(event="set_dax_kaddr", fn_name="trace_dax_request")
	print("Found set_dax_kaddr function!")

while 1:
	try:
		try:
			(task, pid, cpu, flags, ts, msg) = b.trace_fields()
			num_events += 1
			sys.stdout.write(f'\rCaptured {num_events} events...')
		except ValueError:
			print("Skipping...")
		#(op, pmem_addr, off, ) = msg.split()
		op = msg.split()[0]

		if op == b'W':
			(op, pmem_addr, off, length) = msg.split()
			if not print_sectors:
				trace_file.write(f"WRITE {cpu} {pmem_addr} {length}\n")
			#print(f"WRITE {cpu} {pmem_addr} {off}")
		elif op == b'R':
			(op, pmem_addr, off, length) = msg.split()
			if not print_sectors:
				trace_file.write(f"READ {cpu} {pmem_addr} {length}\n")
			#print(f"READ {cpu} {pmem_addr} {off}")
		elif op == b'DAX':
			(op, map_addr, num_pages) = msg.split()
			trace_file.write(f"DAX {cpu} {map_addr} {num_pages}\n")
			print(f"DAX {cpu} {map_addr} {num_pages}")
		elif op == b'REQ':
			(op, op_r_or_w, sector, length) = msg.split()
			
			if print_sectors:
				if op_r_or_w == b'0':
					trace_file.write(f"READ {cpu} {sector} {length}\n")
				elif op_r_or_w == b'1':
					trace_file.write(f"WRITE {cpu} {sector} {length}\n")
				else:
					raise TypeError(f"Unknown operation {op_r_or_w}")

	except KeyboardInterrupt:
		trace_file.close()
		
		print(f"\nFinished. Written {num_events} events to trace file!")
	
		exit()
