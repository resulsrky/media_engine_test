// nova_camera.cpp - Ana NovaCamera sınıfı implementation
#include "nova_camera.h"
#include "gpu_detector.h"
#include <iostream>

NovaCamera::NovaCamera(const std::string& local_ip, int local_port,
                       const std::string& remote_ip, int remote_port)
    : local_ip(local_ip), remote_ip(remote_ip), local_port(local_port), remote_port(remote_port) {
    
    // GStreamer optimize video kalite ayarları
    config.width = 1280;
    config.height = 720;
    config.framerate = 30;
    config.bitrate = 8000; // 8 Mbps - GStreamer optimize
    config.gop_size = 60; // 2 saniye GOP - GStreamer optimize
    config.enable_gpu = true;
    config.enable_mirror = true;
    
    // GStreamer özel ayarları
    config.enable_hardware_acceleration = true;
    config.enable_low_latency = true;
    config.enable_async_processing = true;
    config.buffer_size = 4;
    config.rtp_payload_type = 96;
    
    video_manager = std::make_unique<VideoManager>(config);
    heartbeat_manager = std::make_unique<HeartbeatManager>(remote_ip, remote_port);
}

bool NovaCamera::initialize() {
    std::cout << "=== Nova Camera - GStreamer Optimize ===" << std::endl;
    std::cout << "Çözünürlük: " << config.width << "x" << config.height << " @ " << config.framerate << " fps" << std::endl;
    std::cout << "Bitrate: " << config.bitrate << " kbps (GStreamer optimize)" << std::endl;
    std::cout << "GOP Size: " << config.gop_size << " frames" << std::endl;
    std::cout << "Ayna Efekti: " << (config.enable_mirror ? "Aktif" : "Pasif") << std::endl;
    std::cout << "Hardware Acceleration: " << (config.enable_hardware_acceleration ? "Aktif" : "Pasif") << std::endl;
    std::cout << "Low Latency: " << (config.enable_low_latency ? "Aktif" : "Pasif") << std::endl;
    
    return video_manager->initialize(remote_ip, remote_port, local_port);
}

bool NovaCamera::start() {
    if (!video_manager->start()) {
        return false;
    }
    
    heartbeat_manager->start();
    std::cout << "✓ GStreamer optimize video streaming başlatıldı" << std::endl;
    std::cout << "✓ Heartbeat aktif" << std::endl;
    std::cout << "✓ GStreamer optimize ayarlar aktif" << std::endl;
    
    return true;
}

void NovaCamera::stop() {
    std::cout << "Program sonlandırılıyor..." << std::endl;
    video_manager->stop();
    heartbeat_manager->stop();
}

bool NovaCamera::is_active() const {
    return video_manager->is_active();
} 