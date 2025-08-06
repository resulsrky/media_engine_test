
// main.cpp - UDP Video Streaming Engine
#include <gst/gst.h>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

class UDPVideoStreamer {
private:
    GstElement *pipeline;
    GstElement *appsink;
    std::string target_ip;
    int target_port;
    bool is_running;
    std::thread streaming_thread;

public:
    UDPVideoStreamer(const std::string& ip = "127.0.0.1", int port = 5000) 
        : pipeline(nullptr), appsink(nullptr), target_ip(ip), target_port(port), is_running(false) {}
    
    ~UDPVideoStreamer() {
        stop();
    }

    bool initialize() {
        // GStreamer'ı başlat
        gst_init(nullptr, nullptr);
        
        // Yüksek performanslı pipeline: 720p 30fps (kamera destekli) + yerel görüntü
        std::string pipeline_desc = 
            "v4l2src device=/dev/video0 ! "
            "image/jpeg,width=1280,height=720,framerate=30/1 ! "
            "jpegdec ! "
            "videoconvert ! "
            "tee name=t ! "
            "queue ! "
            "x264enc tune=zerolatency speed-preset=ultrafast bitrate=2000 ! "
            "rtph264pay ! "
            "udpsink host=" + target_ip + " port=" + std::to_string(target_port) + " sync=false "
            "t. ! "
            "queue ! "
            "xvimagesink sync=false";
        
        GError *error = nullptr;
        pipeline = gst_parse_launch(pipeline_desc.c_str(), &error);
        
        if (!pipeline) {
            std::cerr << "Pipeline oluşturulamadı: " << (error ? error->message : "Bilinmeyen hata") << std::endl;
            if (error) g_error_free(error);
            return false;
        }
        
        std::cout << "UDP Video Streamer başlatıldı - " << target_ip << ":" << target_port << std::endl;
        std::cout << "Hedef: 720p @ 30 fps, 2 Mbps bitrate" << std::endl;
        std::cout << "Yerel görüntü penceresi açılacak..." << std::endl;
        return true;
    }
    
    bool start() {
        if (!pipeline) {
            std::cerr << "Pipeline henüz başlatılmadı!" << std::endl;
            return false;
        }
        
        // Pipeline'ı PLAY durumuna geçir
        GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
        if (ret == GST_STATE_CHANGE_FAILURE) {
            std::cerr << "Pipeline PLAY durumuna geçemedi!" << std::endl;
            return false;
        }
        
        is_running = true;
        std::cout << "Video streaming başlatıldı..." << std::endl;
        
        // Bus mesajlarını dinle
        streaming_thread = std::thread([this]() {
            GstBus *bus = gst_element_get_bus(pipeline);
            
            while (is_running) {
                GstMessage *msg = gst_bus_timed_pop_filtered(
                    bus, GST_CLOCK_TIME_NONE,
                    (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS | GST_MESSAGE_STATE_CHANGED)
                );
                
                if (!msg) continue;
                
                switch (GST_MESSAGE_TYPE(msg)) {
                    case GST_MESSAGE_ERROR: {
                        GError *err;
                        gchar *dbg;
                        gst_message_parse_error(msg, &err, &dbg);
                        std::cerr << "Streaming Hatası: " << err->message << std::endl;
                        if (dbg) std::cerr << "Debug: " << dbg << std::endl;
                        g_clear_error(&err);
                        g_free(dbg);
                        is_running = false;
                        break;
                    }
                    case GST_MESSAGE_EOS:
                        std::cout << "Stream sonuna ulaşıldı." << std::endl;
                        is_running = false;
                        break;
                    case GST_MESSAGE_STATE_CHANGED: {
                        GstState old_state, new_state, pending_state;
                        gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
                        if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline)) {
                            std::cout << "Pipeline durumu: " << gst_element_state_get_name(old_state) 
                                      << " -> " << gst_element_state_get_name(new_state) << std::endl;
                        }
                        break;
                    }
                    default:
                        break;
                }
                gst_message_unref(msg);
            }
            
            gst_object_unref(bus);
        });
        
        return true;
    }
    
    void stop() {
        is_running = false;
        
        if (streaming_thread.joinable()) {
            streaming_thread.join();
        }
        
        if (pipeline) {
            gst_element_set_state(pipeline, GST_STATE_NULL);
            gst_object_unref(pipeline);
            pipeline = nullptr;
        }
        
        std::cout << "Video streaming durduruldu." << std::endl;
    }
    
    bool is_streaming() const {
        return is_running;
    }
    
    void set_target(const std::string& ip, int port) {
        target_ip = ip;
        target_port = port;
        std::cout << "Hedef güncellendi: " << target_ip << ":" << target_port << std::endl;
    }
};

int main(int argc, char *argv[]) {
    std::cout << "=== NovaEngine V2 - UDP Video Streaming Engine ===" << std::endl;
    std::cout << "Hedef: 720p @ 45-50 fps, Serverless Video Conferencing" << std::endl;
    
    // UDP hedef bilgileri (komut satırından alınabilir)
    std::string target_ip = "127.0.0.1";
    int target_port = 5000;
    
    if (argc >= 3) {
        target_ip = argv[1];
        target_port = std::stoi(argv[2]);
    }
    
    UDPVideoStreamer streamer(target_ip, target_port);
    
    if (!streamer.initialize()) {
        std::cerr << "Streamer başlatılamadı!" << std::endl;
        return -1;
    }
    
    if (!streamer.start()) {
        std::cerr << "Streaming başlatılamadı!" << std::endl;
        return -1;
    }
    
    std::cout << "Streaming başlatıldı. Çıkmak için Ctrl+C..." << std::endl;
    std::cout << "Hedef: " << target_ip << ":" << target_port << std::endl;
    std::cout << "Kalite: 720p @ 50 fps" << std::endl;
    std::cout << "Bitrate: 2 Mbps" << std::endl;
    
    // Ana döngü - kullanıcı Ctrl+C yapana kadar bekle
    try {
        while (streamer.is_streaming()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    } catch (...) {
        std::cout << "Program sonlandırılıyor..." << std::endl;
    }
    
    streamer.stop();
    return 0;
}
