import csv
import subprocess

GROUPS = [
    {
        "name": "overhead_single_stepping",
        "symbols": ["debug", "do_debug", "async_page_fault", "skip_prefix", "paranoid_entry", "lookup_address_in_pgd", \
                        "notify_die", "arm_kmmio_fault_page", "get_ins_reg_val", "get_ins_mem_width," "get_ins_reg_width", "lookup_user_address", "kmmio_die_notifier"],
    },
    {
        "name": "overhead_tlb_flushing",
        "symbols": ["native_flush_tlb_one_user"],
    },
    {
        "name": "overhead_syscalls",
        "symbols": ["entry_SYSCALL_64"],
    },
    {
        "name": "overhead_io",
        "symbols": ["__memcpy_flushcache", "__copy_user_nocache"]
    },
    {
        "name": "overhead_locking",
        "symbols": ["mutex", "spin_lock", "spin_unlock"]
    },
    {
        "name": "misc",
        "symbols" : []
    }
]

WORKING_SET_SIZES = [2, 4, 8, 16, 32]
NUM_RUNS = 10

CSV_HEADER = ["run_id", "working_set_size"]
for group in GROUPS:
    CSV_HEADER.append(f"overhead_percent_{group['name']}")


def get_overhead_percent_for_group(line, group):
    total_overhead = 0
    if any(symbol in line for symbol in group["symbols"]):
        overhead_percent = round(float(line.split()[0].replace("%", "")), 2)
        total_overhead += overhead_percent
    return total_overhead


def parse_output(output, run_id, size):
    result = {"run_id": run_id}
    result["working_set_size"] = size
    for line in output.splitlines():
        overhead_percentages = {}

        found = False
        for group in GROUPS:
            overhead_percent = get_overhead_percent_for_group(line, group)
            if overhead_percent:
                overhead_percentages[group["name"]] = overhead_percent
                found = True
                break  

        if not found:
            overhead_percent = round(float(line.split()[0].replace("%", "")), 2)
            if "misc" in overhead_percentages:
                overhead_percentages["misc"] += overhead_percent
            else:
                overhead_percentages["misc"] = overhead_percent

        for group in GROUPS:
            column_name = f"overhead_percent_{group['name']}"
            if column_name in result:
                result[column_name] += overhead_percentages.get(group["name"], 0)
            else:
                result[column_name] = overhead_percentages.get(group["name"], 0)

    return result


def main():
    results = []
    for size in WORKING_SET_SIZES:
        for run_id in range(NUM_RUNS):
            cmd = f"sudo pmemtrace randread sudo bash -c 'perf record -F 2000 -g --call-graph dwarf head -c {size}M </dev/urandom >/mnt/pmem_emul/rand_file.txt' --disable-sampling"
            output = subprocess.run(cmd, shell=True, check=True, capture_output=True)

            cmd = "sudo perf report --no-children --stdio | sort -rn | head -25"
            output = subprocess.run(cmd, shell=True, check=True, capture_output=True,)

            results += [parse_output(output.stdout.decode(), 1 + run_id, size)]
            print(output.stdout.decode())


    with open("results.csv", mode="w", newline="") as csv_file:
        writer = csv.DictWriter(csv_file, fieldnames=CSV_HEADER)
        writer.writeheader()

        for result in results:
            writer.writerow(result)


if __name__ == "__main__":
    main()
