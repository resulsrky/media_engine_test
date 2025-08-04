cmake_minimum_required(VERSION 3.22)
project(zero_copy_cam LANGUAGES CXX)

# C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Intel HD Graphics için SIMD optimizasyonları
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -mtune=native -O3")
    message(STATUS "Intel HD Graphics SIMD optimizations enabled")
endif()

# OpenMP desteği (Intel HD Graphics için paralel işleme)
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    message(STATUS "OpenMP found for Intel HD Graphics parallel processing")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
else()
    message(WARNING "OpenMP not found, will use single-threaded processing")
endif()

# GStreamer (çekirdek + appsink)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED
        gstreamer-1.0
        gstreamer-app-1.0
)

# OpenCV (DNN + SuperRes + Cascade)
find_package(OpenCV REQUIRED
        COMPONENTS core imgproc highgui objdetect dnn dnn_superres
)

# Uygulama
add_executable(zero_copy_cam
        main.cpp
)

# Include ve link dizinleri
target_include_directories(zero_copy_cam PRIVATE
        ${GST_INCLUDE_DIRS}
        ${OpenCV_INCLUDE_DIRS}
)
target_link_directories(zero_copy_cam PRIVATE
        ${GST_LIBRARY_DIRS}
)

# Derleme bayrakları
target_compile_options(zero_copy_cam PRIVATE
        ${GST_CFLAGS_OTHER}
)

# Bağıntılar
target_link_libraries(zero_copy_cam PRIVATE
        ${GST_LIBRARIES}
        ${OpenCV_LIBS}
)
