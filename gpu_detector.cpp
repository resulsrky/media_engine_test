// gpu_detector.cpp - GPU tespiti implementation
#include "gpu_detector.h"

bool GPUDetector::detectNVIDIA() {
    GstElement *test = gst_element_factory_make("nvenc", "test");
    if (test) {
        gst_object_unref(test);
        return true;
    }
    return false;
}

bool GPUDetector::detectVAAPI() {
    GstElement *test = gst_element_factory_make("vaapih264enc", "test");
    if (test) {
        gst_object_unref(test);
        return true;
    }
    return false;
}

std::string GPUDetector::getBestGPUCodec() {
    gst_init(nullptr, nullptr);
    if (detectNVIDIA()) return "h264_nvenc";
    if (detectVAAPI()) return "h264_vaapi";
    return "libx264"; // CPU fallback
} 