// gpu_detector.cpp - GPU ve kamera donanım tespiti implementation
#include "gpu_detector.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <map>

// GStreamer elemanının varlığını kontrol eden yardımcı fonksiyon
static bool has_gst_element(const char* name) {
    // GStreamer'ı geçici olarak başlatıp elemanı arayabiliriz,
    // ancak bu karmaşık olabilir. Şimdilik sistem komutu daha basit.
    std::string command = "gst-inspect-1.0 " + std::string(name) + " > /dev/null 2>&1";
    return system(command.c_str()) == 0;
}

bool GpuDetector::detectNvidiaGpu() {
    return system("which nvidia-smi > /dev/null 2>&1") == 0;
}

bool GpuDetector::detectAmdGpu() {
    // lspci, AMD (Radeon, etc.) için daha güvenilir bir yöntemdir
    return system("lspci | grep -E 'VGA|3D' | grep -i 'AMD|Advanced Micro Devices' > /dev/null 2>&1") == 0;
}

bool GpuDetector::detectIntelGpu() {
    return system("lspci | grep -E 'VGA|3D' | grep -i 'Intel' > /dev/null 2>&1") == 0;
}

std::string GpuDetector::getOptimalGstEncoder() {
    // En iyi performanstan en kötüye doğru sıralama
    if (detectNvidiaGpu() && has_gst_element("nvh264enc")) return "nvh264enc";
    if (detectIntelGpu() && has_gst_element("qsvh264enc")) return "qsvh264enc";
    if (detectAmdGpu() && has_gst_element("vaapih264enc")) return "vaapih264enc"; // AMD için yaygın VAAPI encoder
    if (has_gst_element("v4l2h264enc")) return "v4l2h264enc"; // Genel V4L2 encoder (örn. Raspberry Pi)
    return "x264enc"; // CPU fallback
}

std::vector<CameraCapability> GpuDetector::detectCameraCapabilities() {
    std::vector<CameraCapability> capabilities;
    FILE* pipe = popen("v4l2-ctl --list-formats-ext", "r");
    if (!pipe) {
        perror("popen v4l2-ctl failed");
        return capabilities;
    }

    char buffer[1024];
    std::string result = "";
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        result += buffer;
    }
    pclose(pipe);

    std::istringstream iss(result);
    std::string line;
    CameraCapability current_capability;
    std::string current_format;

    while (std::getline(iss, line)) {
        // Format bilgisini yakala (örn: [0]: 'MJPG')
        if (line.find("Pixel Format") != std::string::npos) {
            size_t start = line.find('\'');
            size_t end = line.find('\'', start + 1);
            if (start != std::string::npos && end != std::string::npos) {
                current_format = line.substr(start + 1, end - start - 1);
            }
        }
        // Çözünürlük bilgisini yakala
        else if (line.find("Size: Discrete") != std::string::npos) {
            std::string res_str = line.substr(line.find("Discrete") + 9);
            size_t x_pos = res_str.find('x');
            if (x_pos != std::string::npos) {
                current_capability.width = std::stoi(res_str.substr(0, x_pos));
                current_capability.height = std::stoi(res_str.substr(x_pos + 1));
                current_capability.format = current_format;
            }
        }
        // Framerate bilgisini yakala ve yeteneği listeye ekle
        else if (line.find("Interval: Discrete") != std::string::npos) {
            size_t open_paren = line.find('(');
            while (open_paren != std::string::npos) {
                size_t space_after_paren = line.find(' ', open_paren);
                size_t close_paren = line.find(')', open_paren);
                if (space_after_paren != std::string::npos && close_paren != std::string::npos) {
                    std::string fps_str = line.substr(open_paren + 1, space_after_paren - open_paren -1);
                    double fps_val = std::stod(fps_str);
                    if (fps_val > 0) {
                        current_capability.framerate = static_cast<int>(fps_val);
                        current_capability.is_supported = true;
                        capabilities.push_back(current_capability);
                    }
                }
                 open_paren = line.find('(', open_paren + 1);
            }
        }
    }
    return capabilities;
}

