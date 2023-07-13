#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#define PMEM_SIZE (size_t) (4096L * 1024 * 1024) // 1MB PMEM region

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <dax_path>\n", argv[0]);
        return 1;
    }

    // Open the PMEM device file
    int fd = open(argv[1], O_RDWR);
    if (fd == -1) {
        perror("Failed to open PMEM device");
        return 1;
    }

    printf("Make sure your disabled Optane DIMM interleaving for per-DIMM performance measurements!\n");

    // Allocate a PMEM region using mmap
    char* pmem_addr = (char*)  mmap(NULL, PMEM_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (pmem_addr == MAP_FAILED) {
        perror("Failed to mmap PMEM");
        close(fd);
        return 1;
    }

    // Generate random data for write workload
    srand(time(NULL));
    uint8_t* data = (uint8_t*)malloc(PMEM_SIZE);
    for (size_t i = 0; i < PMEM_SIZE; i++) {
        data[i] = rand() % 256;
    }

    // Measure read and write bandwidth
    clock_t start, end;
    double elapsed_time;

    start = clock();
    //for (size_t i = 0; i < PMEM_SIZE; i++) {
        //data[i] = ((char*) pmem_addr)[i]; // Read from PMEM
    //}
    memcpy((char*) pmem_addr, data, PMEM_SIZE);
    end = clock();
    elapsed_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Read bandwidth: %.2f MB/s\n", (PMEM_SIZE / (1024.0 * 1024.0)) / elapsed_time);

    start = clock();
    //for (size_t i = 0; i < PMEM_SIZE; i++) {
        //((char*) pmem_addr)[i] = data[i]; // Write to PMEM
    //}
    memcpy(data, (char*) pmem_addr, PMEM_SIZE);
    end = clock();
    elapsed_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Write bandwidth: %.2f MB/s\n", (PMEM_SIZE / (1024.0 * 1024.0)) / elapsed_time);

    // Clean up
    munmap(pmem_addr, PMEM_SIZE);
    free(data);
    close(fd);

    return 0;
}

