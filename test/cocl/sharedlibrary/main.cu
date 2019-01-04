#include "mylib.h"

#include <iostream>
#include <memory>
#include <cassert>

using namespace std;

int main(int argc, char *argv[]) {
    int N = 1024;

    CUstream stream;
    cuStreamCreate(&stream, 0);

    float *hostFloats1;
    cuMemHostAlloc((void **)&hostFloats1, N * sizeof(float), CU_MEMHOSTALLOC_PORTABLE);

    CUdeviceptr deviceFloats1;
    cuMemAlloc(&deviceFloats1, N * sizeof(float));

    hostFloats1[0] = 123.456f;

    cuMemcpyHtoDAsync(
        (CUdeviceptr)(((float *)deviceFloats1)),
        hostFloats1,
        N * sizeof(float),
        stream
    );

    __global__ void mykernel(float *data);
    mykernel<<<dim3(1,1,1), dim3(32,1,1), 0, stream>>>(((float *)deviceFloats1));

    cuMemcpyDtoHAsync(hostFloats1, deviceFloats1, N * sizeof(float), stream);
    cuStreamSynchronize(stream);

    cout << hostFloats1[0] << endl;

    assert(hostFloats1[0] == 126.456f);

    cuMemFreeHost(hostFloats1);
    cuMemFree(deviceFloats1);
    cuStreamDestroy(stream);

    return 0;
}
