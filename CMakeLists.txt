cmake_minimum_required(VERSION 3.18)

project(gst_cuda_pipeline LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CUDA_STANDARD 17)

# Use dynamic MSVC runtime (/MD)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

find_package(CUDAToolkit REQUIRED)

set(GSTREAMER_ROOT "C:/Program Files/gstreamer/1.0/msvc_x86_64")

set(GSTREAMER_INCLUDE_DIRS
    ${GSTREAMER_ROOT}/include/gstreamer-1.0
    ${GSTREAMER_ROOT}/include/glib-2.0
    ${GSTREAMER_ROOT}/lib/glib-2.0/include
)

set(GSTREAMER_LIBRARY_DIRS
    ${GSTREAMER_ROOT}/lib
)

set(GSTREAMER_LIBRARIES
    ${GSTREAMER_LIBRARY_DIRS}/gstreamer-1.0.lib
    ${GSTREAMER_LIBRARY_DIRS}/gstapp-1.0.lib
    ${GSTREAMER_LIBRARY_DIRS}/glib-2.0.lib
    ${GSTREAMER_LIBRARY_DIRS}/gobject-2.0.lib
    ${GSTREAMER_LIBRARY_DIRS}/gmodule-2.0.lib
    ${GSTREAMER_LIBRARY_DIRS}/gthread-2.0.lib
)

add_executable(gst_cuda_pipeline
    src/gst-cuda_pipeline.cpp
    src/blur_kernel.cu
    src/callbacks.cpp
)

target_include_directories(gst_cuda_pipeline PRIVATE
    ${GSTREAMER_INCLUDE_DIRS}
    src/headers
)

set_property(TARGET gst_cuda_pipeline PROPERTY CUDA_SEPARABLE_COMPILATION ON)
set_property(TARGET gst_cuda_pipeline PROPERTY CUDA_RUNTIME_LIBRARY Hybrid)
# Ensure NVCC uses /MD 
target_compile_options(gst_cuda_pipeline PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=/MD>)

target_link_libraries(gst_cuda_pipeline
    ${GSTREAMER_LIBRARIES}
    CUDA::cudart
)