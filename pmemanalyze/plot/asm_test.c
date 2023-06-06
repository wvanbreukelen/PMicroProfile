mfence
// Two operands: 'r0' is the source address (e.g. a user space buffer), 'r1' is the destination address (e.g. PMEM-backed virtual memory)
movnti r0, r1
clflushopt r0
mfence