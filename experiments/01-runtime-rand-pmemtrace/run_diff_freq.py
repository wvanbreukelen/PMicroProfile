import subprocess
import statistics
import re
import os
import matplotlib.pyplot as plt

# Define the file sizes to run the commands with
frequencies = [20, 40, 60, 120]
duty_cycles = [0.5, 0.7, 0.9, 0.95]
num_runs = 2

def run_command_with_pmemtrace(freq, duty_cycle):
    command = f"sudo pmemtrace randwrite sudo bash -c \"time head -c 16M </dev/urandom >/mnt/pmem_emul/rand_file.txt\" --sample-rate {freq} --duty-cycle {duty_cycle}"
    output = subprocess.check_output(command, shell=True, stderr=subprocess.STDOUT, text=True)
    
    real_time_pattern = r"real\s+(\d+)m([\d\.,]+)s[\r\n]+"
    real_time_match = re.search(real_time_pattern, output)

    if not real_time_match:
        print("Error parsing time!")

    minutes = int(real_time_match.group(1))
    seconds = float(real_time_match.group(2).replace(',', '.'))
    real_time = minutes * 60 + seconds

    return real_time


def main():
    # Run the first command with pmemtrace
    print("Running command with pmemtrace...")
    print("-----------------------------")
    freq_values = []
    accuracy_values = []

    fig, axes = plt.subplots(nrows=2, ncols=2)

    i = 0

    for duty_cycle in duty_cycles:
        for freq in frequencies:
            times = [run_command_with_pmemtrace(freq, duty_cycle) for i in range(num_runs)]
            avg_time = statistics.mean(times)
            std_dev = statistics.stdev(times)
            filesize_real = os.path.getsize("/tmp/randwrite.temp")

            total_write_bytes_traced = 0

            with open('/tmp/randwrite.temp') as f:
                all_lines = f.read()

                matches = re.findall(r'W (\d+)', all_lines)
                for match in matches:
                    total_write_bytes_traced += int(match)
                
            accuracy = (total_write_bytes_traced * 100) / (16 * 1024 * 1024)
            
            freq_values.append(freq)
            accuracy_values.append(accuracy)
            
            print(f"D: {duty_cycle * 100}% HZ: {freq:<8} {avg_time:.3f} ({std_dev:.3f} std. dev.) write bytes traced: {total_write_bytes_traced} ({accuracy:.3f}% accuracy)")
        
        # Plot the line graph
        axes.flatten()[i].plot(freq_values, accuracy_values, marker='o')
        axes.flatten()[i].set_title(f"{duty_cycle * 100}% duty cycle", fontsize=8)
        
        i += 1

    plt.setp(axes[-1, :], xlabel='Frequency (Hz)')
    plt.setp(axes[:, 0], ylabel='Accuracy (%)')

    # fig.grid(True)
    plt.tight_layout()
    plt.savefig("fs-diff-freq.pdf", bbox_inches='tight')
    plt.show()


if __name__ == "__main__":
    main()
