// video_config.h - Video kalite ayarları
#pragma once
#include <string>

struct VideoConfig {
    int width = 1280;
    int height = 720;
    int framerate = 30;
    int bitrate = 12000; // 12 Mbps - hızlı hareket için artırıldı
    std::string codec = "h264_nvenc"; // NVIDIA GPU
    std::string preset = "p7"; // En hızlı preset
    std::string profile = "high";
    int gop_size = 30; // 1 saniye GOP - hızlı hareket için azaltıldı
    bool enable_gpu = true;
    bool enable_mirror = true;
}; 