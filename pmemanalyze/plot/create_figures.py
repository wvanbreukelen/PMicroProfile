import sys
import os
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from scipy import stats

def NormalizeData(data):
    return (data - np.min(data)) / (np.max(data) - np.min(data))

# Get the script name
script_name = os.path.basename(__file__)

# Check if a filename was provided as a command-line argument
if len(sys.argv) != 2:
    print(f'Error: Please provide a filename as a command-line argument.\nUsage: python {script_name} filename.csv')
    sys.exit(1)

# Get the filename from the command-line argument
filename = sys.argv[1]

# Try to load the CSV file using pandas
try:
    df = pd.read_csv(filename)
except FileNotFoundError:
    print(f'Error: File "{filename}" not found.')
    sys.exit(1)
except Exception as e:
    print(f'Error: {e}')
    sys.exit(1)

print(df.columns)

# Remove the first row, as its measurements are sometimes unstable.
df = df.iloc[5:]

corr_matrix = df.corr()
print(corr_matrix)

df.insert(1, 'timestamp_sec', df['timestamp'] / 1e9)
df.insert(1, 'sample_duration_sec', df['sample_duration'] / 1e9)

df['num_barriers'] = df['retired_mfence'] + df['retired_sfence'] + df['retired_lfence']

df['avg_latency_inst_write'] = df['write_cycles'] / df['num_writes']
df['avg_latency_inst_read'] = df['read_cycles'] / df['num_reads']

df[(np.abs(stats.zscore(df['avg_latency_inst_write'])) < 3)]
df[(np.abs(stats.zscore(df['avg_latency_inst_read'])) < 3)]


# threshold = np.mean(df['avg_latency_inst_write']) + 1 * np.std(df['avg_latency_inst_write'])
# df.loc[df['avg_latency_inst_write'] > threshold, 'avg_latency_inst_write'] = np.nan

# threshold = np.mean(df['avg_latency_inst_read']) + 1 * np.std(df['avg_latency_inst_read'])
# df.loc[df['avg_latency_inst_read'] > threshold, 'avg_latency_inst_read'] = np.nan

# ( #OneBillion * ( UNC_M_PMM_RPQ_OCCUPANCY.ALL / UNC_M_PMM_RPQ_INSERTS ) / UNC_M_CLOCKTICKS:one_unit ) if #PMM_App_Direct else #NA

df['avg_latency_dev_read'] = (1e9 * (df['rpq_occupancy'] / df['rpq_inserts']) / df['unc_ticks'])
df['avg_latency_dev_write'] = (1e9 * (df['wpq_occupancy'] / df['wpq_inserts']) / df['unc_ticks'])


df['avg_latency_dev_read_dram'] = (1e9 * (df['dram_rpq_occupancy'] / df['dram_rpq_inserts']) / df['unc_ticks'])

Q1 = df.quantile(0.25)
Q3 = df.quantile(0.75)
IQR = Q3 - Q1

cols = ['avg_latency_dev_read_dram']

df = df[~((df[cols] < (Q1 - 1.5 * IQR)) |(df[cols] > (Q3 + 1.5 * IQR))).any(axis=1)]



# df[(np.abs(stats.zscore(df)) < 3).all(axis=1)]

# Calculate Read and Write Amplication
df['ra'] = (df['rpq_inserts'] * 64) / df['bytes_read']
df['wa'] = (df['wpq_inserts'] * 64) / df['bytes_written']

# Source: https://github.com/andikleen/pmu-tools/blob/master/clx_server_ratios.py (line 661)
df['pmem_read_bw'] = (df['rpq_inserts'] * 64 / 1e9) / df['sample_duration_sec']
df['dram_read_bw'] = (df['dram_rpq_inserts'] * 64 / 1e9) / df['sample_duration_sec']

df['pmem_write_bw'] = (df['wpq_inserts'] * 64 / 1e9) / df['sample_duration_sec']
# df['dram_write_bw'] = (df['dram_wpq_inserts'] * 64 / 1e9) / df['sample_duration_sec']

# Convert from bytes to Mebibyte
# df['bytes_read'] = df['bytes_read'] / (1024 * 1024)
# df['bytes_written'] = df['bytes_written'] / (1024 * 1024)

df['total_addr_distance_normalized'] = df['total_addr_distance'] / (df['num_reads'] + df['num_writes'] + df['num_flushes'])

# Average latency of data read request
# df['avg_latency_pmem_read'] = df['rpq_occupancy'] / df['rpq_inserts']



# Smooth the data using a 5-point moving average
window_size = int(len(df) * 0.005) if int(len(df) * 0.005) > 0 else 5
# df['smoothed_reads'] = df['num_reads'].rolling(window_size, center=True).mean()
# df['smoothed_writes'] = df['num_writes'].rolling(window_size, center=True).mean()
# df['smoothed_flushes'] = df['num_flushes'].rolling(window_size, center=True).mean()

df['smoothed_bytes_read'] = df['bytes_read'].rolling(window_size, center=True).mean()
df['smoothed_bytes_written'] = df['bytes_written'].rolling(window_size, center=True).mean()

df['smoothed_avg_latency_inst_write'] = df['avg_latency_inst_write'].rolling(window_size, center=True).mean()
df['smoothed_avg_latency_inst_read'] = df['avg_latency_inst_read'].rolling(window_size, center=True).mean()

