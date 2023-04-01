#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <inttypes.h>
#include <assert.h>

#include <iostream>

#include <string>
#include <sstream>
#include <filesystem>
#include <vector>
#include <regex>
#include <fstream>
#include <optional>
#include <filesystem>


#include <arrow/api.h>
#include <arrow/io/api.h>
#include <parquet/arrow/reader.h>
#include <parquet/arrow/writer.h>
#include <parquet/stream_reader.h>
#include <parquet/stream_writer.h>

using parquet::ArrowWriterProperties;
using parquet::WriterProperties;

enum class TraceOperation {
    READ = 0,
    WRITE = 1,
    CLFLUSH = 2
};


//#define DEBUG
#define ND_CMD_TRACE_ENABLE 11
#define ND_CMD_TRACE_DISABLE 12

#define CURRENT_TRACER "/sys/kernel/debug/tracing/current_tracer"

#define TRACER_BUF_SIZE 128000
#define TRACER_BUF_SIZE_PATH "/sys/kernel/debug/tracing/buffer_size_kb"
#define TRACER_OUTPUT_PIPE "/sys/kernel/debug/tracing/trace_pipe"


#define ENABLE_SAMPLING
const unsigned int SAMPLE_RATE = 10; // was 60 hz for ext4-dax
const double DUTY_CYCLE = 1; // was 2 for ext4-dax

volatile bool is_stopped = false;
pthread_mutex_t stopMutex;
pid_t exec_pid;
//#define ND_IOCTL_VENDOR _IOWR(ND_IOCTL, ND_CMD_TRACE, void)
//#define ND_IOCTL_VENDOR _IOWR(ND_IOCTL, ND_CMD_TRACE_DISABLE, void)


struct sample_thread_args {
	unsigned int sample_rate;
	double duty_cycle;
	int fd;
};

struct read_thread_args {
	const char *output_file;
	const char *str_cmd;
};

bool getStopIssued(void) {
	bool ret;
	pthread_mutex_lock(&stopMutex);
	ret = is_stopped;
	pthread_mutex_unlock(&stopMutex);
	return ret;
}

void setStopIssued(bool val) {
	pthread_mutex_lock(&stopMutex);
	is_stopped = val;
	pthread_mutex_unlock(&stopMutex);
}

bool is_mmiotrace_enabled() {
	char tracer[256];
	FILE *fp = fopen(CURRENT_TRACER, "r");
	if (fp == NULL) {
		perror("Error opening current_tracer file");
		exit(EXIT_FAILURE);
	}
	(void) fgets(tracer, sizeof(tracer), fp);
	fclose(fp);
	tracer[strcspn(tracer, "\n")] = 0; // remove newline character

	#ifdef DEBUG
	printf("Current tracer: %s\n", tracer);
	#endif

	return (strcmp(tracer, "mmiotrace") == 0);
}

void toggle_mmiotrace(bool enable)
{
	FILE *fp = fopen(CURRENT_TRACER, "w");
	if (fp == NULL) {
		perror("Error opening current_tracer file");
		exit(EXIT_FAILURE);
	}

	fprintf(fp, ((enable) ? "mmiotrace" : "nop"));

	fflush(fp);
	fclose(fp);
}

void get_pmem_range(unsigned long *start, unsigned long *end) {
    FILE *fp = fopen("/proc/iomem", "r");
    if (fp == NULL) {
        perror("Error opening /proc/iomem");
        return;
    }
    char line[256] = {0};
    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "Persistent Memory") != NULL) {
            char *start_str = strtok(line, "-");
            char *end_str = strtok(NULL, " ");
            *start = strtoul(start_str, NULL, 16);
            *end = strtoul(end_str, NULL, 16);
            fclose(fp);
            return;
        }
    }
    fclose(fp);
    perror("PMEM range not found in /proc/iomem");
}

inline void enable_pmemtrace(int fd)
{
	#ifndef DEBUG
	ioctl(fd, ND_CMD_TRACE_ENABLE);
	#endif
	//sleep(1);
}

