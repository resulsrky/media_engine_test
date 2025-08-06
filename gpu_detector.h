// gpu_detector.h - GPU tespiti
#pragma once
#include <gst/gst.h>
#include <string>

class GPUDetector {
public:
    static bool detectNVIDIA();
    static bool detectVAAPI();
    static std::string getBestGPUCodec();
}; 