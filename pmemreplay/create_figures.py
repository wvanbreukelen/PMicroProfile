import sys
import os
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

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

df['avg_latency_write'] = df['write_cycles'] / df['num_writes']
df['avg_latency_read'] = df['read_cycles'] / df['num_reads']

# Convert from bytes to Mebibyte
df['bytes_read'] = df['bytes_read'] / (1024 * 1024)
df['bytes_written'] = df['bytes_written'] / (1024 * 1024)

# Smooth the data using a 5-point moving average
window_size = int(len(df) * 0.005) if int(len(df) * 0.01) > 0 else 5
df['smoothed_reads'] = df['num_reads'].rolling(window_size, center=True).mean()
df['smoothed_writes'] = df['num_writes'].rolling(window_size, center=True).mean()
df['smoothed_flushes'] = df['num_flushes'].rolling(window_size, center=True).mean()

df['smoothed_bytes_read'] = df['bytes_read'].rolling(window_size, center=True).mean()
df['smoothed_bytes_written'] = df['bytes_written'].rolling(window_size, center=True).mean()

df['smoothed_avg_latency_write'] = df['avg_latency_write'].rolling(window_size, center=True).mean()
df['smoothed_avg_latency_read'] = df['avg_latency_read'].rolling(window_size, center=True).mean()



# threshold = np.mean(df['avg_latency_write']) + 2 * np.std(df['avg_latency_write'])
# df.loc[df['avg_latency_write'] > threshold, 'avg_latency_write'] = np.nan

# threshold = np.mean(df['avg_latency_read']) + 2 * np.std(df['avg_latency_read'])
# df.loc[df['avg_latency_read'] > threshold, 'avg_latency_read'] = np.nan

# Print the contents of the DataFrame
print(df)

fig, (ax1, ax2, ax3) = plt.subplots(nrows=3, ncols=1, figsize=(10, 8))

# Plot the number of reads and writes on the top subplot
ax1.plot(df['timestamp_sec'], df['smoothed_reads'], label='Number of Reads')
ax1.plot(df['timestamp_sec'], df['smoothed_writes'], label='Number of Writes')
ax1.plot(df['timestamp_sec'], df['smoothed_flushes'], label='Number of Flushes')
ax1.set_xlabel('Time (s)')
ax1.set_ylabel('Number of Operations')
ax1.legend()

# Plot the average latency per write operation on the middle subplot
ax2.plot(df['timestamp_sec'], df['smoothed_avg_latency_write'], label='Avg. Latency per Write')
ax2.plot(df['timestamp_sec'], df['smoothed_avg_latency_read'], label='Avg. Latency per Read')
ax2.set_xlabel('Time (s)')
ax2.set_ylabel('Instruction Latency (cycles)')
ax2.legend()

# Plot the bytes read and written on the bottom subplot
ax3.plot(df['timestamp_sec'], df['smoothed_bytes_read'].cumsum(), label='Bytes Read')
ax3.plot(df['timestamp_sec'], df['smoothed_bytes_written'].cumsum(), label='Bytes Written')
ax3.set_xlabel('Time (s)')
ax3.set_ylabel('Mebibyte (MiB)')
ax3.legend()

plt.tight_layout()

# Display the plot
plt.show()