# PMicroProfile

PMicroProfile is a performance profiling framework that can be used to evaluate the performance of Persistent Memory (PMEM) file systems, such as Ext4-DAX and SplitFS. The unique approach of this framework is that it captures low-level PMEM access traces, and replays those traces to identify performance bottlenecks between CPU and PMEM.

Currently, PMicroProfile only supports Intel Optane DC Persistent Memory.

## Design

PMicroProfile consists of two tools that complement each other:

- _pmemtrace_: a tool that captures file system access patterns in the form of machine instructions (e.g. `movnti`)
- _pmemanalyze_: a tool that replays caputured traces and calculates performance metrics (see example below)

## Installation

The installation instructions for both _pmemtrace_ and _pmemanalyze_ tools are provided below.

### _pmemtrace_

The installation process of _pmemtrace_ consists of two steps: establishing a QEMU virtual machine instance with a custom kernel and afterward installing the _pmemtrace_ tracing tool.
Most of the steps involve running automated scripts that pre-configure all required dependencies and infrastructure in such a way that it requires minimal effort.
First, set up a new QEMU VM instance:

```bash
$ git clone https://github.com/wvanbreukelen/PMicroProfile
$ cd PMicroProfile

# Run the automated install script.
$ ./setup-vm.sh
```

You may now proceed to the Ubuntu installation wizard by executing the \path{vm/run_kvm_iso.sh} script. After finishing the Ubuntu installation, perform the following steps:


1. Remove the installation medium by commenting/removing the line `-cdrom "ubuntu.iso"` in _vm/run_kvm_iso.sh_;
2. Open a terminal inside the VM and execute the following command: `$ cat /proc/mounts | grep " / "`;
3. Copy the mount path of the root file system, for example, _/dev/sda5_, and update the kernel _root_ boot parameter in the _vm/run_kvm.sh_ file accordingly;
4. _Optional_: Modify the number of cores, the amount of RAM, and the size of the emulated PMEM device by modifying the _vm/run_kvm.sh_ file accordingly;
5. Now, boot the VM using the custom kernel by executing the script `vm/run_kvm.sh`;
6. Verify that you are running the custom kernel by running `uname -r` within the VM. This command should print `5.4.232`.

Within the VM, spawn a terminal and, again, clone the git repository. Subsequently, run the _pmemtrace_ installation script:

```bash
$ git clone https://github.com/wvanbreukelen/PMicroProfile
$ cd PMicroProfile

# Run the automated pmemtrace install script.
$ ./install-vm.sh
```

The `pmemtrace` executable will be placed inside the _/usr/local/bin/_ folder so that it is contained in the user `$PATH`. Verify this by running `sudo pmemtrace --help`.





### _pmemanalyze_

In order to install _pmemanalyze_, you will need access to a real machine (no VM). A virtual machine is not supported since it does not implement (representative) Intel PMC and PEBS performance counters required for performance evaluation. The system must contain one or more Intel Optane DCPMM DIMM modules. Furthermore, an Intel Cascadelake-Server based CPU is mandatory. 

To install _pmemanalyze_, run the following bash commands:

```bash
$ git clone https://github.com/wvanbreukelen/PMicroProfile
$ cd PMicroProfile

# Run the automated pmemanalyze install script.
$ ./install-real-machine.sh

# Change directory to experiments folder.
cd experiments
```

## Usage

To enable access tracing when executing a CLI command, use the following syntax: `sudo pmemtrace [OPTIONS] experiment_name [COMMAND]`. When executing the command, _pmemtrace_ configures the in-kernel infrastructure to log low-level PMEM accesses and then executes the provided command. In order to decrease tracing overhead, one may use a sampling-based data collection strategy by passing the `--sample-rate` (in hertz) and `--duty-cycle` (in percentage) arguments. To capture all events, the `--disable-sampling` flag must be provided. Example usage:

```bash
# Mount PMEM device as fsdax
$ cd thesis-research
$ sudo ./mount-ext4.dax

# Example of tracing all accesses random file write (i.e. sampling disabled)
# Optionally, one may enable experimental multi-core capturing support by setting the --enable-multicore flag.
$ sudo pmemtrace randwrite-all-exp sudo bash -c "head -c 16M </dev/urandom >/mnt/pmem_emul/rand_file.txt" --disable-sampling
    
# Example of tracing all accesses random file write with sampling (60 hertz, 80% duty cycle):
$ sudo pmemtrace randwrite-sampling-exp sudo bash -c "head -c 16M </dev/urandom >/mnt/pmem_emul/rand_file.txt" --sample-rate 60 --duty-cycle 0.8
```

The compressed trace file is saved as `experiment_name.parquet` in the current working directory. This trace file is now ready to be replayed in _pmemanalyze_. A readable log can also be found in the system `/tmp` directory.

To replay a trace use the following commands:

```bash
sudo ./mount-devdax.sh
sudo pmemanalyze --device /dev/dax0.0 [trace_file].parquet
```

The raw (unprocessed) performance statistics will be stored in a `data.csv` file.

## Plotting
Plotting is done by using the `postprocess.py` Python script and consists of loading a `data.csv` file:

```bash
# Plot workload, i.e. number of reads, writes, and flushes
python3 pmemanalyze/plot/postprocess.py workload data.csv
 # Bandwidth
python3 pmemanalyze/plot/postprocess.py bandwidth data.csv
 # More performance statistics
python3 pmemanalyze/plot/postprocess.py perf data.csv
```

![Example Metric](./plots/experiments/pmemanalyze_varmail_ext4dax_dram.png)


## Reproducing Experiments

All experiments can be found in the `experiments/` folder. Each experiment is assigned a `REPRODUCE.md` readme file with further instructions to reproduce experiment results.
