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
plots_cat = "all"
title_font_size = 10

# Check if a filename was provided as a command-line argument
if len(sys.argv) < 2:
    print(f'Error: Please provide a filename as a command-line argument.\nUsage: python {script_name} filename.csv')
    sys.exit(1)

# Get the filename from the command-line argument
filename = sys.argv[1]

if len(sys.argv) > 2:
    plots_cat = sys.argv[2]

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

df['avg_latency_inst_write'] = df['write_cycles'] / df['write_cycles_samples']
df['avg_latency_inst_read'] = df['read_cycles'] / df['read_cycles_samples']

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

df['total_ops'] = df['num_reads'] + df['num_writes'] + df['num_flushes']
df['total_addr_distance_normalized'] = df['total_addr_distance'] / (df['total_ops'] + df['num_barriers'])

# Average latency of data read request
# df['avg_latency_pmem_read'] = df['rpq_occupancy'] / df['rpq_inserts']

cols = ['pmem_write_bw']

# multiplier = 16.0  # Adjust this value to remove fewer or more outliers

# Q1 = df.quantile(0.25)
# Q3 = df.quantile(0.75)
# IQR = Q3 - Q1

# df = df[~((df[cols] < (Q1 - multiplier * IQR)) | (df[cols] > (Q3 + multiplier * IQR))).any(axis=1)]




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

df.replace([np.inf, -np.inf], np.nan, inplace=True)
df.dropna(subset=["wa", "ra"], how="all", inplace=True)


# Print the contents of the DataFrame
with pd.option_context('display.max_rows', None, 'display.max_columns', None):  # more options can be specified also
    print(df.loc[[6]])


if plots_cat == "all":
    fig, (ax_rw, ax_bw, ax_lat_read, ax_lat_write, ax_lat_pebs, ax_isad, ax_amp, ax_totals, ax_bar) = plt.subplots(nrows=9, ncols=1, figsize=(12, 10))
elif plots_cat == "workload":
    fig, (ax_rw, ax_bar, ax_isad) = plt.subplots(nrows=3, ncols=1, figsize=(6, 5))
elif plots_cat == "perf":
    fig, (ax_rw, ax_bw, ax_amp) = plt.subplots(nrows=3, ncols=1, figsize=(6, 5))
elif plots_cat == "latency":
    fig, (ax_rw, ax_lat_read, ax_lat_write) = plt.subplots(nrows=3, ncols=1, figsize=(6, 5))
elif plots_cat == "dram":
    fig, (ax_dload, ax_l3_miss_dram, ax_l3_miss_pmm) = plt.subplots(nrows=3, ncols=1, figsize=(5, 4))

fig.suptitle("Filebench Varmail Ext4-DAX, repeated 10 times", size=12)

if plots_cat == "all" or plots_cat == "workload" or plots_cat == "perf" or plots_cat == "latency":
    # Plot the number of reads and writes on the top subplot
    ax_rw.plot(df['timestamp_sec'], df['num_reads'], label='Number of Reads')
    ax_rw.plot(df['timestamp_sec'], df['num_writes'], label='Number of Writes')
    ax_rw.plot(df['timestamp_sec'], df['num_flushes'], label='Number of Flushes')
    # ax1.plot(df['timestamp_sec'], df['num_barriers'], label='Number of Barriers')

    #  Writes: {:.2f} Flushes: {:.2f}
    ax_rw.text(1.0, 1.33, 'Reads: {:.2f} % Writes: {:.2f} %\nFlushes: {:.2f} %'.format((df['num_reads'].sum() / df['total_ops'].sum()) * 100.0, (df['num_writes'].sum() / df['total_ops'].sum()) * 100.0, (df['num_flushes'].sum() / df['total_ops'].sum()) * 100.0),
        horizontalalignment='right',
        verticalalignment='top', size=6, transform=ax_rw.transAxes)
    
    ax_rw.set_ylabel('# Operations (log)')
    ax_rw.set_yscale('log')
    ax_rw.set_title("Number of Retired Instructions", size=title_font_size)
    box = ax_rw.get_position()
    ax_rw.set_position([box.x0, box.y0 + box.height * 0.1,
                 box.width, box.height * 0.9])

    ax_rw.legend(loc='upper center', bbox_to_anchor=(0.5, -0.30),
          fancybox=True, ncol=3, prop={'size': 8})

if plots_cat == "workload":
    ax_bar.plot(df['timestamp_sec'], df['num_barriers'] , label='Total Number of Barriers')
    ax_bar.set_xlabel('Time (s)')
    # ax7.plot(df['timestamp_sec'], (df['bytes_written'].cumsum() / (1024 * 1024)), label='Data Written')
    ax_bar.legend(prop={'size': 8})

if plots_cat == "all" or plots_cat == "perf":
    ax_bw.plot(df['timestamp_sec'], df['pmem_read_bw'], label='Read')
    ax_bw.plot(df['timestamp_sec'], df['pmem_write_bw'], label='Write')
    ax_bw.text(1.0, 1.33, 'Avg. read: {:.3f} GB/s\nAvg. write: {:.3f} GB/s'.format(df['pmem_read_bw'].mean(), df['pmem_write_bw'].mean()),
        horizontalalignment='right',
        verticalalignment='top', size=6, transform=ax_bw.transAxes)
    ax_bw.set_ylabel('GB/s')
    ax_bw.set_title("PMEM Bandwidth", size=title_font_size)
    ax_bw.legend()

