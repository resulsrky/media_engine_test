cmake_minimum_required(VERSION 3.16)
project(MultiPortUDPStreaming VERSION 1.0.0 LANGUAGES CXX)

# C++17 standardını kullan
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Derleme optimizasyonları
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -march=native -mtune=native")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GLIBCXX_USE_CXX11_ABI=1")

# Include dizinleri
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/external)

# Asio kütüphanesini bul (header-only)
find_package(PkgConfig REQUIRED)
find_path(ASIO_INCLUDE_DIR asio.hpp
    PATHS
    ${CMAKE_CURRENT_SOURCE_DIR}/external/asio/asio/include
    ${CMAKE_CURRENT_SOURCE_DIR}/external/asio
    /usr/include
    /usr/local/include
    /opt/local/include
)

if(NOT ASIO_INCLUDE_DIR)
    message(FATAL_ERROR "Asio kütüphanesi bulunamadı!")
endif()

include_directories(${ASIO_INCLUDE_DIR})

# FFmpeg kütüphanelerini bul
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED
    libavcodec
    libavformat
    libavutil
    libswscale
)

# GStreamer kütüphanelerini bul
pkg_check_modules(GST REQUIRED
    gstreamer-1.0
    gstreamer-base-1.0
    gstreamer-video-1.0
    gstreamer-rtp-1.0
    gstreamer-app-1.0
)

# Threading desteği
find_package(Threads REQUIRED)

# Common kütüphanesi (header-only)
add_library(common INTERFACE)
target_include_directories(common INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/include/common
)

# Video Sender executable
add_executable(video_sender
    src/sender/main.cpp
    src/sender/video_sender.cpp
)

target_link_libraries(video_sender
    common
    ${FFMPEG_LIBRARIES}
    ${GST_LIBRARIES}
    Threads::Threads
)

target_include_directories(video_sender PRIVATE
    ${FFMPEG_INCLUDE_DIRS}
    ${GST_INCLUDE_DIRS}
)

target_compile_options(video_sender PRIVATE ${FFMPEG_CFLAGS_OTHER} ${GST_CFLAGS_OTHER})

# Video Receiver executable
add_executable(video_receiver
    src/receiver/main.cpp
    src/receiver/video_receiver.cpp
)

target_link_libraries(video_receiver
    common
    ${FFMPEG_LIBRARIES}
    ${GST_LIBRARIES}
    Threads::Threads
)

target_include_directories(video_receiver PRIVATE
    ${FFMPEG_INCLUDE_DIRS}
    ${GST_INCLUDE_DIRS}
)

target_compile_options(video_receiver PRIVATE ${FFMPEG_CFLAGS_OTHER} ${GST_CFLAGS_OTHER})

# Test executable
add_executable(test_sender
    examples/test_sender.cpp
)

target_link_libraries(test_sender
    common
    Threads::Threads
)

add_executable(test_receiver
    examples/test_receiver.cpp
)

target_link_libraries(test_receiver
    common
    Threads::Threads
)

# Install hedefleri
install(TARGETS video_sender video_receiver
    RUNTIME DESTINATION bin
)

# Debug bilgileri
message(STATUS "Asio include dir: ${ASIO_INCLUDE_DIR}")
message(STATUS "FFmpeg libraries: ${FFMPEG_LIBRARIES}")
message(STATUS "GStreamer libraries: ${GST_LIBRARIES}")
