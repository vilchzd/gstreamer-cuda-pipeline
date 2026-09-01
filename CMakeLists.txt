cmake_minimum_required(VERSION 3.18)

project(gst_cuda_pipeline LANGUAGES CXX CUDA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_ARCHITECTURES 86 89) #Antes set(CMAKE_CUDA_ARCHITECTURES native)

find_package(CUDAToolkit REQUIRED)

if(WIN32)

    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")

    set(GSTREAMER_ROOT "C:/Program Files/gstreamer/1.0/msvc_x86_64")

    set(GSTREAMER_INCLUDE_DIRS
        ${GSTREAMER_ROOT}/include/gstreamer-1.0
        ${GSTREAMER_ROOT}/include/glib-2.0
        ${GSTREAMER_ROOT}/lib/glib-2.0/include
    )

    set(GSTREAMER_LIBRARIES
        ${GSTREAMER_ROOT}/lib/gstreamer-1.0.lib
        ${GSTREAMER_ROOT}/lib/gstapp-1.0.lib
        ${GSTREAMER_ROOT}/lib/glib-2.0.lib
        ${GSTREAMER_ROOT}/lib/gobject-2.0.lib
        ${GSTREAMER_ROOT}/lib/gmodule-2.0.lib
        ${GSTREAMER_ROOT}/lib/gthread-2.0.lib
    )

elseif(UNIX)

    find_package(PkgConfig REQUIRED)

    pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
    pkg_check_modules(GSTREAMER_APP REQUIRED gstreamer-app-1.0)

    set(GSTREAMER_INCLUDE_DIRS
        ${GSTREAMER_INCLUDE_DIRS}
        ${GSTREAMER_APP_INCLUDE_DIRS}
    )

    set(GSTREAMER_LIBRARIES
        ${GSTREAMER_LIBRARIES}
        ${GSTREAMER_APP_LIBRARIES}
    )

endif()

add_executable(main
    src/main.cpp
    src/blur_kernel.cu
    src/callbacks.cpp
)

target_include_directories(main PRIVATE
    ${GSTREAMER_INCLUDE_DIRS}
    src/headers
)

set_property(TARGET main PROPERTY CUDA_SEPARABLE_COMPILATION ON)
set_property(TARGET main PROPERTY CUDA_RUNTIME_LIBRARY Hybrid)

if(WIN32)
    target_compile_options(main PRIVATE $<$<COMPILE_LANGUAGE:CUDA>:-Xcompiler=/MD>)
endif()

target_link_libraries(main
    ${GSTREAMER_LIBRARIES}
    CUDA::cudart
)