# gstreamer-cuda-pipeline
A real-time GStreamer-based video pipeline with CUDA acceleration

# Dependencies

NVIDIA GPU with CUDA support\
CUDA 13.2\
GStreamer 1.28.1\
CMake 3.18+

# Build instructions 

For Windows:
```text
cmake -S . -B build -G "Visual Studio 17 2022" && cmake --build build --config Release && build\Release\main.exe
```

For Linux:
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc) && ./build/main
```

