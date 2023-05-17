## Analysize microarchitectural performance: Ext4-DAX <-> SplitFS

## Prerequisites
- Working VM installation to extract traces, which included installing all dependencies and pmemtrace itself using install script (see Thesis Artifact for installation steps);
- Working Linux 5.* machine, with real Persistent Memory. Kernel must be build with DAX support (Ubuntu includes DAX support by default).
- Python 3 (including matplotlib package);

## Reproducing trace files (done inside VM)
1. Ext4-DAX: execute the `run_ext4dax.sh` script. This script generates a `.parquet` file that can be replayed in `pmemanalyze`
2. SplitFS: execute the `run_splitfs.sh` script. Again, This script generates a `.parquet` file that can be replayed in `pmemanalyze`

## Perforning performance analyze (real machine)
1. Copy the trace files from the VM to the machine that has real PMEM using a tool like `scp`.
2. Ensure you have installed `pmemreplay` by executing the `../../install-real-machine.sh` script.
3. Configure PMEM as a devdax device: `../../mount-devdax.sh`
4. Replay both compressed trace files using `pmemanalyze`: `sudo pmemanalyze --device /dev/dax0.0 **TRACE_FILE**.parquet`.
5. Plot results: `sudo pmemanalyze --plot`.
