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

#include "CLI11.hpp"

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
#define ND_CMD_TRACE_FREQ _IOWR('N', 13, unsigned int[3])
#define ND_CMD_TRACE_IS_MULTICORE _IOWR('N', 14, unsigned int*)
//#define ND_CMD_TRACE_FREQ 13

#define CURRENT_TRACER "/sys/kernel/debug/tracing/current_tracer"

#define TRACER_BUF_SIZE (128000 * 2)
#define TRACER_BUF_SIZE_PATH "/sys/kernel/debug/tracing/buffer_size_kb"
#define TRACER_OUTPUT_PIPE "/sys/kernel/debug/tracing/trace_pipe"


unsigned int SAMPLE_RATE = 0; // was 60 hz for ext4-dax
double DUTY_CYCLE = 0.5;

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

/*
void* pmem_sampler(void *arg)
{
	const struct sample_thread_args* thread_args = (const struct sample_thread_args* ) arg;

	const int period = 1000000 / thread_args->sample_rate;
	const float period_on = period * thread_args->duty_cycle;
	const float period_off = period * (1.0 - thread_args->duty_cycle);
	bool is_enabled = true;

	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(0, &cpuset);
	pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);

	struct sched_param sp;
        sp.sched_priority = sched_get_priority_max(SCHED_FIFO);

	if (sched_setscheduler(getpid(), SCHED_FIFO, &sp) < 0) {
        fprintf(stderr, "Error setting SCHED_FIFO policy, errno: %d.\n", errno);
        exit(EXIT_FAILURE);
    }

	if (thread_args->duty_cycle < 0.0 || thread_args->duty_cycle > 1.0) {
		fprintf(stderr, "Duty cycle is not between 0.0 and 1.0");
		exit(EXIT_FAILURE);
	}


	printf("Thread running... %f %f\n", period_on, period_off);

	while (!getStopIssued()) {
		if (period_off > 0.0) { 
			if (is_enabled) {
				usleep(period_on);
				#ifndef DEBUG
				ioctl(thread_args->fd, ND_CMD_TRACE_DISABLE);
				#endif
			} else {
				usleep(period_off);
				#ifndef DEBUG
				ioctl(thread_args->fd, ND_CMD_TRACE_ENABLE);
				#endif
			}

			is_enabled = !(is_enabled);
		}
	}

	#ifndef DEBUG
	ioctl(thread_args->fd, ND_CMD_TRACE_DISABLE);
	#endif

	sp.sched_priority = sched_get_priority_min(SCHED_OTHER);

	if (sched_setscheduler(getpid(), SCHED_OTHER, &sp) < 0) {
              fprintf(stderr, "Error setting SCHED_OTHER policy, errno: %d.\n", errno);
              exit(EXIT_FAILURE);
        }

	pthread_exit(NULL);
}*/