inline void disable_pmemtrace(int fd) {
	// printf("Disabling pmemtrace...\n");
	#ifndef DEBUG
	ioctl(fd, ND_CMD_TRACE_DISABLE);
	#endif
}

unsigned int get_trace_buf_size() {
	char buf[32];

	int fd = open(TRACER_BUF_SIZE_PATH, O_RDONLY);
    if (fd < 0) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    if (read(fd, buf, sizeof(buf)) < 0) {
        perror("Failed to read file");
        exit(EXIT_FAILURE);
    }

	close(fd);

	uintmax_t buf_size = strtoumax(buf, NULL, 10);

	if (buf_size == UINTMAX_MAX && errno == ERANGE) {
		perror("Error reading current buffer size!\n");
		exit(EXIT_FAILURE);
	}

	return (unsigned int) buf_size;
}

void set_trace_buf_size(const unsigned int buf_size) {
	int fd = open(TRACER_BUF_SIZE_PATH, O_WRONLY);
    if (fd < 0) {
        perror("Failed to open file");
        exit(EXIT_FAILURE);
    }

    if (dprintf(fd, "%d", (int) buf_size) < 0) {
        perror("Failed to write to file");
        exit(EXIT_FAILURE);
    }

    close(fd);
}

void* pmem_sampler(void *arg)
{
	const struct sample_thread_args* thread_args = (const struct sample_thread_args* ) arg;

	const int period = 1000000 / thread_args->sample_rate;
	bool is_enabled = true;

	printf("Thread running...\n");

	while (!getStopIssued()) {
		if (is_enabled) {
			usleep(period * thread_args->duty_cycle);
			#ifndef DEBUG
			ioctl(thread_args->fd, ND_CMD_TRACE_DISABLE);
			#endif
		} else {
			usleep(period);
			#ifndef DEBUG
			ioctl(thread_args->fd, ND_CMD_TRACE_ENABLE);
			#endif
		}

		is_enabled = !(is_enabled);
	}

	#ifndef DEBUG
	ioctl(thread_args->fd, ND_CMD_TRACE_DISABLE);
	#endif

	pthread_exit(NULL);
}

void* pmemtrace_output_thread(void *arg)
{
	const struct read_thread_args* thread_args = (const struct read_thread_args* ) arg;

	int in = open(TRACER_OUTPUT_PIPE, O_RDONLY);
	int out = open(thread_args->output_file, O_WRONLY | O_CREAT | O_TRUNC, 644);

	if (!in || !out) {
		perror("Failed to open file!\n");
		pthread_exit(NULL);
	}

	if (chmod(thread_args->output_file, 0644) < 0) {
		perror("Failed setting file permissions!\n");
		pthread_exit(NULL);
	}

	unsigned long device_start = 0, device_end = 0;

	get_pmem_range(&device_start, &device_end);
	char header_str[128] = {0};
	char cmd_str[256] = {0};
	sprintf(header_str, "PMEMTRACE DEVICE: [%p-%p]\n", (void*) device_start, (void*) device_end);
	sprintf(cmd_str, "TRACE COMMAND:%s\n###\n", thread_args->str_cmd);

	if (write(out, header_str, strnlen(header_str, sizeof(header_str))) < (ssize_t) sizeof(header_str)) {
		perror("Writing error!\n");
		pthread_exit(NULL);
	}
	if (write(out, cmd_str, strnlen(cmd_str, sizeof(cmd_str))) < (ssize_t) sizeof(header_str)) {
		perror("Writing error!\n");
		pthread_exit(NULL);
	}

	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(in, &rfds);

	//struct timeval timeout = {0};
	//timeout.tv_sec = 1;

	char buffer[1024];
	size_t n;

	while (true) {
		if (getStopIssued())
			break;

		n = read(in, buffer, sizeof(buffer));
		if (write(out, buffer, n) == 0) {
			perror("Reading error!\n");
			pthread_exit(NULL);
		}

		//rv = select(in + 1, &rfds, NULL, NULL, &timeout);

		//if (rv == -1) {
		//	perror("Reading error!\n");
		//	pthread_exit(NULL);
		//} else if (rv > 0) {
		//	n = read(in, buffer, sizeof(buffer));
		//	write(out, buffer, n);
		//}
	}


	// while (!getStopIssued() && ((n = fread(buffer, 1, sizeof(buffer), in)) > 0)) {
	// 	fwrite(buffer, 1, n, out);			
	// }

	// Make sure all data is flushed.
	// while ((n = fread(buffer, 1, sizeof(buffer), in)) > 0) {
	// 	fwrite(buffer, 1, n, out);
	// }

	close(in);
	close(out);

	
	pthread_exit(NULL);
}

