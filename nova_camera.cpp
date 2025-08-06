// nova_camera.cpp - Ana NovaCamera sınıfı implementation
#include "nova_camera.h"
#include "gpu_detector.h"
#include <iostream>

NovaCamera::NovaCamera(const std::string& local_ip, int local_port,
                       const std::string& remote_ip, int remote_port)
    : local_ip(local_ip), remote_ip(remote_ip), local_port(local_port), remote_port(remote_port) {
    
    // Video kalite ayarları - kamera destekli
    config.width = 1280;
    config.height = 720;
    config.framerate = 30;
    config.bitrate = 8000; // 8 Mbps
    config.enable_gpu = true;
    config.enable_mirror = true;
    
    video_manager = std::make_unique<VideoManager>(config);
    heartbeat_manager = std::make_unique<HeartbeatManager>(remote_ip, remote_port);
}

bool NovaCamera::initialize() {
    std::cout << "=== Nova Camera - FFmpeg GPU Hızlandırmalı ===" << std::endl;
    std::cout << "Çözünürlük: " << config.width << "x" << config.height << " @ " << config.framerate << " fps" << std::endl;
    std::cout << "Bitrate: " << config.bitrate << " kbps" << std::endl;
    std::cout << "GPU Codec: " << GPUDetector::getBestGPUCodec() << std::endl;
    std::cout << "Ayna Efekti: " << (config.enable_mirror ? "Aktif" : "Pasif") << std::endl;
    
    return video_manager->initialize(remote_ip, remote_port, local_port);
}

bool NovaCamera::start() {
    if (!video_manager->start()) {
        return false;
    }
    
    heartbeat_manager->start();
    
    std::cout << "✓ Video streaming başlatıldı" << std::endl;
    std::cout << "✓ Heartbeat aktif" << std::endl;
    std::cout << "Çıkmak için ESC tuşuna basın..." << std::endl;
    
    return true;
}

void NovaCamera::stop() {
    heartbeat_manager->stop();
    video_manager->stop();
    std::cout << "=== Nova Camera Kapatılıyor ===" << std::endl;
}

bool NovaCamera::is_active() const {
    return video_manager->is_active();
} 