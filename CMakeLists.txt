cmake_minimum_required(VERSION 3.10)
project(nova_engine_v2)

# C++ standardı
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# GStreamer paketleri
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED 
    gstreamer-1.0
    gstreamer-base-1.0
    gstreamer-video-1.0
    gstreamer-rtp-1.0
)

# OpenCV
find_package(OpenCV QUIET)

# Threads
find_package(Threads REQUIRED)

# OpenMP
find_package(OpenMP)

# Tek Kamera Uygulaması
add_executable(nova_camera
        nova_camera.cpp
)

# Include dizinleri
target_include_directories(nova_camera PRIVATE
        ${GST_INCLUDE_DIRS}
        ${OpenCV_INCLUDE_DIRS}
)

# Link dizinleri
target_link_directories(nova_camera PRIVATE
        ${GST_LIBRARY_DIRS}
)

# Derleme bayrakları
target_compile_options(nova_camera PRIVATE
        ${GST_CFLAGS_OTHER}
        $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
        $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra>
)

# Bağıntılar
target_link_libraries(nova_camera PRIVATE
        ${GST_LIBRARIES}
        Threads::Threads
        ${OpenCV_LIBS}
)

# Performans ayarları
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(nova_camera PRIVATE
        -O3
        -march=native
        -mtune=native
        -ffast-math
        -funroll-loops
        -ftree-vectorize
    )
    target_compile_definitions(nova_camera PRIVATE
        NDEBUG
        _RELEASE_BUILD
    )
endif()

# OpenMP desteği
if(OpenMP_CXX_FOUND)
    target_link_libraries(nova_camera PRIVATE OpenMP::OpenMP_CXX)
    target_compile_definitions(nova_camera PRIVATE _OPENMP)
endif()

# Install hedefi
install(TARGETS nova_camera DESTINATION bin)

# Mesajlar
message(STATUS "Yüksek performans optimizasyonları etkinleştirildi")
if(OpenMP_CXX_FOUND)
    message(STATUS "OpenMP bulundu - paralel işleme etkinleştirildi")
endif()
message(STATUS "NovaEngine V2 - UDP Video Streaming Engine")
message(STATUS "Hedef: 720p @ 45-50 fps, Serverless Video Conferencing")
message(STATUS "Optimizasyonlar: SIMD, OpenMP, Threading")
