import sys
import os
import matplotlib.pyplot as plt
import numpy as np

trace_filename = ""

reads_per_core = {}
writes_per_core = {}
total_read = 0
total_written = 0
sector_stats = {}

sector_group_size = 100000

if len(sys.argv) > 1:
	trace_filename = str(sys.argv[1])

	if not os.path.exists(trace_filename):
    		print(f"Error: file '{trace_filename}' does not exist")
    		exit(1)
else:
	print("Please provide trace file!")
	exit(1)
	

with open(trace_filename, 'r') as f:
	for line in f:
		tokens = line.split()

		if len(tokens) != 4:
		    print(f"Error: line '{line.strip()}' has {len(tokens)} tokens instead of 4")
		    continue

		try:
		    op_type = str(tokens[0]).strip()
		    cpu_core = int(tokens[1])
		    sector = int(tokens[2][2:-1], 16)  # convert hex string to int
		    size = int(tokens[3][2:-1])
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
		
		sector_group = sector // sector_group_size
		if sector_group not in sector_stats:
			sector_stats[sector_group] = {'read': {'count': 0, 'size': 0}, 'write': {'count': 0, 'size': 0}}
		if op_type == 'READ':
			sector_stats[sector_group]['read']['count'] += 1
			sector_stats[sector_group]['read']['size'] += (size * 512)
			total_read += (size * 512)
		elif op_type == 'WRITE':
			sector_stats[sector_group]['write']['count'] += 1
			sector_stats[sector_group]['write']['size'] += (size * 512)
			total_written += (size * 512)

print(f"Total amount of data read: {total_read} bytes")
print(f"Total amount of data written: {total_written} bytes")

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


x_labels = np.array(sector_groups) * sector_group_size + (sector_group_size / 2)
x_labels2 = np.array(sector_groups) * sector_group_size + (sector_group_size / 2)

# create the line plot
fig, ax = plt.subplots()
ax.plot(x_labels, read_counts, label='Reads')
ax.plot(x_labels2, write_counts, label='Writes')

# add the x-axis label, y-axis label and legend
ax.set_xlabel(f'Device Sector')
ax.set_ylabel('Frequency')
ax.legend()


plt.show()

# create a list of average read sizes and write sizes for each sector group
read_sizes = [sector_stats[sg]['read']['size'] / sector_stats[sg]['read']['count']
              if sector_stats[sg]['read']['count'] > 0 else 0 for sg in sector_groups]
write_sizes = [sector_stats[sg]['write']['size'] / sector_stats[sg]['write']['count']
               if sector_stats[sg]['write']['count'] > 0 else 0 for sg in sector_groups]

# create the line plot
fig, ax = plt.subplots()
ax.plot(sector_groups, read_sizes, label='Reads')
ax.plot(sector_groups, write_sizes, label='Writes')

# add the x-axis label, y-axis label and legend
ax.set_xlabel(f'Device Sector')
ax.set_ylabel('Average Size')
ax.legend()
plt.show()


