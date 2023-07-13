import sys
import pandas as pd
import matplotlib.pyplot as plt

if len(sys.argv) < 2:
    print("Please provide .csv file as CLI argument!")
    exit(1)

filename = str(sys.argv[1])

# Load the CSV file into a pandas dataframe
df = pd.read_csv(filename, index_col=0)

# Define a dictionary to map the column names to their prettified versions
name_map = {
    'percent_device_io': '% Device I/O',
    'percent_indexing_kernel': '% Indexing Kernel',
    'percent_indexing_user': '% Indexing User',
    'percent_page_fault': '% Page Fault',
    'percent_idle': '% Idle',
    'percent_interrupts': '% Interrupts',
    'percent_other': '% Other'
}

# Rename the columns in the dataframe
df = df.rename(columns=name_map)

# Extract the columns we want to plot
cols_to_plot = list(name_map.values())

# Create a stacked bar chart

fig, axs = plt.subplots(1, 2, figsize=(5.8, 3.8))

ax = df['runtime'].plot(kind='bar', width=0.20, ax=axs[0])

# ax.set_title('PMEM FS Run time: Filebench')
ax.set_ylabel('Run time (seconds)')
ax.set_xlabel('')
ax.figure.autofmt_xdate(rotation=45)



ax = df[cols_to_plot].plot(kind='bar', stacked=True, width=0.30, ax=axs[1]) # Use axs[0] to plot in the left subplot


# Set the title and labels for the chart
fig.suptitle('PMEM FS Performance Breakdown: Filebench Varmail')
ax.set_xlabel('')
ax.set_ylabel('Percentage')

# Format the legend labels
handles, labels = ax.get_legend_handles_labels()
new_labels = [label.replace('%', r'$\%$') for label in labels] # add LaTeX formatting for %
ax.legend(handles, new_labels, loc='lower right', fontsize=7)
ax.figure.autofmt_xdate(rotation=45)

# fig.text(0.5, 0.01, 'File System', ha='center')


#ax.autofmt_xdate(rotation=45)


# plt.tight_layout()

# # Show the plot
# plt.show()

# plt.clf()



plt.tight_layout()

plt.savefig("fs-performance-breakdown.pdf", bbox_inches='tight')
plt.savefig("pmemtrace-overhead.png", bbox_inches='tight', dpi=800)
plt.show()