void signal_handler(int sig)
{
	if (sig == SIGINT) {
		setStopIssued(true);
		putchar('\n');
		//signal(SIGINT, SIG_DFL);
		//raise(SIGINT);

		if (exec_pid) {
			kill(exec_pid, SIGTERM);
		}
	}
}

char* concat_args(int argc, char* argv[]) {
    char* result = (char*) malloc(sizeof(char));
    result[0] = '\0';
    for (int i = 0; i < argc; i++) {
        result = (char*) realloc(result, strlen(result) + strlen(argv[i]) + 2);
        strcat(result, " ");
        strcat(result, argv[i]);
    }
    return result;
}

int compress_trace(std::filesystem::path path)
{
    std::ifstream trace_handle(path);

    if (!trace_handle) {
        std::cerr << "Failed to open trace file: " << path << std::endl;
        return -1;
    }

    std::string line;

    if (!std::getline(trace_handle, line)) {
        return -1;
    }

    std::regex re(R"(PMEMTRACE DEVICE:\s+\[(0x[\da-fA-F]+)-(0x[\da-fA-F]+)\])");
    std::smatch match;

    if (!std::regex_search(line, match, re)) {
        std::cerr << "Invalid trace file format" << std::endl;
        return -1;
    }

    const auto pmem_range_start = std::stoul(match[1].str(), nullptr, 16);
    const auto pmem_range_end = std::stoul(match[2].str(), nullptr, 16);
    std::cout << "Start address: " << std::hex << pmem_range_start << '\n';
    std::cout << "End address: " << std::hex << pmem_range_end << '\n';
    std::cout << "Size: " << std::dec << (pmem_range_end - pmem_range_start) / (1024 * 1024 * 1024) << " GiB" << std::endl; 
   
    std::regex pattern(R"((R|W|F)\s+(\d+)\s+(0x[\da-fA-F]+)\s+([\d.]+)\s+\d+\s+(0x[\da-fA-F]+)\s+(0x[\da-fA-F]+))");
    //TraceFile trace;
    std::smatch matches;
    

    std::shared_ptr<WriterProperties> props =
        WriterProperties::Builder().compression(arrow::Compression::GZIP)->build();

    std::shared_ptr<arrow::io::FileOutputStream> outfile;

    path = path.replace_extension(".parquet");

    PARQUET_ASSIGN_OR_THROW(
        outfile,
        arrow::io::FileOutputStream::Open(path));

    parquet::WriterProperties::Builder builder;
    std::shared_ptr<parquet::schema::GroupNode> schema;
    
    
    // https://arrow.apache.org/docs/cpp/parquet.html
    schema = std::static_pointer_cast<parquet::schema::GroupNode>(parquet::schema::GroupNode::Make(
        path.filename(), parquet::Repetition::REQUIRED, {
            parquet::schema::PrimitiveNode::Make("timestamp", parquet::Repetition::REQUIRED, parquet::Type::DOUBLE),
            parquet::schema::PrimitiveNode::Make("op", parquet::Repetition::REQUIRED, parquet::Type::INT32, parquet::ConvertedType::UINT_32),
            parquet::schema::PrimitiveNode::Make("opcode", parquet::Repetition::REQUIRED, parquet::Type::INT32, parquet::ConvertedType::UINT_32),
            parquet::schema::PrimitiveNode::Make("op_size", parquet::Repetition::REQUIRED, parquet::Type::INT64, parquet::ConvertedType::UINT_64),
            parquet::schema::PrimitiveNode::Make("abs_addr", parquet::Repetition::REQUIRED, parquet::Type::INT64, parquet::ConvertedType::UINT_64),
            parquet::schema::PrimitiveNode::Make("rel_addr", parquet::Repetition::REQUIRED, parquet::Type::INT64, parquet::ConvertedType::UINT_64),
            parquet::schema::PrimitiveNode::Make("data", parquet::Repetition::REQUIRED, parquet::Type::INT64, parquet::ConvertedType::UINT_64),
        }
    ));
    
    parquet::StreamWriter os{
        parquet::ParquetFileWriter::Open(outfile, schema, props)};


    while (std::getline(trace_handle, line)) {
        if (std::regex_search(line, matches, pattern)) {
            TraceOperation op;

            if (matches[1] == "R") {
                op = TraceOperation::READ;
            } else if (matches[1] == "W") {
                op = TraceOperation::WRITE;
            } else if (matches[1] == "F") {
                op = TraceOperation::CLFLUSH;
            } else {
                std::cerr << "Unknown trace operation: " << matches[1] << std::endl;

                return -1;
            }

            // const std::vector<uint8_t> op_bytes = hex_string_to_bytes(matches[5]);
            // std::cout << matches[5] << std::endl;


            const double timestamp_sec = std::stod(matches[4]);
            const unsigned int opcode = std::stoi(matches[3], nullptr, 16);
            const size_t opcode_size = std::stoul(matches[2], nullptr, 16);
            const unsigned long abs_addr = std::stoul(matches[5], nullptr, 16);
            #ifdef ENABLE_ASSERTS
            assert(abs_addr > pmem_range_strawnameart);
            assert(abs_addr < pmem_range_end);
            #endif
            const unsigned long rel_addr = abs_addr - pmem_range_start;
            const unsigned long data = std::stoul(matches[6], nullptr, 16);


            //std::cout << std::hex << abs_addr << std::endl;

            //trace.emplace_back(op, std::stoi(matches[2]), std::stod(matches[3]), abs_addr, rel_addr, op_bytes);
            //TraceEntry(const TraceOperation op, const size_t op_size, const unsigned int opcode, const double timestamp_sec, const unsigned long abs_addr, const unsigned long rel_addr, const unsigned long long data) :
            //trace_entries.emplace_back(op, std::stoi(matches[2]), std::stoi(matches[3], nullptr, 16), std::stod(matches[4]), abs_addr, rel_addr, data);


            os << timestamp_sec << static_cast<uint32_t>(op) << opcode << opcode_size << abs_addr << rel_addr << data << parquet::EndRow;
        }
    }

    std::cout << "num columns: " << std::dec << os.num_columns() << std::endl;

    // std::cout << std::dec << trace_entries.size() << " entries" << std::endl;

    // arrow::Status st = trace_to_arrow(std::filesystem::path(filename), trace_entries);

    // if (!st.ok()) {
    //     std::cerr << st << std::endl;
    //     return -1;
    // }
   
    return 0;
}

