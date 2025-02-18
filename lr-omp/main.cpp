#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "linear.h"

double gettime() {
  struct timeval t;
  gettimeofday(&t, NULL);
  return t.tv_sec + t.tv_usec * 1e-6;
}

clock_t start;
clock_t end;

extern int cpu_offset;

/* Read file */
static void create_dataset(linear_param_t * params, data_t * dataset) {
  FILE *ptr_file = fopen(params->filename, "r");
  if (ptr_file == NULL) {
    perror("Failed to load dataset file");
    exit(1);
  }

  char *token;
  char buf[1024];

  for (int i = 0; i < params->size && fgets(buf, 1024, ptr_file) != NULL; i++) {
    token = strtok(buf, "\t");
    dataset[i].x = atof(token);
    token = strtok(NULL, "\t");
    dataset[i].y = atof(token);
  }

  fclose(ptr_file);
}

static void temperature_regression(results_t * results) {
  linear_param_t params;
  params.filename = TEMP_FILENAME;
  params.size = TEMP_SIZE;
  params.wg_size = TEMP_WORKGROUP_SIZE;
  params.wg_count = TEMP_WORKGROUP_NBR;

  data_t dataset[TEMP_SIZE];
  create_dataset(&params, dataset);

  results->parallelized.ktime = 0;

  parallelized_regression(&params, dataset, &results->parallelized);
  iterative_regression(&params, dataset, &results->iterative);
}

static void print_results(results_t * results) {
  PRINT_RESULT("Parallelized", results->parallelized);
  PRINT_RESULT("Iterative", results->iterative);
}

static void write_results(results_t * results, const char * restricts) {
  FILE* file = fopen(RESULT_FILENAME, restricts);
  WRITE_RESULT(file, results->parallelized);
  WRITE_RESULT(file, results->iterative);
  fclose(file);
}

int main(int argc, char* argv[]) {
  results_t results = {{0}};
  if (argc != 3) {
    printf("Usage: linear <num of loops> <cpu offset>\n");
    printf("Device execution only when cpu offset is 0\n");
    printf("Host execution only when cpu offset is 100\n");
    exit(0);
  }

  int loops = atoi(argv[1]);
  cpu_offset = atoi(argv[2]);

  double total_ktime = 0;
  double starttime = gettime();

  for (int i = 0; i < loops; i++) {
    temperature_regression(&results);
    //write_results(&results, "a");
    total_ktime += results.parallelized.ktime; // kernel time on a device
  }

  double endtime = gettime();
  printf("CPU offset: %d\n", cpu_offset);
  printf("Time: %lf ms\n", 1000.0 * (endtime - starttime));
  printf("Average kernel execution time: %lf us\n", (total_ktime * 1e-3) / loops);

  write_results(&results, "a");

  if (argc == 1 || strcmp(argv[1], "-no_print") > 0) {
    printf("\n> TEMPERATURE REGRESSION (%d)\n\n", TEMP_SIZE);
    print_results(&results);
  }

  return 0;
}
