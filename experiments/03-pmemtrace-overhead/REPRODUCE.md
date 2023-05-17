## Quantifying pmemtrace overhead

## Prerequisites
- Working VM installation, which included installing all dependencies and pmemtrace itself using install script (see Thesis Artifact for installation steps);
- Python 3 (including matplotlib package);

## Steps
1. Execute the `run.py` file. This script will run the workload using `perf`.
2. Execute the `plot.py` file. This script will automatically categorize overheads based on the `perf` report file and create a plot figure.
