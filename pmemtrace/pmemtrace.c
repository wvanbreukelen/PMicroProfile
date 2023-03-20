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

//#define DEBUG
#define ND_CMD_TRACE_ENABLE 11
#define ND_CMD_TRACE_DISABLE 12

#define CURRENT_TRACER "/sys/kernel/debug/tracing/current_tracer"

#define TRACER_BUF_SIZE 128000
#define TRACER_BUF_SIZE_PATH "/sys/kernel/debug/tracing/buffer_size_kb"
#define TRACER_OUTPUT_PIPE "/sys/kernel/debug/tracing/trace_pipe"


#define ENABLE_SAMPLING
const unsigned int SAMPLE_RATE = 120; // 10 hz
const unsigned int DUTY_CYCLE = 32;

bool is_tracing = true;
pid_t exec_pid;
//#define ND_IOCTL_VENDOR _IOWR(ND_IOCTL, ND_CMD_TRACE, void)
//#define ND_IOCTL_VENDOR _IOWR(ND_IOCTL, ND_CMD_TRACE_DISABLE, void)


struct sample_thread_args {
	unsigned int sample_rate;
	unsigned int duty_cycle;
	int fd;
};

struct read_thread_args {
	const char *output_file;
};


bool is_mmiotrace_enabled() {
	char tracer[256];
	FILE *fp = fopen(CURRENT_TRACER, "r");
	if (fp == NULL) {
		perror("Error opening current_tracer file");
		exit(EXIT_FAILURE);
	}
	fgets(tracer, sizeof(tracer), fp);
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

	printf("Writing: %s\n", ((enable) ? "mmiotrace" : "nop"));

	fprintf(fp, ((enable) ? "mmiotrace" : "nop"));

	fclose(fp);
}

void enable_pmemtrace(int fd)
{
	#ifndef DEBUG
	ioctl(fd, ND_CMD_TRACE_ENABLE);
	#endif
	//sleep(1);
}

void disable_pmemtrace(int fd) {
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

	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);	

	printf("Thread running...\n");

	while (is_tracing) {
		//printf("Hello\n\f");
		pthread_testcancel();
		
		if (is_enabled) {
			usleep(period * thread_args->duty_cycle);
			//printf("Disable...\n\f");
			#ifndef DEBUG
			ioctl(thread_args->fd, ND_CMD_TRACE_DISABLE);
			#endif
		} else {
			usleep(period);
			//printf("Enable...\n\f");
			#ifndef DEBUG
			ioctl(thread_args->fd, ND_CMD_TRACE_ENABLE);
			#endif
		}

		is_enabled = !(is_enabled);
	}

	#ifndef DEBUG
	ioctl(thread_args->fd, ND_CMD_TRACE_DISABLE);
	#endif

	printf("Stopping pmem_sampler...\n");

	pthread_exit(NULL);
}

void* pmemtrace_output_thread(void *arg)
{
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
	pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);	

	const struct read_thread_args* thread_args = (const struct read_thread_args* ) arg;

	FILE *in = fopen(TRACER_OUTPUT_PIPE, "r");
	FILE *out = fopen(thread_args->output_file, "w");

	if (!in || !out) {
		perror("Failed to open file!\n");
		exit(EXIT_FAILURE);
	}

	char buffer[1024];
	size_t n;

	while (is_tracing) {
		while ((n = fread(buffer, 1, sizeof(buffer), in)) > 0) {
			fwrite(buffer, 1, n, out);
		}
	}

	// Make sure all data is flushed.
	while ((n = fread(buffer, 1, sizeof(buffer), in)) > 0) {
		fwrite(buffer, 1, n, out);
	}

	fclose(in);
	fclose(out);

	printf("Stopping pmemtrace_output_thread...\n");


	pthread_exit(NULL);
}

void signal_handler(int sig)
{
	if (sig == SIGINT) {
		is_tracing = false;
		putchar('\n');
		//signal(SIGINT, SIG_DFL);
		//raise(SIGINT);

		if (exec_pid) {
			kill(exec_pid, SIGTERM);
		}
	}
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

	sleep(1);

	pthread_t tid_rd;

	struct read_thread_args rd_thread_args = {"trace_dump.log"};

	if (pthread_create(&tid_rd, NULL, pmemtrace_output_thread, (void*) &rd_thread_args) < 0) {
		fprintf(stderr, "Error: pthread_create failed!\n");
		exit(EXIT_FAILURE);
	}

	pthread_detach(tid_rd);

	#ifdef ENABLE_SAMPLING
	pthread_t tid_smpl;
	struct sample_thread_args smpl_thread_args = {SAMPLE_RATE, DUTY_CYCLE, fd};

	if (pthread_create(&tid_smpl, NULL, pmem_sampler, (void*) &smpl_thread_args) < 0) {
		fprintf(stderr, "Error: pthread_create failed!\n");
		exit(EXIT_FAILURE);
	}

	pthread_detach(tid_smpl);
	
	#endif



	int ret_code, status;

	if ((exec_pid = fork()) < 0) {
		fprintf(stderr, "Error: fork error!\n");
		exit(EXIT_FAILURE);
	}

	if (exec_pid == 0) {
		ret_code = execvp(argv[2], &argv[2]);
	} else {
		waitpid(exec_pid, &status, 0);
	}

	#ifdef ENABLE_SAMPLING
	pthread_join(tid_smpl, NULL);
	#endif
	pthread_join(tid_rd, NULL);

	//printf("PIDs: %ld %ld %ld\n", tid_rd, tid_smpl, exec_pid);

	is_tracing = false;


	
	printf("Disabling pmemtrace...\n");

	disable_pmemtrace(fd);

	close(fd);
	
	sleep(4);
	toggle_mmiotrace(false);

	return ret_code;
}