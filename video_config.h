// video_config.h - Video kalite ayarları
#pragma once
#include <string>

struct VideoConfig {
    int width = 1280;
    int height = 720;
    int framerate = 30;
    int bitrate = 8000; // 8 Mbps - GStreamer optimize
    std::string codec = "h264_nvenc"; // NVIDIA GPU
    std::string preset = "p7"; // En hızlı preset
    std::string profile = "high";
    int gop_size = 60; // 2 saniye GOP - GStreamer optimize
    bool enable_gpu = true;
    bool enable_mirror = true;
    
    // GStreamer özel ayarları
    bool enable_hardware_acceleration = true;
    bool enable_low_latency = true;
    bool enable_async_processing = true;
    int buffer_size = 4; // GStreamer buffer size
    int rtp_payload_type = 96; // RTP payload type
}; 