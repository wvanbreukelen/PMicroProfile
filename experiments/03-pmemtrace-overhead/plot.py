import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

def plot_column(groups, column_name, label):
    means = groups.mean()
    stds = groups.std()['overhead_percent_overhead_single_stepping'].values
    plt.errorbar(data['working_set_size'].unique(), means[column_name].values, yerr=stds, label=label)

# load data from CSV
data = pd.read_csv('results.csv')


# group data by overhead type
groups = data.groupby('working_set_size')



plot_column(groups, 'overhead_percent_overhead_single_stepping', 'CPU Single-stepping')
plot_column(groups, 'overhead_percent_overhead_tlb_flushing', 'TLB Flushing')
plot_column(groups, 'overhead_percent_overhead_syscalls', 'System Calls')
plot_column(groups, 'overhead_percent_overhead_io', 'I/O')
plot_column(groups, 'overhead_percent_overhead_locking', 'Locking Primitives')
plot_column(groups, 'overhead_percent_misc', 'Miscellaneous')



# set plot title and axis labels
plt.title('pmemtrace Runtime Overhead')
ax = plt.gca()
ax.set_ylim([0, 100])
plt.xlabel('Working Set Size (MiB)')
plt.ylabel('Percentage of Run Time (%)')

# show legend
plt.legend(prop={'size': 8})

plt.tight_layout()

# show plot
plt.show()
