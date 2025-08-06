cmake_minimum_required(VERSION 3.10)
project(NovaEngineV2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# GStreamer paketlerini bul
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-base-1.0 gstreamer-video-1.0 gstreamer-rtp-1.0)

# OpenMP desteği
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()

# Executable oluştur
add_executable(nova_camera 
    main.cpp
    nova_camera.cpp
    video_manager.cpp
    pipeline_builder.cpp
    gpu_detector.cpp
    heartbeat_manager.cpp
)

# Include dizinleri
target_include_directories(nova_camera PRIVATE ${GST_INCLUDE_DIRS})

# Link kütüphaneleri
target_link_libraries(nova_camera ${GST_LIBRARIES})

# Compiler flags
target_compile_options(nova_camera PRIVATE 
    ${GST_CFLAGS_OTHER}
    -march=native
    -mtune=native
    -O3
    -ffast-math
    -funroll-loops
    -ftree-vectorize
)

# Linker flags
target_link_options(nova_camera PRIVATE ${GST_LDFLAGS_OTHER})
