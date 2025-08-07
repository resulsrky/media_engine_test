// video_config.h - Video kalite ayarları
#pragma once
#include <string>

struct VideoConfig {
    int width = 1280;
    int height = 720;
    int framerate = 30; // 30 fps - Kamera maksimum desteği
    int bitrate = 15000; // 15 Mbps - Yüksek kalite için
    std::string codec = "h264_nvenc"; // NVIDIA GPU
    std::string preset = "p7"; // En hızlı preset
    std::string profile = "high";
    int gop_size = 30; // 1 saniye GOP - 30 fps için
    bool enable_gpu = true;
    bool enable_mirror = true;
    
    // GStreamer özel ayarları
    bool enable_hardware_acceleration = true;
    bool enable_low_latency = true;
    bool enable_async_processing = true;
    int buffer_size = 8; // GStreamer buffer size artırıldı
    int rtp_payload_type = 96; // RTP payload type
    
    // Kalite iyileştirme ayarları
    int crf = 18; // Constant Rate Factor - Düşük değer = Yüksek kalite
    int qp = 20; // Quantization Parameter - Düşük değer = Yüksek kalite
    bool enable_cabac = true; // Context Adaptive Binary Arithmetic Coding
    bool enable_deblock = true; // Deblocking filter
}; 