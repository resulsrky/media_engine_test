// nova_camera.cpp - Ana NovaCamera sınıfı implementation
#include "nova_camera.h"
#include "gpu_detector.h"
#include <iostream>

NovaCamera::NovaCamera(const std::string& local_ip, int local_port,
                       const std::string& remote_ip, int remote_port)
    : local_ip(local_ip), remote_ip(remote_ip), local_port(local_port), remote_port(remote_port) {
    
    // Otomatik donanım tespiti ve optimizasyon
    auto optimal_settings = GpuDetector::getOptimalSettings();
    
    // GStreamer optimize video kalite ayarları - Otomatik optimize
    config.width = optimal_settings.width;
    config.height = optimal_settings.height;
    config.framerate = optimal_settings.framerate;
    config.bitrate = optimal_settings.bitrate;
    config.gop_size = optimal_settings.framerate * 10; // 10 saniye GOP
    config.enable_gpu = optimal_settings.hardware_acceleration;
    config.enable_mirror = true;
    
    // GStreamer özel ayarları
    config.enable_hardware_acceleration = optimal_settings.hardware_acceleration;
    config.enable_low_latency = true;
    config.enable_async_processing = true;
    config.buffer_size = 8; // Buffer size artırıldı
    config.rtp_payload_type = 96;
    
    // Kalite iyileştirme ayarları - Maksimum kalite
    config.crf = 18; // Constant Rate Factor - Düşük değer = Yüksek kalite
    config.qp = 20; // Quantization Parameter - Düşük değer = Yüksek kalite
    config.enable_cabac = true; // Context Adaptive Binary Arithmetic Coding
    config.enable_deblock = true; // Deblocking filter
    
    video_manager = std::make_unique<VideoManager>(config);
    heartbeat_manager = std::make_unique<HeartbeatManager>(remote_ip, remote_port);
}

bool NovaCamera::initialize() {
    std::cout << "=== Nova Camera - Otomatik Donanım Optimizasyonu ===" << std::endl;
    
    // Donanım tespiti
    std::cout << "🔍 Donanım tespiti yapılıyor..." << std::endl;
    
    if (GpuDetector::detectNvidiaGpu()) {
        std::cout << "✓ NVIDIA GPU tespit edildi - NVENC encoding" << std::endl;
    } else if (GpuDetector::detectAmdGpu()) {
        std::cout << "✓ AMD GPU tespit edildi - AMF encoding" << std::endl;
    } else if (GpuDetector::detectIntelGpu()) {
        std::cout << "✓ Intel GPU tespit edildi - QSV encoding" << std::endl;
    } else {
        std::cout << "⚠ GPU bulunamadı - CPU encoding kullanılacak" << std::endl;
    }
    
    // Kamera yetenekleri
    auto capabilities = GpuDetector::detectCameraCapabilities();
    std::cout << "📹 Kamera yetenekleri tespit edildi:" << std::endl;
    for (const auto& cap : capabilities) {
        if (cap.is_supported) {
            std::cout << "  - " << cap.width << "x" << cap.height << " @ " << cap.framerate << " fps" << std::endl;
        }
    }
    
    // Optimize edilmiş ayarlar
    std::cout << "⚡ Otomatik optimize edilmiş ayarlar:" << std::endl;
    std::cout << "Çözünürlük: " << config.width << "x" << config.height << " @ " << config.framerate << " fps" << std::endl;
    std::cout << "Bitrate: " << config.bitrate << " kbps (Maksimum kalite)" << std::endl;
    std::cout << "GOP Size: " << config.gop_size << " frames (" << (static_cast<double>(config.gop_size) / std::max(1, config.framerate)) << " sn)" << std::endl;
    std::cout << "CRF: " << config.crf << " (Maksimum kalite)" << std::endl;
    std::cout << "QP: " << config.qp << " (Maksimum kalite)" << std::endl;
    std::cout << "Hardware Acceleration: " << (config.enable_hardware_acceleration ? "Aktif" : "Pasif") << std::endl;
    std::cout << "Ayna Efekti: " << (config.enable_mirror ? "Aktif" : "Pasif") << std::endl;
    std::cout << "Low Latency: " << (config.enable_low_latency ? "Aktif" : "Pasif") << std::endl;
    
    return video_manager->initialize(remote_ip, remote_port, local_port);
}

bool NovaCamera::start() {
    if (!video_manager->start()) {
        return false;
    }
    
    heartbeat_manager->start();
    std::cout << "✓ Otomatik optimize video streaming başlatıldı" << std::endl;
    std::cout << "✓ Heartbeat aktif" << std::endl;
    std::cout << "✓ Maksimum kalite ayarları aktif" << std::endl;
    
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