cmake_minimum_required(VERSION 3.18)
project(gst_cuda_pipeline LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)

# --- GStreamer Windows paths ---
set(GSTREAMER_ROOT "C:/Program Files/gstreamer/1.0/msvc_x86_64")
set(GSTREAMER_INCLUDE_DIRS
    ${GSTREAMER_ROOT}/include/gstreamer-1.0
    ${GSTREAMER_ROOT}/include/glib-2.0
    ${GSTREAMER_ROOT}/lib/glib-2.0/include
)
set(GSTREAMER_LIBRARY_DIRS ${GSTREAMER_ROOT}/lib)
set(GSTREAMER_LIBRARIES
    ${GSTREAMER_LIBRARY_DIRS}/gstreamer-1.0.lib
    ${GSTREAMER_LIBRARY_DIRS}/gstapp-1.0.lib
    ${GSTREAMER_LIBRARY_DIRS}/glib-2.0.lib
    ${GSTREAMER_LIBRARY_DIRS}/gobject-2.0.lib
    ${GSTREAMER_LIBRARY_DIRS}/gmodule-2.0.lib
    ${GSTREAMER_LIBRARY_DIRS}/gthread-2.0.lib
)

include_directories(
    ${GSTREAMER_INCLUDE_DIRS}
    src/headers
)
link_directories(${GSTREAMER_LIBRARY_DIRS})

add_executable(gst_cuda_pipeline
    src/gst-cuda_pipeline.cpp
    src/blur_kernel.cu
    src/callbacks.cpp
)

target_link_libraries(gst_cuda_pipeline
    ${GSTREAMER_LIBRARIES}
)

set_target_properties(gst_cuda_pipeline PROPERTIES
    CUDA_SEPARABLE_COMPILATION ON
)