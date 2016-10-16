// Copyright Hugh Perkins 2016

// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0

// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "hostside_opencl_funcs.h"

#include "cocl_memory.h"

#include <iostream>
#include <memory>
#include <vector>
#include <map>
#include <set>

#include "EasyCL.h"

#include "CL/cl.h"

using namespace std;
using namespace easycl;

extern "C" {
    void hostside_opencl_funcs_assure_initialized(void);
}

namespace cocl {
    class LaunchConfiguration {
    public:
        size_t grid[3];
        size_t block[3];
        unique_ptr<CLKernel> kernel;
        CLQueue *queue = 0;  // NOT owned by us

        vector<cl_mem> kernelArgsToBeReleased;
        vector<Memory *> kernelArgsToBeRemapped;
    };
    LaunchConfiguration launchConfiguration;

    unique_ptr<EasyCL> cl;
    cl_context *ctx;
    cl_command_queue *queue;

    bool initialized = false;
}

using namespace cocl;

void hostside_opencl_funcs_init() {
    cout << "initialize cl context" << endl;
    cl.reset(EasyCL::createForFirstGpuOtherwiseCpu());
    ctx = cl->context;
    queue = cl->queue;
}

void hostside_opencl_funcs_assure_initialized(void) {
    // yes this is not threadsafe.  or anything safe really...
    if(!initialized) {
        hostside_opencl_funcs_init();
        initialized = true;
    }
}

extern "C" {
    size_t cudaSetDevice (int device);
    const char *cudaGetErrorString (size_t error);
    size_t cudaGetDevice (int *device);
    size_t cudaGetDeviceCount (int *count);
    size_t cudaGetLastError();
    size_t cudaConfigureCall(
        unsigned long long grid_xy, unsigned int grid_z,
        unsigned long long block_xy, unsigned int block_z, size_t sharedMem=0, void *stream=0);
    void configureKernel(
        const char *kernelName, const char *clSourcecodeString);

    /*
    cuDeviceCanAccessPeer
    cuOccupancyMaxActiveBlocksP
    */

    size_t cuInit(unsigned int flags);
    size_t cuDeviceGetCount(int *count);
    size_t cuDeviceGet (void *device, int ordinal);

}

size_t cuDeviceGet (void *device, int ordinal) {
    cout << "cuDeviceGet redirected" << endl;
    *(int *)device = 0;
    return 0;
}

size_t cuDeviceGetCount (int *count) {
    return cudaGetDeviceCount(count);
}

size_t cuInit(unsigned int flags) {
    cout << "redirected cuInit()" << endl;
    hostside_opencl_funcs_assure_initialized();
    return 0;
}

size_t cudaGetLastError() {
    cout << "cudaGetLastError" << endl;
    return 0;
}

const char *cudaGetErrorString (size_t error) {
    cout << "cudaGetErrorString error=" << error << endl;
    return "all was ok?";
}

size_t cudaGetDevice (int *device) {
    cout << "cudaGetDevice" << endl;
    *device = 0;
    return 0;
}

size_t cudaGetDeviceCount (int *count) {
    cout << "cudaGetDeviceCount" << endl;
    *count = 1;
    return 0;
}

size_t cudaSetDevice (int device) {
    cout << "cudaSetDevice stub device=" << device << endl;
    return 0;
}

size_t cudaConfigureCall(
        unsigned long long grid_xy, unsigned int grid_z,
        unsigned long long block_xy, unsigned int block_z, size_t sharedMem, void *queue_as_voidstar) {
    CLQueue *queue = (CLQueue *)queue_as_voidstar;
    cout << "cudaConfigureCall queue=" << queue << endl;
    if(sharedMem != 0) {
        cout << "cudaConfigureCall: Not implemented: non-zero shared memory" << endl;
        throw runtime_error("cudaConfigureCall: Not implemented: non-zero shared memory");
    }
    if(queue == 0) {
        queue = cl->default_queue;
        cout << "using default_queue " << queue << endl;
    }
    cout << "grid_xy " << grid_xy << " grid_z " << grid_z << endl;
    cout << "block_xy " << block_xy << " grid_z " << block_z << endl;
    int grid_x = grid_xy & ((1ul << 31) - 1);
    int grid_y = grid_xy >> 32;
    int block_x = block_xy & ((1ul << 31) - 1);
    int block_y = block_xy >> 32;
    cout << "grid(" << grid_x << ", " << grid_y << ", " << grid_z << ")" << endl;
    cout << "block(" << block_x << ", " << block_y << ", " << block_z << ")" << endl;
    launchConfiguration.queue = queue;
    launchConfiguration.grid[0] = grid_x;
    launchConfiguration.grid[1] = grid_y;
    launchConfiguration.grid[2] = grid_z;
    launchConfiguration.block[0] = block_x;
    launchConfiguration.block[1] = block_y;
    launchConfiguration.block[2] = block_z;
    return 0;
}

