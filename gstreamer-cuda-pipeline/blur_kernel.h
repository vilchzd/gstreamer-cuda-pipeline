#ifndef BLUR_KERNEL_H
#define BLUR_KERNEL_H

#include <string>
using namespace std;

#define TARGET_CHANNELS 3  // Desired image output use 1 for GREYSCALE or 3 for BGR  
#define BLOCK_SIZE 8      // Threads per block (MAX 32 for 2D block)
#define GRID_SIZE 9       // Grid size (MAX_GRID_SIZE = 48)  15 max for 30fps

//GPU Funtions
void gpu_wrapper_blurBGR(unsigned char* h_input, unsigned char*, unsigned char* d_input, unsigned char* d_output, int width, int height, size_t size, int grid);
//void gpu_wrapper_blurGRAY(unsigned char* h_input, unsigned char* h_output, int width, int height, int grid);

#endif 
