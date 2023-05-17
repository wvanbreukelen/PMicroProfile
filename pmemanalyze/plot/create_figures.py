import sys
import os
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from scipy import stats

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

df.insert(1, 'timestamp_sec', df['timestamp'] / 1e9)

df['avg_latency_inst_write'] = df['write_cycles'] / df['num_writes']
df['avg_latency_inst_read'] = df['read_cycles'] / df['num_reads']

df[(np.abs(stats.zscore(df['avg_latency_inst_write'])) < 3)]
df[(np.abs(stats.zscore(df['avg_latency_inst_read'])) < 3)]


# threshold = np.mean(df['avg_latency_inst_write']) + 1 * np.std(df['avg_latency_inst_write'])
# df.loc[df['avg_latency_inst_write'] > threshold, 'avg_latency_inst_write'] = np.nan

# threshold = np.mean(df['avg_latency_inst_read']) + 1 * np.std(df['avg_latency_inst_read'])
# df.loc[df['avg_latency_inst_read'] > threshold, 'avg_latency_inst_read'] = np.nan

# ( #OneBillion * ( UNC_M_PMM_RPQ_OCCUPANCY.ALL / UNC_M_PMM_RPQ_INSERTS ) / UNC_M_CLOCKTICKS:one_unit ) if #PMM_App_Direct else #NA
df['avg_latency_dev_read'] = (1000000000 * (df['rpq_occupancy'] / df['rpq_inserts']) / df['unc_ticks'])
df['avg_latency_dev_write'] = (1000000000 * (df['wpq_occupancy'] / df['wpq_inserts']) / df['unc_ticks'])

# Calculate Read and Write Amplication
df['ra'] = (df['rpq_inserts'] * 64) / df['bytes_read']
df['wa'] = (df['wpq_inserts'] * 64) / df['bytes_written']

# Convert from bytes to Mebibyte
df['bytes_read'] = df['bytes_read'] / (1024 * 1024)
df['bytes_written'] = df['bytes_written'] / (1024 * 1024)

# Average latency of data read request
# df['avg_latency_pmem_read'] = df['rpq_occupancy'] / df['rpq_inserts']



# Smooth the data using a 5-point moving average
window_size = int(len(df) * 0.005) if int(len(df) * 0.005) > 0 else 5
df['smoothed_reads'] = df['num_reads'].rolling(window_size, center=True).mean()
df['smoothed_writes'] = df['num_writes'].rolling(window_size, center=True).mean()
df['smoothed_flushes'] = df['num_flushes'].rolling(window_size, center=True).mean()

df['smoothed_bytes_read'] = df['bytes_read'].rolling(window_size, center=True).mean()
df['smoothed_bytes_written'] = df['bytes_written'].rolling(window_size, center=True).mean()

df['smoothed_avg_latency_inst_write'] = df['avg_latency_inst_write'].rolling(window_size, center=True).mean()
df['smoothed_avg_latency_inst_read'] = df['avg_latency_inst_read'].rolling(window_size, center=True).mean()

df['smoothed_ra'] = df['ra'].rolling(window_size, center=True).mean()
df['smoothed_wa'] = df['wa'].rolling(window_size, center=True).mean()



# Print the contents of the DataFrame
print(df)

fig, (ax1, ax2, ax3, ax4, ax5) = plt.subplots(nrows=5, ncols=1, figsize=(10, 8))

# Plot the number of reads and writes on the top subplot
ax1.plot(df['timestamp_sec'], df['smoothed_reads'], label='Number of Reads')
ax1.plot(df['timestamp_sec'], df['smoothed_writes'], label='Number of Writes')
ax1.plot(df['timestamp_sec'], df['smoothed_flushes'], label='Number of Flushes')
ax1.set_xlabel('Time (s)')
ax1.set_ylabel('Number of Operations')
ax1.legend()

# Plot the average latency per write operation on the middle subplot
ax2.plot(df['timestamp_sec'], df['avg_latency_inst_write'], label='Avg. Latency per Write')
ax2.plot(df['timestamp_sec'], df['avg_latency_inst_write'], label='Avg. Latency per Read')
ax2.set_xlabel('Time (s)')
ax2.set_ylabel('Instruction Latency (cycles)')
ax2.legend()

# Plot the bytes read and written on the bottom subplot
ax3.plot(df['timestamp_sec'], df['smoothed_bytes_read'].cumsum(), label='Bytes Read')
ax3.plot(df['timestamp_sec'], df['smoothed_bytes_written'].cumsum(), label='Bytes Written')
ax3.set_xlabel('Time (s)')
ax3.set_ylabel('Mebibyte (MiB)')
ax3.legend()

ax4.plot(df['timestamp_sec'], df['wa'], label='Device Read Amplification')
ax4.plot(df['timestamp_sec'], df['ra'], label='Device Write Amplification')
ax4.set_xlabel('Time (s)')
ax4.set_ylabel('Factor')
ax4.legend()


ax5.plot(df['timestamp_sec'], df['avg_latency_dev_read'], label='Device Read Latency')
ax5.plot(df['timestamp_sec'], df['avg_latency_dev_write'], label='Device Write Latency')
ax5.set_xlabel('Time (s)')
ax5.set_ylabel('Latency (ns)')
ax5.legend()



plt.tight_layout()

# Display the plot
plt.show()