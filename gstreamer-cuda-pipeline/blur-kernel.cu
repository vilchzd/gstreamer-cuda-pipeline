#include <iostream>   
#include <cstdlib>   
#include <iomanip>
#include <vector>
#include <cuda_runtime.h>
#include "blur-kernel.h"

using namespace std;

__global__ void gpu_blurBGR(unsigned char* input, unsigned char* output, int width, int height, int grid) {

    __shared__ unsigned char tile[(BLOCK_SIZE + 2 * GRID_SIZE) * (BLOCK_SIZE + 2 * GRID_SIZE) * 3];

    int y = threadIdx.y + BLOCK_SIZE * blockIdx.y; //global pixel positions
    int x = threadIdx.x + BLOCK_SIZE * blockIdx.x;

    int shared_x = threadIdx.x + grid; //map center pixel
    int shared_y = threadIdx.y + grid;
    int shared_width = BLOCK_SIZE + 2 * GRID_SIZE;

    if (x < width && y < height) {
        int in_index = (y * width + x) * 3;
        int sh_index = (shared_y * shared_width + shared_x) * 3;
        tile[sh_index + 0] = input[in_index + 0]; // B
        tile[sh_index + 1] = input[in_index + 1]; // G
        tile[sh_index + 2] = input[in_index + 2]; // R
    }

    for (int dy = -grid; dy <= grid; dy++) {
        for (int dx = -grid; dx <= grid; dx++)  {
            int gy = y + dy;
            int gx = x + dx;
            int sh_index = ((shared_y + dy) * shared_width + (shared_x + dx)) * 3;

            if (gx >= 0 && gx < width && gy >= 0 && gy < height) {
                int in_index = (gy * width + gx) * 3;
                tile[sh_index + 0] = input[in_index + 0];
                tile[sh_index + 1] = input[in_index + 1];
                tile[sh_index + 2] = input[in_index + 2];
            } else {
                tile[sh_index + 0] = 0; // zero padding
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
                if (blur_y >= 0 && blur_x >= 0 && blur_y < shared_width && blur_x < shared_width) {
                    int sh_index = (blur_y * shared_width + blur_x) * 3;
                    blur_sum_B += tile[sh_index + 0];
                    blur_sum_G += tile[sh_index + 1];
                    blur_sum_R += tile[sh_index + 2];
                    count++;
                }
            }
        }
        int out_index = (y * width + x) * 3;
        output[out_index + 0] = blur_sum_B / count;
        output[out_index + 1] = blur_sum_G / count;
        output[out_index + 2] = blur_sum_R / count;
    }  
}

void gpu_wrapper_blurBGR(unsigned char* h_input, unsigned char* h_output, unsigned char* d_input, unsigned char* d_output, int width, int height, size_t size, int grid) {

    //unsigned char *d_input, *d_output;
    //int size = width * height * sizeof(unsigned char) * 3;
    
    // cudaMalloc((void**)&d_input, size);
    // cudaMalloc((void**)&d_output, size);
    cudaMemcpy(d_input, h_input, size, cudaMemcpyHostToDevice);
    
    dim3 block_size(BLOCK_SIZE, BLOCK_SIZE);
    dim3 grid_size((width + BLOCK_SIZE - 1)/BLOCK_SIZE, (height + BLOCK_SIZE - 1)/BLOCK_SIZE);

    gpu_blurBGR<<<grid_size, block_size>>>(d_input, d_output, width, height, grid);

    //cudaDeviceSynchronize();
    
    cudaMemcpy(h_output, d_output, size, cudaMemcpyDeviceToHost);

    // cudaFree(d_input);
    // cudaFree(d_output);

}

/*
__global__ void gpu_blurGRAY(unsigned char* input, unsigned char* output, int width, int height, int grid) {

    __shared__ unsigned char tile[BLOCK_SIZE + 2 * GRID_SIZE][BLOCK_SIZE + 2 * GRID_SIZE];

    int y = threadIdx.y + BLOCK_SIZE * blockIdx.y; // Global pixel positions
    int x = threadIdx.x + BLOCK_SIZE * blockIdx.x;
    
    int shared_x = threadIdx.x + grid; // Maps center pixel
    int shared_y = threadIdx.y + grid;
    int shared_width = BLOCK_SIZE + 2 * GRID_SIZE;

    if (x < width && y < height) {
        tile[shared_y][shared_x] = input[y * width + x];
    }

    for (int dy = -grid; dy <= grid; dy++) {
        for (int dx = -grid; dx <= grid; dx++)  {
            int gy = y + dy;
            int gx = x + dx;
            if (gx >= 0 && gx < width && gy >= 0 && gy < height) {
                tile[shared_y + dy][shared_x + dx] = input[gy * width + gx];
            } else {
                tile[shared_y + dy][shared_x + dx] = 0; // Zero padding
            }
        }
    }

    __syncthreads();

    if (x < width && y < height) {
        int blur_sum = 0;
        int count = 0;
        for (int grid_y = -grid; grid_y <= grid; grid_y++) {
            for (int grid_x = -grid; grid_x <= grid; grid_x++) {
                int blur_y = shared_y + grid_y;
                int blur_x = shared_x + grid_x;
                if (blur_y >= 0 && blur_x >= 0 && blur_y < shared_width && blur_x < shared_width) {
                    blur_sum += tile[blur_y][blur_x];
                    count++;
                }
            }
        }
        output[y * width + x] = blur_sum / count;
    }  
}

void gpu_wrapper_blurGRAY(unsigned char* h_input, unsigned char* h_output, int width, int height, int grid) {

    unsigned char *d_input, *d_output;
    int size = width*height*sizeof(unsigned char);
    
    cudaMalloc((void**)&d_input, size);
    cudaMalloc((void**)&d_output, size);
    cudaMemcpy(d_input, h_input, size, cudaMemcpyHostToDevice);
    
    dim3 block_size(BLOCK_SIZE, BLOCK_SIZE);
    dim3 grid_size((width + BLOCK_SIZE - 1)/BLOCK_SIZE, (height + BLOCK_SIZE - 1)/BLOCK_SIZE);

    gpu_blurGRAY<<<grid_size, block_size>>>(d_input, d_output, width, height, grid);

    cudaDeviceSynchronize();
    
    cudaMemcpy(h_output, d_output, size, cudaMemcpyDeviceToHost);

    cudaFree(d_input);
    cudaFree(d_output);

}
*/


