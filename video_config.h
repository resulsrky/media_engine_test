// video_config.h - Video kalite ayarları
#pragma once
#include <string>

struct VideoConfig {
    int width = 1280;
    int height = 720;
    int framerate = 30;
    int bitrate = 8000; // 8 Mbps
    std::string codec = "h264_nvenc"; // NVIDIA GPU
    std::string preset = "p7"; // En hızlı preset
    std::string profile = "high";
    int gop_size = 60; // 1 saniye GOP
    bool enable_gpu = true;
    bool enable_mirror = true;
}; 