#include <iostream>
#include <hip/hip_runtime.h>

// __device__ void bloat(int *dstArr, int *srcArr1, int *srcArr2, size_t length) {
// #pragma unroll
//   for (int i = 0; i < 1000; ++i) {
//     asm volatile ("v_nop");
//   }
// }

__global__ void accumulate(int *dstArr, int *srcArr1, int *srcArr2,
                           size_t length) {
  std::uint64_t idx = hipThreadIdx_x;

  if (idx >= length)
    return;

  // dstArr[idx] = srcArr1[idx] + srcArr2[idx];
  dstArr[idx] = srcArr1[idx] + srcArr2[idx] + 10;

  // bloat(dstArr, srcArr1, srcArr2, length);
}

int main() {
  int *dstArr, *srcArr1, *srcArr2;
  const size_t length = 20;
  hipMallocManaged(&dstArr, length * sizeof(int));
  hipMallocManaged(&srcArr1, length * sizeof(int));
  hipMallocManaged(&srcArr2, length * sizeof(int));

  for (size_t i = 0; i < length; ++i) {
    srcArr1[i] = i;
    srcArr2[i] = i * 2;
    dstArr[i] = 0;
  }

  hipLaunchKernelGGL(accumulate, 32, 32, 0, 0, dstArr, srcArr1,
                     srcArr2, length);
  hipDeviceSynchronize();

  for (int i = 0; i < length; ++i) {
    std::cout << dstArr[i] << ' ';
  }
  std::cout << '\n';
}
