import sys
import os
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from scipy import stats
import matplotlib.ticker as mticker
import seaborn


def NormalizeData(data):
    return (data - np.min(data)) / (np.max(data) - np.min(data))

# Get the script name
script_name = os.path.basename(__file__)
plots_cat = "all"
cur_filesystem = "SplitFS"
title_font_size = 14
info_font_size = 9
axes_font_size = 14
legend_font_size = 11

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
df['total_addr_distance_normalized'] = df['total_addr_distance'] / (df['total_ops'])
df['num_barriers'] = df['num_barriers'].astype(int)
# Average latency of data read request
# df['avg_latency_pmem_read'] = df['rpq_occupancy'] / df['rpq_inserts']

cols = ['wa']

# multiplier = 2.0  # Adjust this value to remove fewer or more outliers

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

stat_margin = 1.55

if plots_cat == "all":
    fig, (ax_rw, ax_bw, ax_lat_read, ax_lat_write, ax_lat_pebs, ax_isad, ax_amp, ax_totals, ax_bar) = plt.subplots(nrows=9, ncols=1, figsize=(12, 10), sharex=True)
    fig.text(0.5, 0.01, 'Time (s)', ha='center', size=axes_font_size)
elif plots_cat == "workload_rw":
    fig, (ax_rw) = plt.subplots(nrows=1, ncols=1, figsize=(8.5, 2.5))
    fig.text(0.5, 0.02, 'Time (s)', ha='center', size=axes_font_size)
elif plots_cat == "workload":
    fig, (ax_rw, ax_bar, ax_isad) = plt.subplots(nrows=3, ncols=1, figsize=(6, 4), sharex=True)
    fig.text(0.5, 0.01, 'Time (s)', ha='center')
elif plots_cat == "perf":
    fig, (ax_bw, ax_amp, ax_lat_read, ax_lat_write) = plt.subplots(nrows=4, ncols=1, figsize=(6, 4), sharex=True)
    fig.text(0.5, 0.01, 'Time (s)', ha='center')
elif plots_cat == "amp":
    fig, (ax_bw, ax_amp) = plt.subplots(nrows=2, ncols=1, figsize=(6, 2), sharex=True)
    fig.text(0.5, 0.02, 'Time (s)', ha='center')
elif plots_cat == "latency":
    fig, (ax_lat_read) = plt.subplots(nrows=1, ncols=1, figsize=(3, 2), sharex=False)
    # fig.text(0.5, 0.01, 'Time (s)', ha='center')
elif plots_cat == "dram":
    fig, (ax_dload, ax_l3_miss_dram, ax_l3_miss_pmm) = plt.subplots(nrows=3, ncols=1, figsize=(5, 4), sharex=True)
    fig.text(0.5, 0.01, 'Time (s)', ha='center')
elif plots_cat == "inst":
    fig, (ax_inst) = plt.subplots(nrows=1, ncols=1, figsize=(5,2.5))

# fig.suptitle("Ext4-DAX, repeated 10 times", size=12)



mask = df.columns.str.contains('retired_mov*')

num_writes_nt = df.loc[:,mask].sum(axis=1).sum()
percentage_not_nt = ((df['num_writes'].sum() - num_writes_nt) / df['num_writes'].sum()) * 100.0
percentage_nt = 100.0 - percentage_not_nt


if plots_cat == "all" or plots_cat == "workload" or plots_cat == "workload_rw":
    # Plot the number of reads and writes on the top subplot
    ax_rw.plot(df['timestamp_sec'], df['num_reads'], label='Number of Reads')
    ax_rw.plot(df['timestamp_sec'], df['num_writes'], label='Number of Writes')
    ax_rw.plot(df['timestamp_sec'], df['num_flushes'], label='Number of Flushes')
    # ax1.plot(df['timestamp_sec'], df['num_barriers'], label='Number of Barriers')

    #  Writes: {:.2f} Flushes: {:.2f}
    ax_rw.text(1.0, stat_margin, 'Reads: {:.2f}% Writes: {:.2f}%\n({:.2f}% is NT) Flushes: {:.2f}%'.format((df['num_reads'].sum() / df['total_ops'].sum()) * 100.0, (df['num_writes'].sum() / df['total_ops'].sum()) * 100.0, percentage_nt, (df['num_flushes'].sum() / df['total_ops'].sum()) * 100.0),
        horizontalalignment='right',
        verticalalignment='top', size=info_font_size, transform=ax_rw.transAxes)
    
    ax_rw.set_ylabel('Count (log)', fontsize=axes_font_size)
    ax_rw.set_yscale('log')
    ax_rw.set_title("Number of Retired Instructions {}".format(cur_filesystem), size=title_font_size)
    box = ax_rw.get_position()
    ax_rw.set_position([box.x0, box.y0 + box.height * 0.1,
                 box.width, box.height * 0.9])

    ax_rw.legend(loc='upper center', bbox_to_anchor=(0.5, -0.30),
          fancybox=True, ncol=3, prop={'size': legend_font_size})