long long GpuDetector::calculateOptimalBitrate(int width, int height, int framerate) {
    long long pixels = width * height;
    double base_bitrate;

    // Yüksek Kalite için temel bitrate değerleri (kbps cinsinden)
    if (pixels >= 1920 * 1080) {
        base_bitrate = 8000; // 1080p için 8 Mbps temel
    } else if (pixels >= 1280 * 720) {
        base_bitrate = 5000; // 720p için 5 Mbps temel
    } else {
        base_bitrate = 2500;  // Düşük çözünürlük için 2.5 Mbps
    }

    // Framerate'e göre çarpan uygula. 60 FPS için bitrate'i ciddi oranda artır.
    double fps_factor = 1.0;
    if (framerate > 50) { // 59.94, 60 fps
        fps_factor = 2.0;
    } else if (framerate > 25) { // 29.97, 30 fps
        fps_factor = 1.5;
    }

    // Hareket karmaşıklığı için bir çarpan daha ekleyebiliriz (şimdilik sabit)
    double motion_factor = 1.5; // Ortalama hareketli sahne için

    long long optimal_bitrate = static_cast<long long>(base_bitrate * fps_factor * motion_factor);

    // Sonucu 1000'lik katlara yuvarla ve makul bir alt limit koy
    return std::max(4000LL, (optimal_bitrate / 1000) * 1000);
}

OptimalSettings GpuDetector::getOptimalSettings() {
    OptimalSettings settings;
    auto capabilities = detectCameraCapabilities();

    if (capabilities.empty()) {
        std::cerr << "UYARI: Kamera yetenekleri tespit edilemedi. Varsayılan ayarlara dönülüyor." << std::endl;
        settings.width = 1280;
        settings.height = 720;
        settings.framerate = 30;
        settings.format = "MJPG";
        settings.hardware_acceleration = (getOptimalGstEncoder() != "x264enc");
        settings.bitrate = calculateOptimalBitrate(settings.width, settings.height, settings.framerate);
        return settings;
    }

    // En iyi ayarı bulmak için önceliklendirme:
    // 1. 1920x1080 @ 60 FPS
    // 2. 1920x1080 @ en yüksek FPS
    // 3. 1280x720 @ 60 FPS
    // 4. 1280x720 @ en yüksek FPS
    // 5. En yüksek çözünürlük @ en yüksek FPS

    auto find_best = [&](int w, int h, int fps_req = -1) -> std::vector<CameraCapability> {
        std::vector<CameraCapability> matches;
        for (const auto& cap : capabilities) {
            if (cap.width == w && cap.height == h) {
                if (fps_req != -1 && cap.framerate == fps_req) {
                    matches.push_back(cap);
                    return matches; // Tam isabet, hemen dön
                }
                matches.push_back(cap);
            }
        }
        if (!matches.empty()) {
             std::sort(matches.begin(), matches.end(), [](const CameraCapability& a, const CameraCapability& b){
                return a.framerate > b.framerate; // En yüksek framerate'i başa al
            });
        }
        return matches;
    };

    std::vector<CameraCapability> best_options;

    best_options = find_best(1920, 1080, 60);
    if (best_options.empty()) best_options = find_best(1920, 1080);
    if (best_options.empty()) best_options = find_best(1280, 720, 60);
    if (best_options.empty()) best_options = find_best(1280, 720);

    CameraCapability best_capability;

    if (!best_options.empty()) {
        best_capability = best_options.front(); // Sıralamadan dolayı en iyisi başta
    } else {
        // Hiçbir tercihli mod bulunamazsa, en yüksek piksel ve FPS'yi seç
        std::sort(capabilities.begin(), capabilities.end(), [](const CameraCapability& a, const CameraCapability& b) {
            long long pixels_a = a.width * a.height;
            long long pixels_b = b.width * b.height;
            if (pixels_a != pixels_b) return pixels_a > pixels_b;
            return a.framerate > b.framerate;
        });
        best_capability = capabilities.front();
    }

    settings.width = best_capability.width;
    settings.height = best_capability.height;
    settings.framerate = best_capability.framerate;
    settings.format = best_capability.format; // Genellikle MJPG veya YUYV

    settings.bitrate = calculateOptimalBitrate(settings.width, settings.height, settings.framerate);
    settings.hardware_acceleration = (getOptimalGstEncoder() != "x264enc");

    return settings;
}