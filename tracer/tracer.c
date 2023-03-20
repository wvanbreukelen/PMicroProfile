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

#define DEBUG
#define ND_CMD_TRACE 11
#define ND_CMD_TRACE_DISABLE 12

const int SAMPLE_RATE = 10; // 5 hz

volatile bool is_tracing = true;
pid_t exec_pid;
//#define ND_IOCTL_VENDOR _IOWR(ND_IOCTL, ND_CMD_TRACE, void)
//#define ND_IOCTL_VENDOR _IOWR(ND_IOCTL, ND_CMD_TRACE_DISABLE, void)


struct sample_thread_args {
	int sample_rate;
	int fd;
};

void enable_pmemtrace()
{
	#ifndef DEBUG
	ioctl(fd, ND_CMD_TRACE);
	#endif
	sleep(1);
}

void disable_pmemtrace() {
	// printf("Disabling pmemtrace...\n");
	
	

	#ifndef DEBUG
	ioctl(fd, ND_CMD_TRACE_DISABLE);
	#endif
}

void* pmem_sampler(void *arg)
{
	const struct sample_thread_args* thread_args = (const struct sample_thread_args* ) arg;

	const int period = 1000000 / thread_args->sample_rate;
	bool is_enabled = true;


	while (is_tracing) {
		usleep(period);
		//printf("Hello\n\f");
		
		if (is_enabled) {
			printf("Disable...\n\f");
			#ifndef DEBUG
			ioctl(thread_args->fd, ND_CMD_TRACE_DISABLE);
			#endif
		} else {
			printf("Enable...\n\f");
			#ifndef DEBUG
			ioctl(thread_args->fd, ND_CMD_TRACE_ENABLE);
			#endif
		}

		is_enabled = !(is_enabled);
	}

	#ifndef DEBUG
	ioctl(fd, ND_CMD_TRACE_DISABLE);
	#endif

	pthread_exit(NULL);
}


void signal_handler(int sig)
{
	if (sig == SIGINT) {
		is_tracing = false;
		//signal(SIGINT, SIG_DFL);
		//raise(SIGINT);

		//if (exec_pid) {
			//kill(exec_pid, SIGTERM);
		//}
	}
}

int main(int argc, char** argv)
{


	if (geteuid() != 0) {
		printf("This program must be run as root.\n");
		exit(EXIT_FAILURE);
	}

	if (argc < 2) {
		printf("Please provide a ndctl device handle, e.g. /dev/ndctl0\n");
		exit(EXIT_FAILURE);
	}

	if (argc < 3) {
		printf("Please provide the command to run\n");
		exit(EXIT_FAILURE);
	}

	int fd = open(argv[1], O_RDWR);
	if (fd < 0)
	{
		printf("Failed to open device %s!\n", argv[1]);
		exit(EXIT_FAILURE);
	}

	
	printf("Opened device, enabling pmemtrace...\n");
	struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask, SIGINT);
    sigaction(SIGINT, &sa, NULL);

	struct sample_thread_args thread_args = {SAMPLE_RATE, fd};
	
	enable_pmemtrace();

	pthread_t tid;

	if (pthread_create(&tid, NULL, pmem_sampler, (void*) &thread_args) < 0) {
		fprintf(stderr, "Error: pthread_create failed!\n");
		exit(EXIT_FAILURE);
	}

	pthread_detach(tid);
	pthread_join(tid, NULL);


	int ret_code, status;

	if ((exec_pid = fork()) < 0) {
		fprintf(stderr, "Error: fork error!\n");
		exit(EXIT_FAILURE);
	}

	if (exec_pid == 0) {
		ret_code = execvp(argv[2], &argv[2]);
	}

	waitpid(exec_pid, &status, 0);

	
	printf("Disabling pmemtrace...\n");

	pthread_cancel(tid);
	disable_pmemtrace();

	return ret_code;
}
