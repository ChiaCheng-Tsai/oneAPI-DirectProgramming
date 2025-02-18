/**
 * Copyright 2015 NVIDIA Corporation.  All rights reserved.
 *
 * Please refer to the NVIDIA end user license agreement (EULA) associated
 * with this source code for terms and conditions that govern your use of
 * this software. Any use, reproduction, disclosure, or distribution of
 * this software and related documentation outside the terms of the EULA
 * is strictly prohibited.
 *
 */
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <chrono>
#include "common.h"

#define NUM_OF_BLOCKS 1024
#define NUM_OF_THREADS 128

inline
void reduceInShared_native(half2 *const v, nd_item<1> &item)
{
  int lid = item.get_local_id(0);
  if(lid<64) v[lid] = v[lid] + v[lid+64];
  item.barrier(access::fence_space::local_space);
  if(lid<32) v[lid] = v[lid] + v[lid+32];
  item.barrier(access::fence_space::local_space);
  if(lid<32) v[lid] = v[lid] + v[lid+16];
  item.barrier(access::fence_space::local_space);
  if(lid<32) v[lid] = v[lid] + v[lid+8];
  item.barrier(access::fence_space::local_space);
  if(lid<32) v[lid] = v[lid] + v[lid+4];
  item.barrier(access::fence_space::local_space);
  if(lid<32) v[lid] = v[lid] + v[lid+2];
  item.barrier(access::fence_space::local_space);
  if(lid<32) v[lid] = v[lid] + v[lid+1];
  item.barrier(access::fence_space::local_space);
}

void scalarProductKernel_native(const half2 *a,
                                const half2 *b,
                                float *results, 
                                      half2 *shArray,
                                const size_t size,
                                nd_item<1> item)
{
  int lid = item.get_local_id(0);
  int gid = item.get_group(0); 

  const int stride = item.get_group_range(0) * item.get_local_range(0);

  half2 value(0.f, 0.f);
  shArray[lid] = value;

  for (int i = item.get_global_id(0); i < size; i += stride)
  {
    value = a[i] * b[i] + value;
  }

  shArray[lid] = value;
  item.barrier(access::fence_space::local_space);
  reduceInShared_native(shArray, item);

  if (lid == 0)
  {
    half2 result = shArray[0];
    float f_result = (float)result.y() + (float)result.x();
    results[gid] = f_result;
  }
}

void generateInput(half2 *a, size_t size)
{
  for (size_t i = 0; i < size; ++i)
  {
    half2 temp;
    temp.x() = static_cast<float>(rand() % 4);
    temp.y() = static_cast<float>(rand() % 2);
    a[i] = temp;
  }
}

int main(int argc, char *argv[])
{
  if (argc != 2) {
    printf("Usage: %s <repeat>\n", argv[0]);
    return 1;
  }
  const int repeat = atoi(argv[1]);

  size_t size = NUM_OF_BLOCKS*NUM_OF_THREADS*16;

#ifdef USE_GPU
  gpu_selector dev_sel;
#else
  cpu_selector dev_sel;
#endif
  queue q(dev_sel);

  half2 *a = (half2 *)malloc(size * sizeof(half2));
  half2 *b = (half2 *)malloc(size * sizeof(half2));
  float *r = (float*) malloc (NUM_OF_BLOCKS*sizeof(float));
  buffer<float, 1> d_r (NUM_OF_BLOCKS);

  srand(123); 
  generateInput(a, size);
  buffer<half2, 1> d_a (a, size);

  generateInput(b, size);
  buffer<half2, 1> d_b (b, size);

  range<1> gws (NUM_OF_BLOCKS * NUM_OF_THREADS);
  range<1> lws (NUM_OF_THREADS);

  // warmup
  for (int i = 0; i < repeat; i++) {
    q.submit([&](handler &cgh) {
      auto a = d_a.get_access<sycl_read>(cgh); 
      auto b = d_b.get_access<sycl_read>(cgh); 
      auto r = d_r.get_access<sycl_discard_write>(cgh); 
      accessor<half2, 1, sycl_read_write, sycl_lmem> shArray(NUM_OF_THREADS, cgh);
      cgh.parallel_for<class warm_sp2>(nd_range<1>(gws, lws), [=](nd_item<1> item) {
        scalarProductKernel_native(
          a.get_pointer(),
          b.get_pointer(),
          r.get_pointer(),
          shArray.get_pointer(),
          size, item);
      });
    });
  }
  q.wait();
  auto start = std::chrono::steady_clock::now();

  for (int i = 0; i < repeat; i++) {
    q.submit([&](handler &cgh) {
      auto a = d_a.get_access<sycl_read>(cgh); 
      auto b = d_b.get_access<sycl_read>(cgh); 
      auto r = d_r.get_access<sycl_discard_write>(cgh); 
      accessor<half2, 1, sycl_read_write, sycl_lmem> shArray(NUM_OF_THREADS, cgh);
      cgh.parallel_for<class sp2>(nd_range<1>(gws, lws), [=](nd_item<1> item) {
        scalarProductKernel_native(
          a.get_pointer(),
          b.get_pointer(),
          r.get_pointer(),
          shArray.get_pointer(),
          size, item);
      });
    });
  }

  q.wait();
  auto end = std::chrono::steady_clock::now();
  auto time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
  printf("Average kernel execution time %f (us)\n", (time * 1e-3f) / repeat);

  q.submit([&](handler &cgh) {
    auto acc = d_r.get_access<sycl_read>(cgh); 
    cgh.copy(acc, r);
  }).wait();

  float result_native = 0;
  for (int i = 0; i < NUM_OF_BLOCKS; ++i)
  {
    result_native += r[i];
  }
  printf("Result native operators\t: %f \n", result_native);

  float result_reference = 5241674.f;

  printf("fp16ScalarProduct %s\n", (fabs(result_reference - result_native) < 0.00001) ? 
         "PASS" : "FAIL");

  free(a);
  free(b);
  free(r);

  return EXIT_SUCCESS;
}