int main(int argc, char** argv)
{
	// sudo ./tracer /dev/ndctl0 sudo bash -c "head -c 1M </dev/urandom >/mnt/pmem_emul/some_rand2.txt"

	assert(DUTY_CYCLE > 0);
	assert(SAMPLE_RATE > 0);

	if (geteuid() != 0) {
		fprintf(stderr, "This program must be run as root.\n");
		exit(EXIT_FAILURE);
	}

	if (argc < 2) {
		fprintf(stderr, "Please provide a ndctl device handle, e.g. /dev/ndctl0\n");
		exit(EXIT_FAILURE);
	}

	if (argc < 3) {
		fprintf(stderr, "Please specify the trace name, e.g. my_simple_trace\n");
		exit(EXIT_FAILURE);
	}

	if (argc < 4) {
		fprintf(stderr, "Please provide the command to run\n");
		exit(EXIT_FAILURE);
	}


	if (is_mmiotrace_enabled()) {
		printf("mmiotrace is enabled\n");
   	} else {
		fprintf(stderr, "mmiotrace is not enabled, enabling...\n");

		toggle_mmiotrace(true);
		sleep(2);

		//exit(EXIT_FAILURE);
  	}

	if (get_trace_buf_size() < TRACER_BUF_SIZE) {
		printf("Increasing tracing buffer size to %u bytes...\n", TRACER_BUF_SIZE);
		set_trace_buf_size(TRACER_BUF_SIZE);
		sleep(2);
	}

	//printf("Current trace buffer size: %u\n", get_trace_buf_size());

	char* trace_name = strdup(argv[2]);
	char* trace_loc = strcat(trace_name, ".trf");
        printf("Trace file location: %s\n", trace_loc);
        char* merge_cmd = concat_args(argc - 3, &argv[3]);

        struct read_thread_args rd_thread_args = {trace_loc, merge_cmd};



	int pipe_fds[2];

	if (pipe(pipe_fds) < 0) {
        perror("Unable to establish pipe between processes!\n");
        exit(EXIT_FAILURE);
    }

	if ((exec_pid = fork()) < 0) {
		fprintf(stderr, "Error: fork error!\n");
		exit(EXIT_FAILURE);
	}

	int ret_code, status;

	if (exec_pid == 0) {
		char buf[1];
		close(pipe_fds[1]);
		(void) read(pipe_fds[0], buf, 1);

		execvp(argv[3], &argv[3]);

		exit(EXIT_FAILURE);
	}

	int fd = open(argv[1], O_RDWR);
	if (fd < 0)
	{
		fprintf(stderr, "Failed to open device %s!\n", argv[1]);
		exit(EXIT_FAILURE);
	}

	
	printf("Opened device, enabling pmemtrace...\n");
	struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGINT);
    sigaction(SIGINT, &sa, NULL);

	enable_pmemtrace(fd);
	
	pthread_t tid_rd;
	// pthread_attr_t attr;
	// sched_param param;
	// pthread_attr_init(&attr);
	// attr.sched_priority = 0;
	

	if (pthread_create(&tid_rd, NULL, pmemtrace_output_thread, (void*) &rd_thread_args) < 0) {
		fprintf(stderr, "Error: pthread_create failed!\n");
		exit(EXIT_FAILURE);
	}

	if (pthread_setschedprio(tid_rd, 50) < 0) {
		fprintf(stderr, "Error: set prio failed!\n");
		exit(EXIT_FAILURE);
	}

	#ifdef ENABLE_SAMPLING
	pthread_t tid_smpl;
	struct sample_thread_args smpl_thread_args = {SAMPLE_RATE, DUTY_CYCLE, fd};

	if (pthread_create(&tid_smpl, NULL, pmem_sampler, (void*) &smpl_thread_args) < 0) {
		fprintf(stderr, "Error: pthread_create failed!\n");
		exit(EXIT_FAILURE);
	}

	pthread_detach(tid_smpl);
	#endif
	pthread_detach(tid_rd);

	sleep(3);

	printf("Running command...\n\f");

	close(pipe_fds[0]);
	write(pipe_fds[1], "x", 1);

	waitpid(exec_pid, &status, 0);

	ret_code = WIFEXITED(status);

	printf("Stop running...\n");

	setStopIssued(true);

	
	pthread_join(tid_rd, NULL);
	#ifdef ENABLE_SAMPLING
	pthread_join(tid_smpl, NULL);
	#endif
	

	//printf("PIDs: %ld %ld %ld\n", tid_rd, tid_smpl, exec_pid);
	
	printf("Disabling pmemtrace...\n");

	disable_pmemtrace(fd);

	close(fd);
	sleep(2);
	toggle_mmiotrace(false);

	free(merge_cmd);

	printf("Compressing trace file...\n");

	compress_trace(std::string(trace_loc));

	printf("Done!\n");

	return ret_code;
}
