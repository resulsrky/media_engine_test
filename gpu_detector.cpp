// gpu_detector.cpp - GPU tespiti implementation
#include "gpu_detector.h"
#include <iostream>
#include <gst/gst.h>

bool GPUDetector::detectNVIDIA() {
    const char* nvidia_elements[] = {
        "nvenc", "nvh264enc", "nvenc_h264", "nvh264enc"
    };
    
    for (const auto& element : nvidia_elements) {
        GstElementFactory* factory = gst_element_factory_find(element);
        if (factory) {
            gst_object_unref(factory);
            return true;
        }
    }
    return false;
}

bool GPUDetector::detectVAAPI() {
    const char* vaapi_elements[] = {
        "vaapih264enc", "vaapiencode", "vaapih264enc"
    };
    
    for (const auto& element : vaapi_elements) {
        GstElementFactory* factory = gst_element_factory_find(element);
        if (factory) {
            gst_object_unref(factory);
            return true;
        }
    }
    return false;
}

bool GPUDetector::detectQSV() {
    const char* qsv_elements[] = {
        "qsvh264enc", "msdkh264enc", "qsvh264enc"
    };
    
    for (const auto& element : qsv_elements) {
        GstElementFactory* factory = gst_element_factory_find(element);
        if (factory) {
            gst_object_unref(factory);
            return true;
        }
    }
    return false;
}

bool GPUDetector::detectV4L2H264() {
    const char* v4l2_elements[] = {
        "v4l2h264enc", "v4l2video0h264enc", "v4l2h264enc"
    };
    
    for (const auto& element : v4l2_elements) {
        GstElementFactory* factory = gst_element_factory_find(element);
        if (factory) {
            gst_object_unref(factory);
            return true;
        }
    }
    return false;
}

std::string GPUDetector::getBestGPUCodec() {
    gst_init(nullptr, nullptr);
    
    std::cout << "🔍 GStreamer GPU tespiti yapılıyor..." << std::endl;
    
    // NVIDIA NVENC - en hızlı
    if (detectNVIDIA()) {
        std::cout << "✓ NVIDIA GPU bulundu (NVENC) - GStreamer optimize" << std::endl;
        return "h264_nvenc";
    }
    
    // Intel/AMD VAAPI - iyi performans
    if (detectVAAPI()) {
        std::cout << "✓ Intel/AMD GPU bulundu (VAAPI) - GStreamer optimize" << std::endl;
        return "h264_vaapi";
    }
    
    // Intel Quick Sync - hızlı
    if (detectQSV()) {
        std::cout << "✓ Intel Quick Sync bulundu (QSV) - GStreamer optimize" << std::endl;
        return "h264_qsv";
    }
    
    // V4L2 Hardware - genel
    if (detectV4L2H264()) {
        std::cout << "✓ V4L2 H.264 encoder bulundu - GStreamer optimize" << std::endl;
        return "h264_v4l2";
    }
    
    std::cout << "⚠ GPU encoder bulunamadı, GStreamer CPU encoding kullanılacak" << std::endl;
    return "libx264"; // CPU fallback - GStreamer optimize
} 