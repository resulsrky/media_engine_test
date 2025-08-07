// gpu_detector.cpp - GPU ve kamera donanım tespiti implementation
#include "gpu_detector.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>

bool GpuDetector::detectNvidiaGpu() {
    std::ifstream file("/proc/driver/nvidia/version");
    if (file.good()) {
        file.close();
        return true;
    }
    
    // nvidia-smi komutu ile kontrol
    int result = system("nvidia-smi --query-gpu=name --format=csv,noheader,nounits > /dev/null 2>&1");
    return (result == 0);
}

bool GpuDetector::detectAmdGpu() {
    std::ifstream file("/sys/class/drm/card0/device/vendor");
    if (file.good()) {
        std::string vendor;
        file >> vendor;
        file.close();
        return (vendor == "0x1002"); // AMD vendor ID
    }
    return false;
}

bool GpuDetector::detectIntelGpu() {
    std::ifstream file("/sys/class/drm/card0/device/vendor");
    if (file.good()) {
        std::string vendor;
        file >> vendor;
        file.close();
        return (vendor == "0x8086"); // Intel vendor ID
    }
    return false;
}

std::string GpuDetector::getOptimalCodec() {
    if (detectNvidiaGpu()) {
        return "h264_nvenc";
    } else if (detectAmdGpu()) {
        return "h264_amf";
    } else if (detectIntelGpu()) {
        return "h264_qsv";
    } else {
        return "libx264"; // CPU fallback
    }
}

std::vector<CameraCapability> GpuDetector::detectCameraCapabilities() {
    std::vector<CameraCapability> capabilities;
    
    // v4l2-ctl komutu ile kamera yeteneklerini al
    FILE* pipe = popen("v4l2-ctl --list-formats-ext 2>/dev/null", "r");
    if (!pipe) {
        return capabilities;
    }
    
    char buffer[256];
    std::string result;
    
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    
    pclose(pipe);
    
    // Sonuçları parse et
    std::istringstream iss(result);
    std::string line;
    CameraCapability current;
    bool in_format = false;
    
    while (std::getline(iss, line)) {
        if (line.find("Size: Discrete") != std::string::npos) {
            // Çözünürlük bilgisini parse et
            size_t pos = line.find("Size: Discrete ");
            if (pos != std::string::npos) {
                std::string size = line.substr(pos + 15);
                size_t x_pos = size.find("x");
                if (x_pos != std::string::npos) {
                    current.width = std::stoi(size.substr(0, x_pos));
                    current.height = std::stoi(size.substr(x_pos + 1));
                }
            }
        } else if (line.find("Interval: Discrete") != std::string::npos) {
            // Framerate bilgisini parse et
            size_t pos = line.find("(");
            if (pos != std::string::npos) {
                std::string fps_str = line.substr(pos + 1);
                size_t end_pos = fps_str.find(" fps");
                if (end_pos != std::string::npos) {
                    current.framerate = std::stoi(fps_str.substr(0, end_pos));
                    current.is_supported = true;
                    capabilities.push_back(current);
                }
            }
        }
    }
    
    return capabilities;
}

OptimalSettings GpuDetector::getOptimalSettings() {
    OptimalSettings settings;
    
    // Kamera yeteneklerini tespit et
    auto capabilities = detectCameraCapabilities();
    
    if (capabilities.empty()) {
        // Varsayılan ayarlar
        settings.width = 1280;
        settings.height = 720;
        settings.framerate = 30;
        settings.bitrate = 15000;
        settings.format = "MJPG";
        settings.hardware_acceleration = false;
        return settings;
    }
    
    // En yüksek çözünürlük ve framerate'i seç
    auto best_capability = *std::max_element(capabilities.begin(), capabilities.end(),
        [](const CameraCapability& a, const CameraCapability& b) {
            if (a.width * a.height != b.width * b.height) {
                return a.width * a.height < b.width * b.height;
            }
            return a.framerate < b.framerate;
        });
    
    settings.width = best_capability.width;
    settings.height = best_capability.height;
    settings.framerate = best_capability.framerate;
    settings.format = best_capability.format;
    
    // Bitrate'i hesapla
    settings.bitrate = calculateOptimalBitrate(settings.width, settings.height, settings.framerate);
    
    // Hardware acceleration kontrolü
    settings.hardware_acceleration = (getOptimalCodec() != "libx264");
    
    return settings;
}

int GpuDetector::calculateOptimalBitrate(int width, int height, int framerate) {
    // Piksel sayısına ve framerate'e göre bitrate hesapla
    int pixels = width * height;
    int base_bitrate;
    
    if (pixels >= 1920 * 1080) {
        base_bitrate = 20000; // 1080p için 20 Mbps
    } else if (pixels >= 1280 * 720) {
        base_bitrate = 15000; // 720p için 15 Mbps
    } else if (pixels >= 854 * 480) {
        base_bitrate = 8000;  // 480p için 8 Mbps
    } else {
        base_bitrate = 4000;  // Düşük çözünürlük için 4 Mbps
    }
    
    // Framerate'e göre ayarla
    float fps_factor = framerate / 30.0f;
    int optimal_bitrate = static_cast<int>(base_bitrate * fps_factor);
    
    // Maksimum kalite için minimum 8000 kbps
    return std::max(optimal_bitrate, 8000);
} 