#include "../buffer_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>

#define BENCH_FILE_SIZE (10 * 1024 * 1024)
#define TEMP_FILENAME "temp_bench_file.dat"

int main() {
    printf("Creating %d MB test file...\n", BENCH_FILE_SIZE / 1024 / 1024);
    FILE *f = fopen(TEMP_FILENAME, "wb");
    if (!f) { perror("fopen"); return 1; }
    unsigned char *dummy = malloc(BENCH_FILE_SIZE);
    if (!dummy) { perror("malloc"); fclose(f); return 1; }
    // Fill with random-ish data
    for(size_t i=0; i<BENCH_FILE_SIZE; i++) dummy[i] = (unsigned char)(i % 256);
    if (fwrite(dummy, 1, BENCH_FILE_SIZE, f) != BENCH_FILE_SIZE) {
        perror("fwrite"); free(dummy); fclose(f); return 1;
    }
    free(dummy);
    fclose(f);

    printf("Benchmarking read_file_to_buffer + base64_encode...\n");

    clock_t start = clock();

    size_t file_size = 0;
    unsigned char *buffer = read_file_to_buffer(TEMP_FILENAME, &file_size);
    if (!buffer) { fprintf(stderr, "read_file failed\n"); remove(TEMP_FILENAME); return 1; }

    char *b64 = base64_encode(buffer, file_size);
    if (!b64) { fprintf(stderr, "base64 failed\n"); free(buffer); remove(TEMP_FILENAME); return 1; }

    clock_t end = clock();
    double cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;

    printf("Time taken: %f seconds\n", cpu_time_used);

    free(buffer);
    free(b64);
    remove(TEMP_FILENAME);
    return 0;
}
