// gpu_detector.h - GPU tespiti
#pragma once
#include <gst/gst.h>
#include <string>

class GPUDetector {
public:
    static bool detectNVIDIA();
    static bool detectVAAPI();
    static bool detectQSV();
    static bool detectV4L2H264();
    static std::string getBestGPUCodec();
}; 