## Analysize microarchitectural performance: Ext4-DAX <-> SplitFS baseline

## Prerequisites
- Working VM installation to extract traces, which included installing all dependencies and pmemtrace itself using install script (see Thesis Artifact for installation steps);
     * (!) Please make sure that you installed the Clang compiling, including the provided pass, by adding the `--build-llvm` flag, i.e.: `../../install-real-machine.sh --build-llvm`.
- Working Linux 5.* machine, with real Persistent Memory. Kernel must be build with DAX support (Ubuntu includes DAX support by default).
- Python 3 (including matplotlib package);

## Reproducing trace files (done inside VM)
1. Ext4-DAX: execute the `run_ext4dax_500M_50_50_rw.sh` script. This script generates a `.parquet` file that can be replayed in `pmemanalyze`
2. SplitFS: execute the `run_splitfs_500M_50_50_rw.sh` script. Again, This script generates a `.parquet` file that can be replayed in `pmemanalyze`

## Performing performance analyze (real machine)
1. Copy the trace files from the VM to the machine that has real PMEM using a tool like `scp`.
2. Ensure you have installed `pmemreplay` by executing the `../../install-real-machine.sh` script.
3. In system with multiple Optane DIMMs, make sure interleaving is disabled:

```
ndctl destroy-namespace -f all
ipmctl create -goal PersistentMemoryType=AppDirectNotInterleaved
# Reboot now!
# Execute after reboot:
ndctl create-namespace
```

4. Configure PMEM as a devdax device: `../../mount-devdax.sh`
5. Replay both compressed trace files using `pmemanalyze`: `sudo pmemanalyze --device /dev/dax0.0 **TRACE_FILE**.parquet`.
6. Plot results: `sudo pmemanalyze --plot`.