df['smoothed_ra'] = df['ra'].rolling(window_size, center=True).mean()
df['smoothed_wa'] = df['wa'].rolling(window_size, center=True).mean()



# Print the contents of the DataFrame
with pd.option_context('display.max_rows', None, 'display.max_columns', None):  # more options can be specified also
    print(df.loc[[6]])

fig, (ax1, ax2, ax3, ax4, ax5, ax6, ax7, ax8) = plt.subplots(nrows=8, ncols=1, figsize=(12, 10))

fig.suptitle("pmemanalyze 200 MiB FIO benchmark, 100% / 0% R/W ratio (repeated 25 times)")

# Plot the number of reads and writes on the top subplot
ax1.plot(df['timestamp_sec'], df['num_reads'], label='Number of Reads')
ax1.plot(df['timestamp_sec'], df['num_writes'], label='Number of Writes')
ax1.plot(df['timestamp_sec'], df['num_flushes'], label='Number of Flushes')
# ax1.plot(df['timestamp_sec'], df['num_barriers'], label='Number of Barriers')
ax1.set_xlabel('Time (s)')
ax1.set_ylabel('Number Op.')
ax1.set_title("Retired instructions of time")
ax1.legend()

ax2.plot(df['timestamp_sec'], df['pmem_read_bw'], label='Read')
ax2.plot(df['timestamp_sec'], df['pmem_write_bw'], label='Write')
ax2.set_xlabel('Time (s)')
ax2.set_ylabel('GB/s')
ax2.set_title("PMEM Bandwidth")
ax2.legend()

# Plot the average latency per write operation on the middle subplot
ax3.plot(df['timestamp_sec'], df['avg_latency_inst_read'], label='Read')
ax3.plot(df['timestamp_sec'], df['avg_latency_inst_write'], label='Write')
ax3.set_xlabel('Time (s)')
ax3.set_ylabel('CPU cycles')
ax3.set_title("Instruction Latency: rdtsc timer")
ax3.legend()


# ax4.plot(df['timestamp_sec'], df['avg_latency_dev_read_dram'], label='DRAM Read Latency')
ax4.plot(df['timestamp_sec'], df['avg_latency_dev_read'], label='Read')
ax4.plot(df['timestamp_sec'], df['avg_latency_dev_write'], label='Write')
ax4.set_xlabel('Time (s)')
ax4.set_ylabel('Latency (ns)')
ax4.set_title("Instruction Latency: measured using PEBS PMEM counters")
ax4.legend()

# Plot the bytes read and written on the bottom subplot
# ax3.plot(df['timestamp_sec'], df['smoothed_bytes_read'].cumsum(), label='Bytes Read')
# ax3.plot(df['timestamp_sec'], df['smoothed_bytes_written'].cumsum(), label='Bytes Written')
# ax3.set_xlabel('Time (s)')
# ax3.set_ylabel('Mebibyte (MiB)')
# ax3.legend()

ax5.plot(df['timestamp_sec'], df['total_addr_distance_normalized'], label='Address Distance')

ax5.set_title("Inner-Sample Address Distance (ISAD)\nClose to 0.0 -> high spacial locality, Close to 1.0 -> low spacial locality)")
ax5.set_ylim([0.0, 0.5])
ax5.set_ylabel('ISAD')

ax6.plot(df['timestamp_sec'], df['ra'], label='RA')
ax6.plot(df['timestamp_sec'], df['wa'], label='WA')
ax6.set_xlabel('Time (s)')
ax6.set_ylabel('Factor')
ax6.set_title("Device Read/Write Amplification")
ax6.legend()

ax7.plot(df['timestamp_sec'], (df['total_read_write'] / (1024 * 1024)), label='Total Read/Write')
# ax7.plot(df['timestamp_sec'], (df['bytes_written'].cumsum() / (1024 * 1024)), label='Data Written')
ax7.legend()

ax8.plot(df['timestamp_sec'], df['num_barriers'] , label='Total Number of Barriers')
# ax7.plot(df['timestamp_sec'], (df['bytes_written'].cumsum() / (1024 * 1024)), label='Data Written')
ax8.legend()

plt.tight_layout()

# Display the plot
plt.show()

fig, (ax1, ax2, ax3) = plt.subplots(nrows=3, ncols=1, figsize=(6, 4))

fig.suptitle("pmemanalyze cache behavior, same workload")

ax1.plot(df['timestamp_sec'], df['any_scoop_pmm'])
ax1.set_xlabel('Time (s)')
ax1.set_title("PMEM direct loads")

ax2.plot(df['timestamp_sec'], df['any_scoop_l3_miss_dram'])
# ax7.plot(df['timestamp_sec'], df['l3_misses_local_pmm'], label='Cache miss PMEM')
ax2.set_ylabel('Count')
ax2.set_xlabel('Time (s)')
ax2.set_title("Cache Behavior: DRAM L3 Misses")

# ax8.plot(df['timestamp_sec'], df['any_scoop_l3_miss_dram'], label='Cache miss DRAM')
ax3.plot(df['timestamp_sec'], df['l3_misses_local_pmm'])
ax3.set_ylabel('Count')
ax3.set_xlabel('Time (s)')
ax3.set_title("Cache Behavior: PMEM L3 Misses")


plt.tight_layout()

# Display the plot
plt.show()