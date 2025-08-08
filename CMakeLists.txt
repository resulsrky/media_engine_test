cmake_minimum_required(VERSION 3.10)
project(NovaEngineV2)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# GStreamer paketleri
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED 
    gstreamer-1.0
    gstreamer-base-1.0
    gstreamer-video-1.0
    gstreamer-rtp-1.0
    gstreamer-app-1.0
)

# GStreamer optimize compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -mtune=native")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math -funroll-loops -ftree-vectorize")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GSTREAMER_OPTIMIZE")

# Executable
add_executable(nova_camera
    main.cpp
    nova_camera.cpp
    video_manager.cpp
    pipeline_builder.cpp
    gpu_detector.cpp
    heartbeat_manager.cpp
)

# Include directories
target_include_directories(nova_camera PRIVATE ${GST_INCLUDE_DIRS})

# Link libraries
target_link_libraries(nova_camera ${GST_LIBRARIES})

# GStreamer optimize link flags
target_link_options(nova_camera PRIVATE ${GST_LDFLAGS})

# Reassembly Engine Executable
add_executable(reassembly_test
    reassembly_test.cpp
    reassembly_engine.cpp
)

# Include directories for reassembly
target_include_directories(reassembly_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Link libraries for reassembly (threading support)
target_link_libraries(reassembly_test pthread)
