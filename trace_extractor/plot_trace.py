import sys
import os
import matplotlib.pyplot as plt
import matplotlib.ticker as ticker
import numpy as np
import re

trace_filename = ""

reads_per_core = {}
writes_per_core = {}
total_read = 0
total_written = 0
sector_stats = {}
sector_stats_mmio = {}


sector_group_size = 30000000
mmiotrace_filename = None

if len(sys.argv) > 1:
	trace_filename = str(sys.argv[1])

	if not os.path.exists(trace_filename):
    		print(f"Error: file '{trace_filename}' does not exist")
    		exit(1)
    		
	if len(sys.argv) > 2:
    		mmiotrace_filename = str(sys.argv[2])
    		
    		if not os.path.exists(mmiotrace_filename):
    			print(f"Error: file '{mmiotrace_filename}' does not exist")
    			exit(1)
else:
	print("Please provide trace file!")
	exit(1)
	

with open(trace_filename, 'r') as f:
	for line in f:
		tokens = line.split()

		try:
		    op_type = str(tokens[0]).strip()
		    cpu_core = int(tokens[1])
		    pmem_addr = int(tokens[2][2:-1], 16)  # convert hex string to int
		   
		    if len(tokens) > 3:
		    	size = int(tokens[3][2:-1])
		    else:
		    	size = 0
		except ValueError:
		    print(f"Error: could not parse line '{line.strip()}'")
		    continue

		if op_type == 'READ':
			if cpu_core not in reads_per_core:
				reads_per_core[cpu_core] = 0
			reads_per_core[cpu_core] += 1
		elif op_type == 'WRITE':
			if cpu_core not in writes_per_core:
				writes_per_core[cpu_core] = 0
			writes_per_core[cpu_core] += 1
		
		sector_group = pmem_addr // sector_group_size
		if sector_group not in sector_stats:
			sector_stats[sector_group] = {'read': {'count': 0, 'size': 0}, 'write': {'count': 0, 'size': 0}, 'dax': {'count': 0}, 'write_mmio': {'count': 0}}
		if op_type == 'READ':
			sector_stats[sector_group]['read']['count'] += 1
			sector_stats[sector_group]['read']['size'] += size
			total_read += size
		elif op_type == 'WRITE':
			sector_stats[sector_group]['write']['count'] += 1
			sector_stats[sector_group]['write']['size'] += size
			total_written += size
		elif op_type == 'DAX':
			sector_stats[sector_group]['dax']['count'] += 1


if mmiotrace_filename:
	mark_start_pattern = re.compile(r"^MARK.*WRITE_START$")
	mark_end_pattern = re.compile(r"^MARK.*WRITE_STOP$")
	
	line_count = 0
	found_start = False
	
	with open(mmiotrace_filename, "r") as file:
		for line in file:
			line = line.strip()
			# Check if the current line contains the start mark
			if mark_start_pattern.search(line) and not found_start:
				print("Found start")
				found_start = True
				continue  # Skip to the next line

			# Check if the current line contains the end mark
			if mark_end_pattern.search(line):
				break
			    
			# If we've found the start mark, increment the line count
			if found_start:
				line_count += 1
				
				line_parts = line.split(' ')
				addr = int(line_parts[3], 16)
			    
				sector_group = addr // sector_group_size
				if sector_group not in sector_stats:
					sector_stats[sector_group] = {'read': {'count': 0, 'size': 0}, 'write': {'count': 0, 'size': 0}, 'dax': {'count': 0}, 'write_mmio': {'count': 0}}
			    
				sector_stats[sector_group]['write_mmio']['count'] += 1



print(f"Total amount of data read: {total_read} bytes ({round(total_read / (1024 ** 2), 2)} MiB, {round(total_read / (1024 ** 3), 2)} GiB)")
print(f"Total amount of data written: {total_written} bytes ({round(total_written / (1024 ** 2), 2)} MiB, {round(total_written / (1024 ** 3), 2)} GiB)")
print(f"DAX operations: {sum(sector['dax']['count'] for sector in sector_stats.values())}")

