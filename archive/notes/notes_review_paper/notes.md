OdinFS (efficient parallel writing) and SPMFS
Paper Metric: CPU seconds per accessed GiB

--> A close look of its on-DIMM buffering Lingfeng.... EuroSys '22.

--> Intel tracing: https://dl.acm.org/doi/pdf/10.1145/3453933.3454018


'
Idea paper: control parallel access to the PMEM device using micro-architectural counters.


Use CPU time instead of CPU cyles as number of CPU cycles may be impacted by dynamic frequency scaling. (/proc/stat)
Most of their analysis is based on the CPU time. Paper does not really go into the details of prefetching, cache line false sharing. et. cetera.
Measure parallel bandwidth of PMEM device by splitting workloads over a specified number of threads.

Parallel performance:
- Two parallel PM writes to a single PM DIMM is a reasonable compromise between bandwidth and CPU cost (using an semaphore).
- For each PM area, introduce a workqueue with a limited amount of worker threads pinned to the respective NUMA node.
- Cool idea, might be difficult to implement without prior knowledge: Use PEBS to sample all retired store instructions. Reset the counter to a preconfigured value on overflows. PEBS periodically caputures additional information for an event and writes it into a memery. The information includes the current instruction pointer, registers, and the address of a memory range. Once the buffer is full, PEBS triggers an interrupt to allow the OS to process the buffer.
	Accounting mechanism:
	- Add counter for the amount of bytes written to PM to the task attributes.
	- Setup PEBS to sample retired instructions for each process with a PMEM mapping.
	- Process the sample after PEBS triggers an interrupt
	- For each sampled store, discard DRAM reads (probably filter on physical pages?)
	- Disassemble the store instruction to obtain the operand size (1 bytes to 64 bytes).
	- Add the operand size multiplied by the sampling interval to the task's PM counter. The assumption is that for each sampled store, we missed an amount of similar stores proportional to the 		sampling interval.
	- Make the PM counters available as files in the proc file system.
	- Use Zydis to disassemble the instructions.
	- Major challange: use PEBS form the kernel, the driver intergrated in perf does not allow access to samples from kernel space. Disable perf access to PMU, set up PEBS directly.
	- Sample 0.01 % procent of the writes
	
- HeHem uses PEBS to estimate the relative hotness of pages by counting all sampled read and writes accesses per page, without inspecting the access size. HeMem samples every process,  rather than only processes that have a PM mapping.
-


- Two sources of write amplications: cache and inside PMEM device (we knew this already)

	
Framework:
- Measure (device) latency
- Measure (device/FS) bandwidth
- 
