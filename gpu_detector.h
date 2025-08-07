// gpu_detector.h - GPU ve kamera donanım tespiti
#pragma once
#include <string>
#include <vector>

struct CameraCapability {
    int width;
    int height;
    int framerate;
    std::string format;
    bool is_supported;
};

struct OptimalSettings {
    int width;
    int height;
    int framerate;
    int bitrate;
    std::string format;
    bool hardware_acceleration;
};

class GpuDetector {
public:
    static bool detectNvidiaGpu();
    static bool detectAmdGpu();
    static bool detectIntelGpu();
    static std::string getOptimalCodec();
    static std::vector<CameraCapability> detectCameraCapabilities();
    static OptimalSettings getOptimalSettings();
    static int calculateOptimalBitrate(int width, int height, int framerate);
}; 