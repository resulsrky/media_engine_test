// video_chat.cpp - Karşılıklı Görüntülü İletişim
#include <gst/gst.h>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <signal.h>

class VideoChat {
private:
    GstElement *send_pipeline;
    GstElement *receive_pipeline;
    std::string local_ip;
    std::string remote_ip;
    int local_port;
    int remote_port;
    bool is_running;
    std::thread send_thread;
    std::thread receive_thread;

public:
    VideoChat(const std::string& local_ip = "0.0.0.0", int local_port = 5000,
              const std::string& remote_ip = "127.0.0.1", int remote_port = 5001) 
        : local_ip(local_ip), remote_ip(remote_ip), local_port(local_port), remote_port(remote_port),
          is_running(false), send_pipeline(nullptr), receive_pipeline(nullptr) {}
    
    ~VideoChat() {
        stop();
    }

    bool initialize() {
        gst_init(nullptr, nullptr);
        
        // Gönderici pipeline: Kamera -> UDP
        std::string send_pipeline_desc = 
            "v4l2src device=/dev/video0 ! "
            "image/jpeg,width=1280,height=720,framerate=30/1 ! "
            "jpegdec ! "
            "videoconvert ! "
            "x264enc tune=zerolatency speed-preset=ultrafast bitrate=2000 ! "
            "rtph264pay ! "
            "udpsink host=" + remote_ip + " port=" + std::to_string(remote_port) + " sync=false";
        
        // Alıcı pipeline: UDP -> Görüntü
        std::string receive_pipeline_desc = 
            "udpsrc port=" + std::to_string(local_port) + " ! "
            "application/x-rtp,media=video,clock-rate=90000,encoding-name=H264 ! "
            "rtph264depay ! "
            "h264parse ! "
            "avdec_h264 ! "
            "videoconvert ! "
            "xvimagesink sync=false";
        
        GError *error = nullptr;
        
        // Gönderici pipeline oluştur
        send_pipeline = gst_parse_launch(send_pipeline_desc.c_str(), &error);
        if (!send_pipeline) {
            std::cerr << "Gönderici pipeline oluşturulamadı: " << (error ? error->message : "Bilinmeyen hata") << std::endl;
            if (error) g_error_free(error);
            return false;
        }
        
        // Alıcı pipeline oluştur
        receive_pipeline = gst_parse_launch(receive_pipeline_desc.c_str(), &error);
        if (!receive_pipeline) {
            std::cerr << "Alıcı pipeline oluşturulamadı: " << (error ? error->message : "Bilinmeyen hata") << std::endl;
            if (error) g_error_free(error);
            gst_object_unref(send_pipeline);
            return false;
        }
        
        std::cout << "Video Chat başlatıldı!" << std::endl;
        std::cout << "Gönderici: " << remote_ip << ":" << remote_port << std::endl;
        std::cout << "Alıcı: " << local_ip << ":" << local_port << std::endl;
        std::cout << "Kalite: 720p @ 30 fps, 2 Mbps" << std::endl;
        
        return true;
    }
    
    bool start() {
        if (!send_pipeline || !receive_pipeline) {
            std::cerr << "Pipeline'lar henüz başlatılmadı!" << std::endl;
            return false;
        }
        
        is_running = true;
        
        // Gönderici thread'i başlat
        send_thread = std::thread([this]() {
            GstStateChangeReturn ret = gst_element_set_state(send_pipeline, GST_STATE_PLAYING);
            if (ret == GST_STATE_CHANGE_FAILURE) {
                std::cerr << "Gönderici pipeline PLAY durumuna geçemedi!" << std::endl;
                return;
            }
            
            std::cout << "Video gönderimi başlatıldı..." << std::endl;
            
            GstBus *bus = gst_element_get_bus(send_pipeline);
            while (is_running) {
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
        
        // Alıcı thread'i başlat
        receive_thread = std::thread([this]() {
            GstStateChangeReturn ret = gst_element_set_state(receive_pipeline, GST_STATE_PLAYING);
            if (ret == GST_STATE_CHANGE_FAILURE) {
                std::cerr << "Alıcı pipeline PLAY durumuna geçemedi!" << std::endl;
                return;
            }
            
            std::cout << "Video alımı başlatıldı..." << std::endl;
            
            GstBus *bus = gst_element_get_bus(receive_pipeline);
            while (is_running) {
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
    
    void stop() {
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
        
        std::cout << "Video Chat durduruldu." << std::endl;
    }
    
    bool is_active() const {
        return is_running;
    }
    
    void set_remote(const std::string& ip, int port) {
        remote_ip = ip;
        remote_port = port;
        std::cout << "Uzak hedef güncellendi: " << remote_ip << ":" << remote_port << std::endl;
    }
};

// Global değişken signal handler için
VideoChat* g_video_chat = nullptr;

void signal_handler(int signal) {
    if (g_video_chat) {
        std::cout << "\nProgram sonlandırılıyor..." << std::endl;
        g_video_chat->stop();
    }
    exit(0);
}

int main(int argc, char *argv[]) {
    std::cout << "=== Video Chat - Karşılıklı Görüntülü İletişim ===" << std::endl;
    
    // Komut satırı parametreleri
    std::string local_ip = "0.0.0.0";
    int local_port = 5000;
    std::string remote_ip = "127.0.0.1";
    int remote_port = 5001;
    
    if (argc >= 5) {
        local_ip = argv[1];
        local_port = std::stoi(argv[2]);
        remote_ip = argv[3];
        remote_port = std::stoi(argv[4]);
    } else if (argc >= 3) {
        remote_ip = argv[1];
        remote_port = std::stoi(argv[2]);
    }
    
    std::cout << "Kullanım: " << argv[0] << " [local_ip] [local_port] [remote_ip] [remote_port]" << std::endl;
    std::cout << "Örnek: " << argv[0] << " 0.0.0.0 5000 192.168.1.100 5001" << std::endl;
    std::cout << "Varsayılan: Local: " << local_ip << ":" << local_port 
              << ", Remote: " << remote_ip << ":" << remote_port << std::endl;
    
    // Signal handler ayarla
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    VideoChat chat(local_ip, local_port, remote_ip, remote_port);
    g_video_chat = &chat;
    
    if (!chat.initialize()) {
        std::cerr << "Video Chat başlatılamadı!" << std::endl;
        return -1;
    }
    
    if (!chat.start()) {
        std::cerr << "Video Chat başlatılamadı!" << std::endl;
        return -1;
    }
    
    std::cout << "\n=== Video Chat Aktif ===" << std::endl;
    std::cout << "Kamera görüntünüz karşı tarafa gönderiliyor..." << std::endl;
    std::cout << "Karşı tarafın görüntüsü pencerede gösteriliyor..." << std::endl;
    std::cout << "Çıkmak için Ctrl+C..." << std::endl;
    
    // Ana döngü
    while (chat.is_active()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    return 0;
} 