// video_manager.cpp - Video işleme yöneticisi implementation
#include "video_manager.h"
#include "gpu_detector.h"
#include <iostream>

VideoManager::VideoManager(const VideoConfig& cfg) 
    : send_pipeline(nullptr), receive_pipeline(nullptr), is_running(false), 
      config(cfg), builder(cfg) {}

VideoManager::~VideoManager() {
    stop();
}

bool VideoManager::initialize(const std::string& remote_ip, int remote_port, int local_port) {
    gst_init(nullptr, nullptr);
    
    // GPU detection
    std::string gpu_codec = GpuDetector::getOptimalCodec();
    std::cout << "✓ GPU Codec: " << gpu_codec << std::endl;
    
    // Pipeline oluştur
    std::string send_pipeline_desc = builder.buildSenderPipeline(remote_ip, remote_port);
    std::string receive_pipeline_desc = builder.buildReceiverPipeline(local_port);

    // DEBUG: Oluşturulan pipeline'ları yazdır
    std::cout << "SEND PIPELINE: " << send_pipeline_desc << std::endl;
    std::cout << "RECV PIPELINE: " << receive_pipeline_desc << std::endl;
    
    GError *error = nullptr;
    
    send_pipeline = gst_parse_launch(send_pipeline_desc.c_str(), &error);
    if (!send_pipeline) {
        std::cerr << "Gönderici pipeline hatası: " << (error ? error->message : "Bilinmeyen hata") << std::endl;
        if (error) g_error_free(error);
        return false;
    }
    
    receive_pipeline = gst_parse_launch(receive_pipeline_desc.c_str(), &error);
    if (!receive_pipeline) {
        std::cerr << "Alıcı pipeline hatası: " << (error ? error->message : "Bilinmeyen hata") << std::endl;
        if (error) g_error_free(error);
        return false;
    }
    
    return true;
}

bool VideoManager::start() {
    if (!send_pipeline || !receive_pipeline) {
        std::cerr << "Pipeline'lar başlatılamadı!" << std::endl;
        return false;
    }
    
    is_running = true;
    
    // Gönderici thread
    send_thread = std::thread([this]() {
        GstStateChangeReturn ret = gst_element_set_state(send_pipeline, GST_STATE_PLAYING);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            std::cerr << "Gönderici pipeline PLAY durumuna geçemedi!" << std::endl;
            return;
        }
        
        std::cout << "✓ Kamera görüntüsü alınıyor..." << std::endl;
        std::cout << "✓ GPU encoding aktif..." << std::endl;
        
        GstBus *bus = gst_element_get_bus(send_pipeline);
        while (is_running.load()) {
            GstMessage *msg = gst_bus_timed_pop_filtered(
                bus, GST_CLOCK_TIME_NONE,
                (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS)
            );
            
            if (!msg) continue;
            
            switch (GST_MESSAGE_TYPE(msg)) {
                case GST_MESSAGE_ERROR: {
                    GError *err;
                    gchar *dbg;
                    gst_message_parse_error(msg, &err, &dbg);
                    std::cerr << "Gönderici Hatası: " << err->message << std::endl;
                    g_clear_error(&err);
                    g_free(dbg);
                    break;
                }
                case GST_MESSAGE_EOS:
                    std::cout << "Gönderici stream sonuna ulaştı." << std::endl;
                    break;
                default:
                    break;
            }
            gst_message_unref(msg);
        }
        
        gst_object_unref(bus);
    });
    
    // Alıcı thread
    receive_thread = std::thread([this]() {
        GstStateChangeReturn ret = gst_element_set_state(receive_pipeline, GST_STATE_PLAYING);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            std::cerr << "Alıcı pipeline PLAY durumuna geçemedi!" << std::endl;
            return;
        }
        
        std::cout << "✓ Karşı taraf görüntüsü alınıyor..." << std::endl;
        
        GstBus *bus = gst_element_get_bus(receive_pipeline);
        while (is_running.load()) {
            GstMessage *msg = gst_bus_timed_pop_filtered(
                bus, GST_CLOCK_TIME_NONE,
                (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS)
            );
            
            if (!msg) continue;
            
            switch (GST_MESSAGE_TYPE(msg)) {
                case GST_MESSAGE_ERROR: {
                    GError *err;
                    gchar *dbg;
                    gst_message_parse_error(msg, &err, &dbg);
                    std::cerr << "Alıcı Hatası: " << err->message << std::endl;
                    g_clear_error(&err);
                    g_free(dbg);
                    break;
                }
                case GST_MESSAGE_EOS:
                    std::cout << "Alıcı stream sonuna ulaştı." << std::endl;
                    break;
                default:
                    break;
            }
            gst_message_unref(msg);
        }
        
        gst_object_unref(bus);
    });
    
    return true;
}

void VideoManager::stop() {
    is_running = false;
    
    if (send_thread.joinable()) {
        send_thread.join();
    }
    if (receive_thread.joinable()) {
        receive_thread.join();
    }
    
    if (send_pipeline) {
        gst_element_set_state(send_pipeline, GST_STATE_NULL);
        gst_object_unref(send_pipeline);
        send_pipeline = nullptr;
    }
    
    if (receive_pipeline) {
        gst_element_set_state(receive_pipeline, GST_STATE_NULL);
        gst_object_unref(receive_pipeline);
        receive_pipeline = nullptr;
    }
}

bool VideoManager::is_active() const {
    return is_running.load();
} 