void configureKernel(
        const char *kernelName, const char *clSourcecodeString) {
    // cout << "configureKernel (name=" << kernelName << ", source=" << clSourcecodeString << ")" << endl;
    hostside_opencl_funcs_assure_initialized();
    launchConfiguration.kernel.reset(cl->buildKernelFromString(clSourcecodeString, kernelName, "", "__internal__"));
}

void setKernelArgStruct(char *pCpuStruct, int structAllocateSize) {
    // we're going to:
    // allocate a cl_mem for the struct
    // copy the cpu struct to the cl_mem
    // pass the cl_mem into the kernel

    // we should also:
    // deallocate the cl_mem after calling the kernel
    // (we assume hte struct is passed by-value, so we dont have to actually copy it back afterwards)
    cout << "setKernelArgStruct structsize=" << structAllocateSize << endl;
    // int idx = 
    cl_int err;
    cl_mem gpu_struct = clCreateBuffer(*ctx, CL_MEM_READ_WRITE, structAllocateSize,
                                           NULL, &err);
    cl->checkError(err);
    err = clEnqueueWriteBuffer(launchConfiguration.queue->queue, gpu_struct, CL_TRUE, 0,
                                      structAllocateSize, pCpuStruct, 0, NULL, NULL);
    cl->checkError(err);
    launchConfiguration.kernelArgsToBeReleased.push_back(gpu_struct);
    launchConfiguration.kernel->inout(&launchConfiguration.kernelArgsToBeReleased[launchConfiguration.kernelArgsToBeReleased.size() - 1]);
}

void setKernelArgFloatStar(float *hostpointer) {
    cout << "setKernelArgFloatStar " << hostpointer << endl;
    Memory *memory = getMemoryForHostPointer(hostpointer);
    cl_mem clmem = memory->clmem;

    if(memory->needsMap()) {
        cout << "setKernelArgFloatStar running unmap" << endl;
        memory->unmap();
        launchConfiguration.kernelArgsToBeRemapped.push_back(memory);
    }

    launchConfiguration.kernel->inout(&clmem);
}

void setKernelArgCharStar(char *hostpointer) {
    cout << "setKernelArgCharStar" << endl;
    Memory *pMemory = getMemoryForHostPointer(hostpointer);
    cl_mem clmem = pMemory->clmem;
    launchConfiguration.kernel->inout(&clmem);
}

// void setKernelArgCharStar(char *clmem_as_charstar) {
//     cout << "setKernelArgCharStar" << endl;
//     cl_mem *p_mem = (cl_mem *)clmem_as_charstar;
//     // cout << "setKernelArgFloatStar" << endl;
//     kernel->inout(p_mem);
// }

// void setKernelArgStruct() {
//     cout << "setKernelArgStruct" << endl;
//     // cl_mem *p_mem = (cl_mem *)clmem_as_charstar;
//     // cout << "setKernelArgFloatStar" << endl;
//     // kernel->inout(p_mem);
// }

void setKernelArgInt64(int64_t value) {
    cout << "setKernelArgInt64 " << value << endl;
    launchConfiguration.kernel->in(value);
}

void setKernelArgInt32(int value) {
    cout << "setKernelArgInt32 " << value << endl;
    launchConfiguration.kernel->in(value);
}

void setKernelArgFloat(float value) {
    cout << "setKernelArgFloat " << value << endl;
    launchConfiguration.kernel->in(value);
}

void kernelGo() {
    cout << "kernelGo " << endl;
    size_t global[3];
    for(int i = 0; i < 3; i++) {
        global[i] = launchConfiguration.grid[i] * launchConfiguration.block[i];
        cout << "global[" << i << "]=" << global[i] << endl;
    }
    for(int i = 0; i < 3; i++) {
        cout << "block[" << i << "]=" << launchConfiguration.block[i] << endl;
    }
    // cout << "launching kernel, using OpenCL..." << endl;
    launchConfiguration.kernel->run(launchConfiguration.queue, 3, global, launchConfiguration.block);
    cout << ".. kernel queued" << endl;
    // cl->finish();
    // cout << ".. kernel finished" << endl;
    for(auto it=launchConfiguration.kernelArgsToBeReleased.begin(); it != launchConfiguration.kernelArgsToBeReleased.end(); it++) {
        cout << "release arg" << endl;
        cl_mem memObject = *it;
        cl_int err = clReleaseMemObject(memObject);
        cl->checkError(err);
    }
    launchConfiguration.kernelArgsToBeReleased.clear();

    for(auto it=launchConfiguration.kernelArgsToBeRemapped.begin(); it != launchConfiguration.kernelArgsToBeRemapped.end(); it++) {
        Memory *pMemory = *it;
        pMemory->map();
    }
    launchConfiguration.kernelArgsToBeRemapped.clear();
}