void* pmemtrace_output_thread(void *arg)
{
	const struct read_thread_args* thread_args = (const struct read_thread_args* ) arg;

	int in = open(TRACER_OUTPUT_PIPE, O_RDONLY);
	int out = open(thread_args->output_file, O_WRONLY | O_CREAT | O_TRUNC, 644);

	if (!in || !out) {
		perror("Failed to open file!\n");
		pthread_exit(NULL);
	}

	int flags = fcntl(in, F_GETFL, 0);
	fcntl(in, F_SETFL, flags | O_NONBLOCK);


	if (chmod(thread_args->output_file, 0644) < 0) {
		perror("Failed setting file permissions!\n");
		pthread_exit(NULL);
	}

	unsigned long device_start = 0, device_end = 0;

	get_pmem_range(&device_start, &device_end);
	char header_str[256] = {0};
	char cmd_str[512] = {0};
	sprintf(header_str, "PMEMTRACE DEVICE: [%p-%p]\n", (void*) device_start, (void*) device_end);
	sprintf(cmd_str, "TRACE COMMAND:%s\n###\n", thread_args->str_cmd);

	write(out, header_str, strnlen(header_str, sizeof(header_str)));
	write(out, cmd_str, strnlen(cmd_str, sizeof(cmd_str)));


	char buffer[1024];
	size_t n;
	std::string temp_str;

	while (true) {
		if (getStopIssued())
			break;

		n = read(in, buffer, sizeof(buffer));
 		if (n)
			write(out, buffer, n);
	}

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

template<size_t N>
bool tokenize(std::string const &str, std::array<std::string, N>& out)
{
	std::stringstream ss(str);

	std::string s;
	size_t i = 0;

	while (std::getline(ss, s, ' ')) {
		if ((i + 1) > N) {
			return false;
		}

		out[(i++)] = s;
	}

	return (i == N);
}

bool compress_trace(std::filesystem::path read_path, std::filesystem::path write_path)
{
    std::ifstream trace_handle(read_path);

	std::cout << "Compressing to file " << write_path << std::endl;

    if (!trace_handle) {
        std::cerr << "Failed to open trace file: " << read_path << std::endl;
        return false;
    }

    std::string line;

    if (!std::getline(trace_handle, line)) {
        return false;
    }

    std::regex re(R"(PMEMTRACE DEVICE:\s+\[(0x[\da-fA-F]+)-(0x[\da-fA-F]+)\])");
    std::smatch match;

    if (!std::regex_search(line, match, re)) {
        std::cerr << "Invalid trace file format" << std::endl;
        return false;
    }

    const auto pmem_range_start = std::stoul(match[1].str(), nullptr, 16);
    const auto pmem_range_end = std::stoul(match[2].str(), nullptr, 16);
    std::cout << "Start address: " << std::hex << pmem_range_start << '\n';
    std::cout << "End address: " << std::hex << pmem_range_end << '\n';
    std::cout << "Size: " << std::dec << (pmem_range_end - pmem_range_start) / (1024 * 1024 * 1024) << " GiB" << std::endl; 
   
    
    std::shared_ptr<WriterProperties> props =
        WriterProperties::Builder().compression(arrow::Compression::GZIP)->build();

    std::shared_ptr<arrow::io::FileOutputStream> outfile;

    PARQUET_ASSIGN_OR_THROW(
        outfile,
        arrow::io::FileOutputStream::Open(write_path));

    parquet::WriterProperties::Builder builder;
    std::shared_ptr<parquet::schema::GroupNode> schema;
    
    // https://arrow.apache.org/docs/cpp/parquet.html
    schema = std::static_pointer_cast<parquet::schema::GroupNode>(parquet::schema::GroupNode::Make(
        write_path.filename(), parquet::Repetition::REQUIRED, {
            parquet::schema::PrimitiveNode::Make("timestamp", parquet::Repetition::REQUIRED, parquet::Type::DOUBLE),
            parquet::schema::PrimitiveNode::Make("op", parquet::Repetition::REQUIRED, parquet::Type::INT32, parquet::ConvertedType::UINT_32),
            parquet::schema::PrimitiveNode::Make("opcode", parquet::Repetition::REQUIRED, parquet::Type::INT32, parquet::ConvertedType::UINT_32),
            parquet::schema::PrimitiveNode::Make("op_size", parquet::Repetition::REQUIRED, parquet::Type::INT64, parquet::ConvertedType::UINT_64),
            parquet::schema::PrimitiveNode::Make("abs_addr", parquet::Repetition::REQUIRED, parquet::Type::INT64, parquet::ConvertedType::UINT_64),
            parquet::schema::PrimitiveNode::Make("rel_addr", parquet::Repetition::REQUIRED, parquet::Type::INT64, parquet::ConvertedType::UINT_64),
            parquet::schema::PrimitiveNode::Make("data", parquet::Repetition::REQUIRED, parquet::Type::INT64, parquet::ConvertedType::UINT_64),
			parquet::schema::PrimitiveNode::Make("cpu_id", parquet::Repetition::REQUIRED, parquet::Type::INT32, parquet::ConvertedType::UINT_32),
        }
    ));
    
    parquet::StreamWriter os{
        parquet::ParquetFileWriter::Open(outfile, schema, props)};

	TraceOperation op;
	std::array<std::string, 9> tokens_line;
	

    while (std::getline(trace_handle, line)) {
		if (!tokenize(line, tokens_line)) {
			continue;
		}

		if (tokens_line[0] == "R") {
			op = TraceOperation::READ;
		} else if (tokens_line[0] == "W") {
			op = TraceOperation::WRITE;
		} else if (tokens_line[0] == "F") {
			op = TraceOperation::CLFLUSH;
		} else {
			//std::cerr << "Unknown trace operation: " << tokens_line[0] << std::endl;

			continue;
			//return false;
		}

		const double timestamp_sec = std::stod(tokens_line[3]);
		const unsigned int opcode = std::stoi(tokens_line[2], nullptr, 16);
		const size_t opcode_size = std::stoul(tokens_line[1], nullptr, 16);
		const unsigned long abs_addr = std::stoul(tokens_line[5], nullptr, 16);
		#ifdef ENABLE_ASSERTS
		assert(abs_addr > pmem_range_strawnameart);
		assert(abs_addr < pmem_range_end);
		#endif
		const unsigned long rel_addr = abs_addr - pmem_range_start;
		const unsigned long long data = std::stoull(tokens_line[6], nullptr, 16);
		const unsigned int cpu_id = std::stoi(tokens_line[8]);

		//std::cout << timestamp_sec << " " << static_cast<uint32_t>(op) << " " << std::hex << opcode << " " << std::dec << opcode_size << " 0x" << std::hex << abs_addr << " 0x" << rel_addr << " 0x" << data << " " << std::dec << cpu_id << std::endl;


		os << timestamp_sec << static_cast<uint32_t>(op) << opcode << opcode_size << abs_addr << rel_addr << static_cast<uint64_t>(data) << cpu_id << parquet::EndRow;
    }

    return true;
}

int main(int argc, char** argv)
{
	if (geteuid() != 0) {
		fprintf(stderr, "This program must be run as root.\n");
		exit(EXIT_FAILURE);
	}


	std::string pmem_device_loc;
	std::string trace_name;
	bool disable_sampling = false;
	unsigned int enable_multicore = false;

 	CLI::App app{"pmemtrace - A Persistent Memory Micro-Architecture Aware Trace Capture Tool"};

	app.add_option("tracename", trace_name, "Trace name")
        ->required();
	app.add_option("--device", pmem_device_loc, "Persistent Memory ndctl device handle")
        ->default_val("/dev/ndctl0");
	app.add_flag("--disable-sampling", disable_sampling, "Disable sampling; use a 100\% duty cycle")
		->default_val(false);
	app.add_flag("--enable-multicore", enable_multicore, "Enable multicore support (experimental, unstable)")
		->default_val(false);
	auto smpl_option = app.add_option("-s, --sample-rate", SAMPLE_RATE, "Sample rate")
        ->default_val(SAMPLE_RATE)->check(CLI::Range(0, 240, "Sample rate must be between 0 and 240 Hz"));;
	app.add_option("--duty-cycle", DUTY_CYCLE, "Duty cycle")
		->default_val(DUTY_CYCLE)->check(CLI::Range(0.0, 1.0, "Duty cycle must be between 0.0 and 1.0"));
	app.add_option("--sample-rate-pfaults", SAMPLE_RATE, "Toggle probing on/off after a selected number of page faults, e.g. 10")
		->excludes(smpl_option);
	
	app.allow_extras();

   	CLI11_PARSE(app, argc, argv);

	std::vector<std::string> _remaining_args = app.remaining();

	if (_remaining_args.size() == 0) {
		perror("Provide an CLI command!\n");
		return -1;
	}

	char **remaining_args = new char*[_remaining_args.size()];

	for (size_t i = 0; i < _remaining_args.size(); ++i) {
		remaining_args[i] = new char[_remaining_args[i].size() + 1];
		std::strcpy(remaining_args[i], _remaining_args[i].c_str());
	}


	int pipe_fds[2];

	if (pipe(pipe_fds) < 0) {
        perror("Unable to establish pipe between processes!\n");
        exit(EXIT_FAILURE);
    }

	if ((exec_pid = fork()) < 0) {
		fprintf(stderr, "Error: fork error!\n");
		exit(EXIT_FAILURE);
	}

	if (exec_pid == 0) {
		char buf[1];
		close(pipe_fds[1]);

		//char* args = &(remaining_args[0]);
		(void) read(pipe_fds[0], buf, 1);

		execvp(remaining_args[0], &(remaining_args[0]));
		exit(EXIT_FAILURE);
	}

	// Combine remaining_args into a single string
	std::string cmd;
	for (const auto& arg : _remaining_args) {
		cmd += arg;
		cmd += " ";
	}

	int fd = open(pmem_device_loc.c_str(), O_RDWR);
	if (fd < 0)
	{
		fprintf(stderr, "Failed to open device %s!\n", pmem_device_loc.c_str());
		exit(EXIT_FAILURE);
	}

	if (ioctl(fd, ND_CMD_TRACE_IS_MULTICORE, &enable_multicore) < 0) {
		printf("Warning: failed to set multicore to %u\n", enable_multicore);
	}

	if (is_mmiotrace_enabled()) {
		printf("mmiotrace is enabled\n");
   	} else {
		fprintf(stderr, "mmiotrace is not enabled, enabling...\n");

		toggle_mmiotrace(true);
		sleep(1);

		//exit(EXIT_FAILURE);
  	}

	if (get_trace_buf_size() < TRACER_BUF_SIZE) {
		printf("Increasing tracing buffer size to %u bytes...\n", TRACER_BUF_SIZE);
		set_trace_buf_size(TRACER_BUF_SIZE);
		sleep(1);
	}

	//printf("Current trace buffer size: %u\n", get_trace_buf_size());

	const std::string temp_trace_loc = "/tmp/" + trace_name + ".temp";
	const std::string compressed_trace_loc = trace_name + ".parquet";

	printf("Temp trace file location: %s\n", temp_trace_loc.c_str());

	struct read_thread_args rd_thread_args = {temp_trace_loc.c_str(), cmd.c_str()};



	
	printf("Opened device, enabling pmemtrace...\n");
	struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGINT);
    sigaction(SIGINT, &sa, NULL);

	

	enable_pmemtrace(fd);

	#ifndef DEBUG
	unsigned int ioctl_payload[3] = {0};
	
	if (!disable_sampling) {
		ioctl_payload[0] = SAMPLE_RATE;
		ioctl_payload[1] = static_cast<unsigned int>(DUTY_CYCLE * 100);
		ioctl_payload[2] = (app.count("--sample-rate-pfaults")) ? 0 : 1;
	}

	ioctl(fd, ND_CMD_TRACE_FREQ, &ioctl_payload);
	#endif

	// exit(EXIT_SUCCESS);
	
	pthread_t tid_rd;

	if (pthread_create(&tid_rd, NULL, pmemtrace_output_thread, (void*) &rd_thread_args) < 0) {
		fprintf(stderr, "Error: pthread_create failed!\n");
		exit(EXIT_FAILURE);
	}

	// if (!disable_sampling) {
	// 	struct sample_thread_args smpl_thread_args = {SAMPLE_RATE, DUTY_CYCLE, fd};

	// 	if (pthread_create(&tid_smpl, NULL, pmem_sampler, (void*) &smpl_thread_args) < 0) {
	// 		fprintf(stderr, "Error: pthread_create failed!\n");
	// 		exit(EXIT_FAILURE);
	// 	}

	// 	pthread_detach(tid_smpl);
	// }
	pthread_detach(tid_rd);

	sleep(1);

	printf("Running command...\n\f");

	close(pipe_fds[0]);
	write(pipe_fds[1], "x", 1);

	int ret_code = 1, status;


	waitpid(exec_pid, &status, 0);

	if (WIFEXITED(status)) {
		ret_code = WEXITSTATUS(status);
	}

	setStopIssued(true);
	
	pthread_join(tid_rd, NULL);
	// if (!disable_sampling) {
	// 	pthread_join(tid_smpl, NULL);
	// }

	sleep(1);

	printf("Disabling pmemtrace...\n");

	disable_pmemtrace(fd);

	close(fd);
	sync();
	sleep(1);

	toggle_mmiotrace(false);

	printf("Compressing trace file...\n");

	if (!compress_trace(temp_trace_loc, compressed_trace_loc)) {
		perror("Failed to compress file!\n");
		return 1;
	}

	return ret_code;
}