if plots_cat == "all" or plots_cat == "workload":
    ax_bar.plot(df['timestamp_sec'], df['num_barriers'])
    ax_bar.set_ylabel('Count', fontsize=axes_font_size)
    ax_bar.set_title("Number of Memory Barriers", size=title_font_size)
    # ax7.plot(df['timestamp_sec'], (df['bytes_written'].cumsum() / (1024 * 1024)), label='Data Written')


if plots_cat == "all" or plots_cat == "perf" or plots_cat == "amp":
    ax_bw.plot(df['timestamp_sec'], df['pmem_read_bw'], label='Read')
    ax_bw.plot(df['timestamp_sec'], df['pmem_write_bw'], label='Write')
    ax_bw.text(1.0, stat_margin, 'Avg. read: {:.3f} GB/s\nAvg. write: {:.3f} GB/s'.format(df['pmem_read_bw'].mean(), df['pmem_write_bw'].mean()),
        horizontalalignment='right',
        verticalalignment='top', size=info_font_size, transform=ax_bw.transAxes)
    ax_bw.set_ylabel('GB/s', fontsize=axes_font_size)
    ax_bw.set_title("Bandwidth", size=title_font_size)
    box = ax_bw.get_position()
    ax_bw.set_position([box.x0, box.y0 + box.height * 0.3,
                 box.width, box.height * 0.7])

    ax_bw.legend(loc='upper center', bbox_to_anchor=(0.5, 0.0),
          fancybox=True, ncol=3, prop={'size': 8})
    

if plots_cat == "all" or plots_cat == "latency" or plots_cat == "perf":
    # Plot the average latency per write operation on the middle subplot
    ax_lat_read.plot(df['timestamp_sec'], df['avg_latency_inst_read'], marker='o', markersize=2, linestyle='None')
    ax_lat_read.set_ylabel('ns', fontsize=axes_font_size)
    # ax_lat_read.set_xlabel('Time (sec)', fontsize=axes_font_size)
    ax_lat_read.set_title("Instruction Read Latency", size=title_font_size)
    ax_lat_read.text(1.0, 1.1, 'Avg. {:.2f} ns ($\sigma$={:.2f})'.format(df['avg_latency_inst_read'].mean(), df['avg_latency_inst_read'].std()),
        horizontalalignment='right',
        verticalalignment='top', size=info_font_size, transform=ax_lat_read.transAxes)
    # ax_lat_read.legend()

    ax_lat_write.plot(df['timestamp_sec'], df['avg_latency_inst_write'], marker='o', markersize=2, c='darkorange', linestyle='None')
    ax_lat_write.set_ylabel('ns', fontsize=axes_font_size)
    ax_lat_write.set_title("Instruction Write Latency", size=title_font_size)
    ax_lat_write.text(1.0, 1.1, 'Avg. {:.2f} ns ($\sigma$={:.2f})'.format(df['avg_latency_inst_write'].mean(), df['avg_latency_inst_write'].std()),
        horizontalalignment='right',
        verticalalignment='top', size=info_font_size, transform=ax_lat_write.transAxes)

# ax4.plot(df['timestamp_sec'], df['avg_latency_dev_read_dram'], label='DRAM Read Latency')
if plots_cat == "all":
    ax_lat_pebs.plot(df['timestamp_sec'], df['avg_latency_dev_read'], label='Read')
    # ax_lat_pebs.plot(df['timestamp_sec'], df['avg_latency_dev_write'], label='Write')
    ax_lat_pebs.set_ylabel('Latency (ns)')
    ax_lat_pebs.set_title("Instruction Latency: measured using PEBS PMEM counters", size=title_font_size)
    ax_lat_pebs.legend()

if plots_cat == "all" or plots_cat == "workload":
    ax_isad.plot(df['timestamp_sec'], df['total_addr_distance_normalized'], label='Address Distance')

    ax_isad.set_title("Inner-Sample Address Distance (ISAD)", size=title_font_size)
    # ax5.set_ylim([0.0, 0.5])
    ax_isad.set_ylabel('ISAD')

    ax_isad.text(1.0, 1.25, 'Avg. {:.4f} ($\sigma$ {:.4f})'.format(df['total_addr_distance_normalized'].mean(), df['total_addr_distance_normalized'].std()),
        horizontalalignment='right',
        verticalalignment='top', size=info_font_size, transform=ax_isad.transAxes)