if plots_cat == "all" or plots_cat == "latency":
    # Plot the average latency per write operation on the middle subplot
    ax_lat_read.plot(df['timestamp_sec'], df['avg_latency_inst_read'], marker='o', markersize=2, linestyle='None')
    # ax_lat.plot(df['timestamp_sec'], df['avg_latency_inst_write'], label='Write')
    ax_lat_read.set_xlabel('Time (s)')
    ax_lat_read.set_ylabel('Nanoseconds')
    ax_lat_read.set_title("Instruction Read Latency", size=title_font_size)
    ax_lat_read.text(1.0, 1.25, 'Avg. {:.2f} ns (std: {:.2f})'.format(df['avg_latency_inst_read'].mean(), df['avg_latency_inst_read'].std()),
        horizontalalignment='right',
        verticalalignment='top', size=6, transform=ax_lat_read.transAxes)
    # ax_lat_read.legend()

    ax_lat_write.plot(df['timestamp_sec'], df['avg_latency_inst_write'], marker='o', markersize=2, c='darkorange', linestyle='None')
    # ax_lat.plot(df['timestamp_sec'], df['avg_latency_inst_write'], label='Write')
    ax_lat_write.set_xlabel('Time (s)')
    ax_lat_write.set_ylabel('Nanoseconds')
    ax_lat_write.set_title("Instruction Write Latency", size=title_font_size)
    ax_lat_write.text(1.0, 1.25, 'Avg. {:.2f} ns (std: {:.2f})'.format(df['avg_latency_inst_write'].mean(), df['avg_latency_inst_write'].std()),
        horizontalalignment='right',
        verticalalignment='top', size=6, transform=ax_lat_write.transAxes)
    # ax_lat_write.legend()

# ax4.plot(df['timestamp_sec'], df['avg_latency_dev_read_dram'], label='DRAM Read Latency')
if plots_cat == "all":
    ax_lat_pebs.plot(df['timestamp_sec'], df['avg_latency_dev_read'], label='Read')
    # ax_lat_pebs.plot(df['timestamp_sec'], df['avg_latency_dev_write'], label='Write')
    ax_lat_pebs.set_xlabel('Time (s)')
    ax_lat_pebs.set_ylabel('Latency (ns)')
    ax_lat_pebs.set_title("Instruction Latency: measured using PEBS PMEM counters", size=title_font_size)
    ax_lat_pebs.legend()

if plots_cat == "all" or plots_cat == "workload":
    ax_isad.plot(df['timestamp_sec'], df['total_addr_distance_normalized'], label='Address Distance')

    ax_isad.set_title("Inner-Sample Address Distance (ISAD)", size=title_font_size)
    # ax5.set_ylim([0.0, 0.5])
    ax_isad.set_ylabel('ISAD')

if plots_cat == "all" or plots_cat == "perf":
    ax_amp.plot(df['timestamp_sec'], df['ra'], label='RA', marker='o', markersize=2, linestyle='None')
    ax_amp.plot(df['timestamp_sec'], df['wa'], label='WA', marker='o', markersize=2, linestyle='None')
    ax_amp.set_xlabel('Time (s)')
    ax_amp.set_ylabel('Factor')
    ax_amp.set_title("Device Read/Write Amplification", size=title_font_size)
    ax_amp.text(1.0, 1.33, 'Avg. RA: {:.2f}\nAvg. WA: {:.2f}'.format(df['ra'].mean(), df['wa'].mean()),
        horizontalalignment='right',
        verticalalignment='top', size=6, transform=ax_amp.transAxes)
    ax_amp.legend()

if plots_cat == "all":
    ax_totals.plot(df['timestamp_sec'], (df['total_read_write'] / (1024 * 1024)), label='Total Read/Write')
    # ax7.plot(df['timestamp_sec'], (df['bytes_written'].cumsum() / (1024 * 1024)), label='Data Written')
    ax_totals.legend()

if plots_cat == "all":
    ax_bar.plot(df['timestamp_sec'], df['num_barriers'] , label='Total Number of Barriers')
    # ax7.plot(df['timestamp_sec'], (df['bytes_written'].cumsum() / (1024 * 1024)), label='Data Written')
    ax_bar.legend()

if plots_cat == "dram":
    ax_dload.plot(df['timestamp_sec'], df['any_scoop_pmm'])
    ax_dload.set_xlabel('Time (s)')
    ax_dload.set_title("PMEM direct loads")

    ax_l3_miss_pmm.plot(df['timestamp_sec'], df['l3_misses_local_pmm'])
    ax_l3_miss_pmm.set_ylabel('Count')
    ax_l3_miss_pmm.set_xlabel('Time (s)')
    ax_l3_miss_pmm.set_title("PMEM L3 Cache Misses")

    ax_l3_miss_dram.plot(df['timestamp_sec'], df['any_scoop_l3_miss_dram'])
    # ax7.plot(df['timestamp_sec'], df['l3_misses_local_pmm'], label='Cache miss PMEM')
    ax_l3_miss_dram.set_ylabel('Count')
    ax_l3_miss_dram.set_xlabel('Time (s)')
    ax_l3_miss_dram.set_title("DRAM L3 Cache Misses")



plt.tight_layout()

# Display the plot
plt.show()




# plt.tight_layout()

# # Display the plot
# plt.show()