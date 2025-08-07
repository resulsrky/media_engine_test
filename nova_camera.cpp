// nova_camera.cpp - Ana NovaCamera sınıfı implementation
#include "nova_camera.h"
#include "gpu_detector.h"
#include <iostream>
#include <algorithm> // for std::max

NovaCamera::NovaCamera(const std::string& local_ip, int local_port,
                       const std::string& remote_ip, int remote_port)
    : local_ip(local_ip), remote_ip(remote_ip), local_port(local_port), remote_port(remote_port) {

    // Otomatik donanım tespiti ve en iyi ayarları al
    auto optimal_settings = GpuDetector::getOptimalSettings();

    // GStreamer için VideoConfig'i doldur
    config.width = optimal_settings.width;
    config.height = optimal_settings.height;
    config.framerate = optimal_settings.framerate;
    config.bitrate = optimal_settings.bitrate;
    config.format = optimal_settings.format;

    // GOP size (keyframe aralığı), genellikle 1-2 saniye olarak ayarlanır
    config.gop_size = optimal_settings.framerate * 2;

    config.enable_gpu = optimal_settings.hardware_acceleration;
    config.enable_mirror = true; // Ayna efekti açık kalsın

    // VideoManager ve HeartbeatManager'ı oluştur
    video_manager = std::make_unique<VideoManager>(config);
    heartbeat_manager = std::make_unique<HeartbeatManager>(remote_ip, remote_port);
}

bool NovaCamera::initialize() {
    std::cout << "=== Nova Camera - Otomatik Donanım Optimizasyonu ===" << std::endl;
    std::cout << "🔍 Donanım ve kamera tespiti yapılıyor..." << std::endl;

    std::string encoder = GpuDetector::getOptimalGstEncoder();
    if (encoder == "x264enc") {
        std::cout << "⚠ Donanım hızlandırmalı encoder bulunamadı. CPU (x264enc) kullanılacak." << std::endl;
    } else {
        std::cout << "✓ GPU Encoder bulundu: " << encoder << std::endl;
    }

    std::cout << "📹 En iyi kamera modu seçildi:" << std::endl;
    std::cout << "  - Çözünürlük: " << config.width << "x" << config.height << std::endl;
    std::cout << "  - Kare Hızı (FPS): " << config.framerate << std::endl;
    std::cout << "  - Kamera Formatı: " << config.format << std::endl;

    std::cout << "⚡ Otomatik optimize edilmiş yayın ayarları:" << std::endl;
    std::cout << "  - Bitrate: " << config.bitrate / 1000 << " kbps (" << config.bitrate / 1000000.0 << " Mbps)" << std::endl;
    std::cout << "  - GOP Boyutu: " << config.gop_size << " kare (" << (double)config.gop_size / std::max(1, config.framerate) << " saniyede bir keyframe)" << std::endl;
    std::cout << "  - Donanım Hızlandırma: " << (config.enable_gpu ? "Aktif" : "Pasif") << std::endl;
    std::cout << "  - Ayna Efekti: " << (config.enable_mirror ? "Aktif" : "Pasif") << std::endl;
    std::cout << "----------------------------------------------------" << std::endl;

    return video_manager->initialize(remote_ip, remote_port, local_port);
}

bool NovaCamera::start() {
    if (!video_manager->start()) {
        return false;
    }

    heartbeat_manager->start();
    std::cout << "✓ Video yayını ve alımı başlatıldı." << std::endl;
    std::cout << "✓ Heartbeat sinyalleri gönderiliyor." << std::endl;
    std::cout << "Programdan çıkmak için ESC tuşuna basın." << std::endl;

    return true;
}

void NovaCamera::stop() {
    std::cout << "\nProgram sonlandırılıyor..." << std::endl;
    heartbeat_manager->stop();
    video_manager->stop();
    std::cout << "Tüm servisler durduruldu. Hoşçakalın!" << std::endl;
}

bool NovaCamera::is_active() const {
    return video_manager && video_manager->is_active();
}