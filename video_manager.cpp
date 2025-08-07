// video_manager.cpp - Video işleme yöneticisi implementation
#include "video_manager.h"
#include <iostream>

VideoManager::VideoManager(const VideoConfig& cfg)
    : send_pipeline(nullptr), receive_pipeline(nullptr), is_running(false),
      config(cfg), builder(cfg) {}

VideoManager::~VideoManager() {
    if (is_running) {
        stop();
    }
    // GStreamer'ı temizle
    gst_deinit();
}

void VideoManager::run_pipeline_loop(GstElement* pipeline, const std::string& pipeline_name) {
    GstBus *bus = gst_element_get_bus(pipeline);
    GstMessage *msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
        (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

    if (msg != NULL) {
        GError *err = nullptr;
        gchar *debug_info = nullptr;

        switch (GST_MESSAGE_TYPE(msg)) {
            case GST_MESSAGE_ERROR:
                gst_message_parse_error(msg, &err, &debug_info);
                std::cerr << "HATA (" << pipeline_name << "): " << err->message << std::endl;
                if (debug_info) {
                    std::cerr << "Debug Bilgisi: " << debug_info << std::endl;
                }
                g_clear_error(&err);
                g_free(debug_info);
                break;
            case GST_MESSAGE_EOS:
                std::cout << "BİLGİ (" << pipeline_name << "): End-Of-Stream." << std::endl;
                break;
            default:
                std::cerr << "Beklenmedik mesaj alındı." << std::endl;
                break;
        }
        gst_message_unref(msg);
    }
    gst_object_unref(bus);
    is_running = false; // Herhangi bir hata veya EOS durumunda döngüyü bitir
}


bool VideoManager::initialize(const std::string& remote_ip, int remote_port, int local_port) {
    gst_init(nullptr, nullptr);

    std::string send_pipeline_str = builder.buildSenderPipeline(remote_ip, remote_port);
    std::string receive_pipeline_str = builder.buildReceiverPipeline(local_port);

    std::cout << "\n--- GStreamer Pipeline'lar ---" << std::endl;
    std::cout << "[GÖNDERİCİ]: " << send_pipeline_str << std::endl;
    std::cout << "[ALICI]: " << receive_pipeline_str << std::endl;
    std::cout << "------------------------------\n" << std::endl;

    GError *error = nullptr;
    send_pipeline = gst_parse_launch(send_pipeline_str.c_str(), &error);
    if (!send_pipeline) {
        std::cerr << "Gönderici pipeline oluşturulamadı: " << (error ? error->message : "Bilinmeyen hata") << std::endl;
        if (error) g_error_free(error);
        return false;
    }

    error = nullptr;
    receive_pipeline = gst_parse_launch(receive_pipeline_str.c_str(), &error);
    if (!receive_pipeline) {
        std::cerr << "Alıcı pipeline oluşturulamadı: " << (error ? error->message : "Bilinmeyen hata") << std::endl;
        if (error) g_error_free(error);
        gst_object_unref(send_pipeline);
        send_pipeline = nullptr;
        return false;
    }

    return true;
}

bool VideoManager::start() {
    if (!send_pipeline || !receive_pipeline) {
        std::cerr << "Hata: Pipeline'lar başlatılamadan önce initialize() çağrılmalı." << std::endl;
        return false;
    }

    // Alıcı pipeline'ı önce başlat
    if (gst_element_set_state(receive_pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "Alıcı pipeline PLAY durumuna geçirilemedi." << std::endl;
        return false;
    }
    std::cout << "✓ Alıcı pipeline başlatıldı." << std::endl;

    // Gönderici pipeline'ı başlat
    if (gst_element_set_state(send_pipeline, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "Gönderici pipeline PLAY durumuna geçirilemedi." << std::endl;
        gst_element_set_state(receive_pipeline, GST_STATE_NULL);
        return false;
    }
    std::cout << "✓ Gönderici pipeline başlatıldı." << std::endl;

    is_running = true;

    // Thread'leri başlat
    receive_thread = std::thread(&VideoManager::run_pipeline_loop, this, receive_pipeline, "Alıcı");
    send_thread = std::thread(&VideoManager::run_pipeline_loop, this, send_pipeline, "Gönderici");

    return true;
}

void VideoManager::stop() {
    if (!is_running.exchange(false)) {
        return; // Zaten durdurulmuş
    }

    // Pipeline'lara EOS (End-of-Stream) göndererek nazikçe kapatmayı dene
    if (send_pipeline) gst_element_send_event(send_pipeline, gst_event_new_eos());
    if (receive_pipeline) gst_element_send_event(receive_pipeline, gst_event_new_eos());

    // Thread'lerin bitmesini bekle
    if (send_thread.joinable()) {
        send_thread.join();
    }
    if (receive_thread.joinable()) {
        receive_thread.join();
    }

    // Pipeline'ları NULL durumuna getir ve kaynakları serbest bırak
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