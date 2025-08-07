// video_config.h - Video kalite ayarları
#pragma once
#include <string>

struct VideoConfig {
    // Dinamik olarak belirlenen ayarlar
    int width;
    int height;
    int framerate;
    long long bitrate; // Yüksek değerler için long long
    int gop_size;
    std::string format; // !! EKSİK OLAN SATIR BUYDU !!

    // Kullanıcı tarafından ayarlanabilen sabitler
    bool enable_gpu = true;
    bool enable_mirror = true;

    // GStreamer özel ayarları (pipeline içinde kullanılır)
    bool enable_low_latency = true;
    bool enable_async_processing = true;
    int buffer_size = 8;
    int rtp_payload_type = 96;

    // Encoder kalite ayarları (pipeline içinde kullanılır)
    int crf = 18;
    int qp = 20;
    bool enable_cabac = true;
    bool enable_deblock = true;
};