import re
import math
import numpy

import matplotlib.pyplot as plt

class TraceOperation:
    def __init__(self, op, size, time, addr, val):
        self.op = op
        self.size = int(size)
        self.time = float(time)
        self.addr = int(addr, 16)
        self.val = val


filename = "trace_dump.log"
ops = []

with open(filename, "r") as f:
    data = f.read()

# pattern for read and write operations
pattern = r"([RW])\s+([\d]+)+\s+([\d.]+)\s+\d+\s+(0x[0-9a-fA-F]+)\s+(0x[0-9a-fA-F]+)\s+.*"
matches = re.findall(pattern, data)
# print the matches
for match in matches:
    op, size, time, addr, val = match
    op = TraceOperation(op, size, time, addr, val)
    ops.append(op)

# print the operations
# for op in ops:
#     print(f"{op.op} operation of size {op.size} at time {op.time}, address {hex(op.addr)}, value {op.val}")

total_bytes_read = sum(op.size for op in ops if op.op == 'R')
total_bytes_written = sum(op.size for op in ops if op.op == 'W')
print(f"Total bytes read: {total_bytes_read}")
print(f"Total bytes written: {total_bytes_written}")

# Extract read and write operations and their addresses
read_ops = [op for op in ops if op.op == "R"]
write_ops = [op for op in ops if op.op == "W"]
read_addrs = [op.addr for op in read_ops]
write_addrs = [op.addr for op in write_ops]

# Determine the address range to plot
min_addr = min(read_addrs + write_addrs)
max_addr = max(read_addrs + write_addrs)
total_range = max_addr - min_addr

num_bars = 100
group_size = total_range // num_bars



# Group the addresses into chunks of 1000 bytes
num_groups = math.ceil((max_addr - min_addr + 1) / group_size)
grouped_read_ops = [[] for _ in range(num_groups)]
grouped_write_ops = [[] for _ in range(num_groups)]
for op in read_ops:
    group_idx = (op.addr - min_addr) // group_size
    grouped_read_ops[group_idx].append(op)
for op in write_ops:
    group_idx = (op.addr - min_addr) // group_size
    grouped_write_ops[group_idx].append(op)

# Count the number of read and write operations in each group
read_counts = [len(group) for group in grouped_read_ops]
write_counts = [len(group) for group in grouped_write_ops]

# Create the x-axis labels
xlabels = [f"0x{addr:08x}" for addr in range(min_addr, max_addr + 1, group_size)]


# Plot the bar chart
x = range(len(read_counts))
fig, ax = plt.subplots()
bar_width = 0.35
ax.bar(x, read_counts, bar_width, label="Read")
ax.bar([i + bar_width for i in x], write_counts, bar_width, label="Write")
ax.set_xticks(x)
ax.set_xticklabels(xlabels, rotation=45)
ax.legend()
plt.tight_layout()
plt.show()