# get the list of unique CPU core IDs and sort them in ascending order
cores = sorted(set(reads_per_core.keys()) | set(writes_per_core.keys()))

# create separate lists of read and write counts for each CPU core
read_counts = [reads_per_core.get(core, 0) for core in cores]
write_counts = [writes_per_core.get(core, 0) for core in cores]

# create the x-axis labels as integers from 0 to the number of unique cores
x_labels = np.arange(len(cores))

bar_width = 0.35

# create the grouped bar chart
fig, ax = plt.subplots()
read_bars = ax.bar(x_labels, read_counts, bar_width, label='Reads')
write_bars = ax.bar(x_labels + bar_width, write_counts, bar_width, label='Writes')

# add the x-axis ticks and labels
ax.set_xticks(x_labels + bar_width / 2)
ax.set_xticklabels(cores)

# add the y-axis label and legend
ax.set_ylabel('Number of operations')
ax.set_xlabel('CPU core')
ax.legend()

# ensure that the bars do not overlap
fig.tight_layout()

# show the plot
plt.show()

# get the list of sector groups and sort them in ascending order
sector_groups = sorted(sector_stats.keys())

# create a list of write counts for each sector group
read_counts = [sector_stats[sg]['read']['count'] for sg in sector_groups]
write_counts = [sector_stats[sg]['write']['count'] for sg in sector_groups]
dax_counts = [sector_stats[sg]['dax']['count'] for sg in sector_groups]
write_counts_mmio = [sector_stats[sg]['write_mmio']['count'] for sg in sector_groups]


x_labels = np.array(sector_groups) * sector_group_size + (sector_group_size / 2)




# create the line plot

fig, ax = plt.subplots()

ax.set_yscale('log')


ax.plot(x_labels, read_counts, label='Reads')
ax.plot(x_labels, write_counts, label='Writes')
ax.plot(x_labels, dax_counts, label='DAX')
ax.plot(x_labels, write_counts_mmio, label='Write MMIO')


plt.xticks(np.arange(0, max(x_labels), int("50000000", 16)))
#plt.xlim([0, int("FFFFFFFF", 16)])

xlabels = map(lambda t: '0x%08X' % int(t), ax.get_xticks())    
ax.set_xticklabels(xlabels);


plt.xticks(np.arange(0, max(x_labels), int("50000000", 16)))
#plt.xlim([0, int("FFFFFFFF", 16)])

xlabels = map(lambda t: '0x%08X' % int(t), ax.get_xticks())    
ax.set_xticklabels(xlabels);

# add the x-axis label, y-axis label and legend
ax.set_xlabel(f'Namespace Virtual Address')
ax.set_ylabel('Number of operations')

ax.set_title('PMEM (24 GiB; QEMU) Operations Varmail SplitFS')
ax.legend()



plt.show()


# create a list of write sizes and read sizes for each sector group
read_sizes = [sector_stats[sg]['read']['size'] / sector_stats[sg]['read']['count'] if sector_stats[sg]['read']['count'] > 0 else 0 for sg in sector_groups]
write_sizes = [sector_stats[sg]['write']['size'] / sector_stats[sg]['write']['count'] if sector_stats[sg]['write']['count'] > 0 else 0 for sg in sector_groups]

# create the x-axis labels as integers from 0 to the number of sector groups
x_labels = np.arange(len(sector_groups))

# create the boxplot
fig, ax = plt.subplots()
ax.boxplot([read_sizes, write_sizes], labels=['Reads', 'Writes'])

# add the x-axis ticks and labels

# add the y-axis label and title
ax.set_ylabel('Average request size (bytes)')
ax.set_title('Average Request Size of Reads and Writes')

# show the plot
plt.show()


