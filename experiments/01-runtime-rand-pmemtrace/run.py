#!/usr/bin/env python3

import subprocess
import statistics
import re


# Define the file sizes to run the commands with
file_sizes = ["2M", "4M", "8M", "16M"]
num_runs = 5

def run_command_with_pmemtrace(file_size):
    command = f"sudo pmemtrace randread sudo bash -c \"time head -c {file_size} </dev/urandom >/mnt/pmem_emul/rand_file.txt\" --disable-sampling"
    output = subprocess.check_output(command, shell=True, stderr=subprocess.STDOUT, text=True)
    
    real_time_pattern = r"real\s+(\d+)m([\d\.,]+)s[\r\n]+"
    real_time_match = re.search(real_time_pattern, output)

    if not real_time_match:
        print("Error parsing time!")

    minutes = int(real_time_match.group(1))
    seconds = float(real_time_match.group(2).replace(',', '.'))
    real_time = minutes * 60 + seconds

    #print(real_time)

    return real_time

def run_command_without_pmemtrace(file_size):
    command = f"bash -c \"time head -c {file_size} </dev/urandom >/mnt/pmem_emul/rand_file.txt\""
    output = subprocess.check_output(command, shell=True, stderr=subprocess.STDOUT, text=True)
    
    real_time_pattern = r"real\s+(\d+)m([\d\.,]+)s[\r\n]+"
    real_time_match = re.search(real_time_pattern, output)

    if not real_time_match:
        print("Error parsing time!")

    minutes = int(real_time_match.group(1))
    seconds = float(real_time_match.group(2).replace(',', '.'))
    real_time = minutes * 60 + seconds

    #print(real_time)

    return real_time

def main():
    # Run the first command with pmemtrace
    print("Running command with pmemtrace...")
    print("-----------------------------")
    for file_size in file_sizes:
        times = [run_command_with_pmemtrace(file_size) for i in range(num_runs)]
        avg_time = statistics.mean(times)
        std_dev = statistics.stdev(times)
        print(f"{file_size:<8} {avg_time:.3f} ({std_dev:.3f} std. dev.)")

    # Run the second command without pmemtrace
    print("\nRunning command without pmemtrace...")
    print("----------------------------------")
    for file_size in file_sizes:
        times = [run_command_without_pmemtrace(file_size) for i in range(num_runs)]
        avg_time = statistics.mean(times)
        std_dev = statistics.stdev(times)
        print(f"{file_size:<8} {avg_time:.3f} ({std_dev:.3f} std. dev.)")

if __name__ == "__main__":
    main()