if plots_cat == "all" or plots_cat == "perf" or plots_cat == "amp":
    ax_amp.plot(df['timestamp_sec'], df['ra'], label='RA', marker='o', markersize=2, linestyle='None')
    ax_amp.plot(df['timestamp_sec'], df['wa'], label='WA', marker='o', markersize=2, linestyle='None')
    # ax_amp.set_xlabel('Time (s)')
    ax_amp.set_ylabel('Factor', fontsize=axes_font_size)
    ax_amp.set_title("Device Read/Write Amplification", size=title_font_size)
    ax_amp.text(1.0, stat_margin, 'Avg. RA: {:.2f}\nAvg. WA: {:.2f}'.format(df['ra'].mean(), df['wa'].mean()),
        horizontalalignment='right', 
        verticalalignment='top', size=info_font_size, transform=ax_amp.transAxes)
    box = ax_amp.get_position()
    ax_amp.set_position([box.x0, box.y0 + box.height * 0.1,
                 box.width, box.height * 0.9])

    if plots_cat != "amp":
        ax_amp.legend(loc='upper center', bbox_to_anchor=(0.5, 0.0),
            fancybox=True, ncol=3, prop={'size': 8})


if plots_cat == "all":
    ax_totals.plot(df['timestamp_sec'], (df['total_read_write'] / (1024 * 1024)), label='Total Read/Write')
    # ax7.plot(df['timestamp_sec'], (df['bytes_written'].cumsum() / (1024 * 1024)), label='Data Written')
    ax_totals.legend()

if plots_cat == "dram":
    ax_dload.plot(df['timestamp_sec'], df['any_scoop_pmm'])
    ax_dload.set_ylabel('Count')
    ax_dload.set_title("PMEM direct loads")

    ax_l3_miss_pmm.plot(df['timestamp_sec'], df['l3_misses_local_pmm'])
    ax_l3_miss_pmm.set_ylabel('Count')
    ax_l3_miss_pmm.set_title("PMEM L3 Cache Misses")

    ax_l3_miss_dram.plot(df['timestamp_sec'], df['any_scoop_l3_miss_dram'])
    ax_l3_miss_dram.set_ylabel('Count')
    ax_l3_miss_dram.set_title("DRAM L3 Cache Misses")


if plots_cat == "inst":
    # mask = df.columns.str.contains('retired_mov*')

    # column_names = df.loc[:,mask]
    # data_dict = {}

    # for column_name in column_names:
    #     print("{} -> {}".format(column_name, df[column_name]))
    #     data_dict[column_name] = df[column_name]

    # print(df.loc[(df['retired_movntq'] > 0) & (df['retired_movntps'] == 0)]['wa'].mean())
    # print(df.loc[(df['retired_movntq'] > 0) & (df['retired_movntps'] == 0)]['ra'].mean())

    # print(df.loc[(df['retired_movntq'] == 0) & (df['retired_movntps'] > 0)]['wa'].mean())
    # print(df.loc[(df['retired_movntq'] == 0) & (df['retired_movntps'] > 0)]['ra'].mean())

    df['timestamp'] = pd.to_datetime(df['timestamp'], unit='ns')
    df = df.set_index('timestamp')

    df2 = df.loc[(df['retired_movntq'] > 0) & (df['retired_movntps'] == 0)].groupby(pd.Grouper(freq='500ms', label='right'))['wa'].mean()
    df3 = df.loc[(df['retired_movntq'] == 0) & (df['retired_movntps'] > 0)].groupby(pd.Grouper(freq='500ms', label='right'))['wa'].mean()


    if df3.size:
        df3 = df3.interpolate(limit=1)
        ax_inst.plot(df3.index.second, df3, label="movntps")
        # seaborn.boxplot(x = df3.index.second, 
        #         y = df3, 
        #         ax = ax_inst)
        # ax_inst.set_title("movntps")
        # ax_inst.set_ylabel('Write Amplification')
        # ax_inst.set_xlabel("Time (sec)")


        # ax_inst.ticklabel_format(style='plain', axis='y',useOffset=False)

    if df2.size:
        ax_inst.plot(df2.index.second, df2, label="movntq")
        # seaborn.boxplot(x = df2.index.second, 
        #         y = df2, 
        #         ax = ax_inst)
        ax_inst.set_title('Write Amplification: movntq and movntps', size=title_font_size)
        ax_inst.set_ylabel('WA (lower is better)', size=axes_font_size)
        ax_inst.set_xlabel("Time (sec)", size=axes_font_size)


        # ax_inst.ticklabel_format(style='plain', axis='y',useOffset=False)
   



    ax_inst.legend(prop={'size': legend_font_size})
    plt.ylim(0, 3)
    #ax_inst.set_yscale('log')
    #ax_inst.boxplot(data)

plt.xticks(fontsize=axes_font_size)
plt.yticks(fontsize=axes_font_size)

plt.tight_layout()

plt.savefig("test.png", format="png", dpi=400)

# Display the plot
plt.show()




# plt.tight_layout()

# # Display the plot
# plt.show()