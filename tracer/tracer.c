#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <string.h>

#define ND_CMD_TRACE 11

#define ND_IOCTL_VENDOR _IOWR(ND_IOCTL, ND_CMD_TRACE, void)

int main(int argc, char** argv)
{
	if (geteuid() != 0) {
        	printf("This program must be run as root.\n");
       	 	exit(EXIT_FAILURE);
    	}

	if (argc < 2) {
		printf("Please provide a ndctl device handle, e.g. /dev/ndctl0\n");
		return 1;
	}

	int fd = open(argv[1], O_RDWR);
	if (fd < 0)
	{
		printf("Failed to open device %s!\n", argv[1]);
		return 1;
	}
	
	printf("Opened device, enabling pmemtrace...\n");
	
	ioctl(fd, ND_CMD_TRACE);

	printf("Tracer enabled!\n");

	close(fd);
}
