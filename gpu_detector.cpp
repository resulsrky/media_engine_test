// gpu_detector.cpp - GPU tespiti implementation
#include "gpu_detector.h"
#include <iostream>

bool GPUDetector::detectNVIDIA() {
    GstElement *test = gst_element_factory_make("nvenc", "test");
    if (test) {
        gst_object_unref(test);
        return true;
    }
    
    // NVENC alternatif kontrolü
    test = gst_element_factory_make("nvh264enc", "test");
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
    
    // VAAPI alternatif kontrolü
    test = gst_element_factory_make("vaapiencode", "test");
    if (test) {
        gst_object_unref(test);
        return true;
    }
    
    return false;
}

bool GPUDetector::detectQSV() {
    GstElement *test = gst_element_factory_make("qsvh264enc", "test");
    if (test) {
        gst_object_unref(test);
        return true;
    }
    
    // QSV alternatif kontrolü
    test = gst_element_factory_make("msdkh264enc", "test");
    if (test) {
        gst_object_unref(test);
        return true;
    }
    
    return false;
}

bool GPUDetector::detectV4L2H264() {
    GstElement *test = gst_element_factory_make("v4l2h264enc", "test");
    if (test) {
        gst_object_unref(test);
        return true;
    }
    
    // V4L2 alternatif kontrolü
    test = gst_element_factory_make("v4l2video0h264enc", "test");
    if (test) {
        gst_object_unref(test);
        return true;
    }
    
    return false;
}

std::string GPUDetector::getBestGPUCodec() {
    gst_init(nullptr, nullptr);
    
    std::cout << "🔍 GPU tespiti yapılıyor..." << std::endl;
    
    // NVIDIA GPU tespiti
    if (detectNVIDIA()) {
        std::cout << "✓ NVIDIA GPU bulundu (NVENC)" << std::endl;
        return "h264_nvenc";
    }
    
    // Intel/AMD VAAPI tespiti
    if (detectVAAPI()) {
        std::cout << "✓ Intel/AMD GPU bulundu (VAAPI)" << std::endl;
        return "h264_vaapi";
    }
    
    // Intel Quick Sync tespiti
    if (detectQSV()) {
        std::cout << "✓ Intel Quick Sync bulundu (QSV)" << std::endl;
        return "h264_qsv";
    }
    
    // V4L2 Hardware encoder tespiti
    if (detectV4L2H264()) {
        std::cout << "✓ V4L2 H.264 encoder bulundu" << std::endl;
        return "h264_v4l2";
    }
    
    std::cout << "⚠ GPU encoder bulunamadı, CPU encoding kullanılacak" << std::endl;
    return "libx264"; // CPU fallback
} 