cmake_minimum_required(VERSION 3.22)
project(nova_engine_v2 LANGUAGES CXX)

# C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Yüksek performans optimizasyonları
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=native -mtune=native -O3 -ffast-math")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -funroll-loops -ftree-vectorize")
    message(STATUS "Yüksek performans optimizasyonları etkinleştirildi")
endif()

# Threading desteği (UDP streaming için kritik)
find_package(Threads REQUIRED)

# OpenMP desteği (paralel işleme)
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    message(STATUS "OpenMP bulundu - paralel işleme etkinleştirildi")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
else()
    message(WARNING "OpenMP bulunamadı, tek thread işleme kullanılacak")
endif()

# GStreamer (çekirdek + UDP streaming için gerekli)
find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED
        gstreamer-1.0
        gstreamer-app-1.0
        gstreamer-video-1.0
        gstreamer-rtp-1.0
)

# OpenCV (opsiyonel - gelecekte video işleme için)
find_package(OpenCV QUIET
        COMPONENTS core imgproc highgui objdetect dnn dnn_superres
)

# Uygulama
add_executable(nova_engine_v2
        main.cpp
)

# Test Receiver
add_executable(test_receiver
        test_receiver.cpp
)

# Basit Kamera Görüntüleyici
add_executable(simple_camera
        simple_camera.cpp
)

# Video Chat - Karşılıklı Görüntülü İletişim
add_executable(video_chat
        video_chat.cpp
)

# Gelişmiş Video Chat - Farklı WiFi Ağları
add_executable(video_chat_advanced
        video_chat_advanced.cpp
)

# Otomatik Optimize Video Chat
add_executable(video_chat_auto
        video_chat_auto.cpp
)

# Include dizinleri
target_include_directories(nova_engine_v2 PRIVATE
        ${GST_INCLUDE_DIRS}
        ${OpenCV_INCLUDE_DIRS}
)

target_include_directories(test_receiver PRIVATE
        ${GST_INCLUDE_DIRS}
        ${OpenCV_INCLUDE_DIRS}
)

target_include_directories(simple_camera PRIVATE
        ${GST_INCLUDE_DIRS}
        ${OpenCV_INCLUDE_DIRS}
)

target_include_directories(video_chat PRIVATE
        ${GST_INCLUDE_DIRS}
        ${OpenCV_INCLUDE_DIRS}
)

target_include_directories(video_chat_advanced PRIVATE
        ${GST_INCLUDE_DIRS}
        ${OpenCV_INCLUDE_DIRS}
)

target_include_directories(video_chat_auto PRIVATE
        ${GST_INCLUDE_DIRS}
        ${OpenCV_INCLUDE_DIRS}
)

# Link dizinleri
target_link_directories(nova_engine_v2 PRIVATE
        ${GST_LIBRARY_DIRS}
)

target_link_directories(test_receiver PRIVATE
        ${GST_LIBRARY_DIRS}
)

target_link_directories(simple_camera PRIVATE
        ${GST_LIBRARY_DIRS}
)

target_link_directories(video_chat PRIVATE
        ${GST_LIBRARY_DIRS}
)

target_link_directories(video_chat_advanced PRIVATE
        ${GST_LIBRARY_DIRS}
)

target_link_directories(video_chat_auto PRIVATE
        ${GST_LIBRARY_DIRS}
)

# Derleme bayrakları
target_compile_options(nova_engine_v2 PRIVATE
        ${GST_CFLAGS_OTHER}
        $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
        $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra>
)

target_compile_options(test_receiver PRIVATE
        ${GST_CFLAGS_OTHER}
        $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
        $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra>
)

target_compile_options(simple_camera PRIVATE
        ${GST_CFLAGS_OTHER}
        $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
        $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra>
)

target_compile_options(video_chat PRIVATE
        ${GST_CFLAGS_OTHER}
        $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
        $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra>
)

target_compile_options(video_chat_advanced PRIVATE
        ${GST_CFLAGS_OTHER}
        $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
        $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra>
)

target_compile_options(video_chat_auto PRIVATE
        ${GST_CFLAGS_OTHER}
        $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra>
        $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra>
)

# Bağıntılar
target_link_libraries(nova_engine_v2 PRIVATE
        ${GST_LIBRARIES}
        Threads::Threads
        ${OpenCV_LIBS}
)

target_link_libraries(test_receiver PRIVATE
        ${GST_LIBRARIES}
        Threads::Threads
        ${OpenCV_LIBS}
)

target_link_libraries(simple_camera PRIVATE
        ${GST_LIBRARIES}
        Threads::Threads
        ${OpenCV_LIBS}
)

target_link_libraries(video_chat PRIVATE
        ${GST_LIBRARIES}
        Threads::Threads
        ${OpenCV_LIBS}
)

target_link_libraries(video_chat_advanced PRIVATE
        ${GST_LIBRARIES}
        Threads::Threads
        ${OpenCV_LIBS}
)

target_link_libraries(video_chat_auto PRIVATE
        ${GST_LIBRARIES}
        Threads::Threads
        ${OpenCV_LIBS}
)

# Performans ayarları
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(nova_engine_v2 PRIVATE
        NDEBUG
        _RELEASE_BUILD
    )
    message(STATUS "Release build - performans optimizasyonları etkinleştirildi")
endif()

# Test hedefi (opsiyonel)
option(BUILD_TESTS "Test hedeflerini derle" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Kurulum hedefi
install(TARGETS nova_engine_v2
        RUNTIME DESTINATION bin
)

message(STATUS "NovaEngine V2 - UDP Video Streaming Engine")
message(STATUS "Hedef: 720p @ 45-50 fps, Serverless Video Conferencing")
message(STATUS "Optimizasyonlar: SIMD, OpenMP, Threading")
