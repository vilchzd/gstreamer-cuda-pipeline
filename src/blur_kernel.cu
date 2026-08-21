#include <cstdlib>   
#include <iomanip>
#include <iostream>
#include <vector>
#include <cuda_runtime.h>

#include "blur_kernel.h"
#include "globals.h"
#include "callbacks.h"

using namespace std;

__global__ void gpu_blurBGR(unsigned char* input, unsigned char* output, int width, int height, int grid) {

    extern __shared__ unsigned char tile[];

    int y = threadIdx.y + BLOCK_SIZE * blockIdx.y; //global pixel positions
    int x = threadIdx.x + BLOCK_SIZE * blockIdx.x;

    int shared_x = threadIdx.x + grid;
    int shared_y = threadIdx.y + grid;
    int shared_width = BLOCK_SIZE + 2 * grid; //tile width

    int block_x = threadIdx.x;
    int block_y = threadIdx.y;

    for (int tile_y = block_y; tile_y < shared_width; tile_y += BLOCK_SIZE) {
        for (int tile_x = block_x; tile_x < shared_width; tile_x += BLOCK_SIZE) {

            int gx = blockIdx.x * BLOCK_SIZE + tile_x - grid;
            int gy = blockIdx.y * BLOCK_SIZE + tile_y - grid;

            int sh_index = (tile_y * shared_width + tile_x) * 3;

            if (gx >= 0 && gx < width && gy >= 0 && gy < height) {

                int in_index = (gy * width + gx) * 3;

                tile[sh_index + 0] = input[in_index + 0];
                tile[sh_index + 1] = input[in_index + 1];
                tile[sh_index + 2] = input[in_index + 2];

            } else {
                
                tile[sh_index + 0] = 0;
                tile[sh_index + 1] = 0;
                tile[sh_index + 2] = 0;

            }
        }
    }

    __syncthreads();

    if (x < width && y < height) {

        int blur_sum_B = 0;
        int blur_sum_G = 0;
        int blur_sum_R = 0;
        int count = 0;

        for (int grid_y = -grid; grid_y <= grid; grid_y++) {
            for (int grid_x = -grid; grid_x <= grid; grid_x++) {

                int blur_y = shared_y + grid_y;
                int blur_x = shared_x + grid_x;

                int sh_index = (blur_y * shared_width + blur_x) * 3;

                blur_sum_B += tile[sh_index + 0];
                blur_sum_G += tile[sh_index + 1];
                blur_sum_R += tile[sh_index + 2];
                count++;
                
            }
        }

        int out_index = (y * width + x) * 3;
        output[out_index + 0] = blur_sum_B / count;
        output[out_index + 1] = blur_sum_G / count;
        output[out_index + 2] = blur_sum_R / count;
    }
}

void gpu_wrapper_blurBGR(unsigned char* h_input, unsigned char* h_output, unsigned char* d_input, unsigned char* d_output, int width, int height, size_t size, int grid) {

    cudaMemcpy(d_input, h_input, size, cudaMemcpyHostToDevice);
    
    dim3 block_size(BLOCK_SIZE, BLOCK_SIZE); //threads per block
    dim3 grid_size((width + BLOCK_SIZE - 1)/BLOCK_SIZE, (height + BLOCK_SIZE - 1)/BLOCK_SIZE); //blocks covering inpt image
    size_t sharedMem_size = (BLOCK_SIZE + 2 * grid) * (BLOCK_SIZE + 2 * grid) * 3; //tile_size

    cudaEvent_t start, stop;
    cudaEventCreate(&start);
    cudaEventCreate(&stop);

    cudaEventRecord(start);

    gpu_blurBGR<<<grid_size, block_size, sharedMem_size>>>(d_input, d_output, width, height, grid);

    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        std::cerr << "CUDA kernel launch error: " << cudaGetErrorString(err) << std::endl;
    }

    cudaEventRecord(stop);

    err = cudaEventSynchronize(stop);
    if (err != cudaSuccess) {
        std::cerr << "CUDA kernel execution error: " << cudaGetErrorString(err) << std::endl;
    }

    cudaEventElapsedTime(&kernel_time, start, stop);

    cudaEventDestroy(start);
    cudaEventDestroy(stop);

    err = cudaMemcpy(h_output, d_output, size, cudaMemcpyDeviceToHost);
    if (err != cudaSuccess) {
        std::cerr << "CUDA COPY ERROR: " << cudaGetErrorString(err) << std::endl;
    }
}


