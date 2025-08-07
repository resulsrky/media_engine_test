// gpu_detector.h - GPU ve kamera donanım tespiti
#pragma once
#include <string>
#include <vector>

struct CameraCapability {
    int width;
    int height;
    int framerate;
    std::string format; // e.g., "MJPG", "YUYV"
    bool is_supported = false;
};

struct OptimalSettings {
    int width;
    int height;
    int framerate;
    long long bitrate; // Bitrate can be large
    std::string format;
    bool hardware_acceleration;
};

class GpuDetector {
public:
    static bool detectNvidiaGpu();
    static bool detectAmdGpu();
    static bool detectIntelGpu();
    static std::string getOptimalGstEncoder();
    static std::vector<CameraCapability> detectCameraCapabilities();
    static OptimalSettings getOptimalSettings();
    static long long calculateOptimalBitrate(int width, int height, int framerate);
};