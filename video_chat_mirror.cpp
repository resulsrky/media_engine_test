// video_chat_mirror.cpp - Ayna Görüntülü Video Chat
// Kamera görüntüsü reverse (ayna) yapılır
#include <gst/gst.h>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <atomic>

class MirrorVideoChat {
private:
    GstElement *send_pipeline;
    GstElement *receive_pipeline;
    std::string local_ip;
    std::string remote_ip;
    int local_port;
    int remote_port;
    std::atomic<bool> is_running;
    std::thread send_thread;
    std::thread receive_thread;
    std::thread heartbeat_thread;
    
    // Video ayarları
    int video_width = 1280;
    int video_height = 720;
    int framerate = 30;
    int bitrate = 4000;
    std::string encoding_preset = "ultrafast";
    bool enable_mirror = true;  // Ayna efekti aktif

public:
    MirrorVideoChat(const std::string& local_ip = "0.0.0.0", int local_port = 5000,
                    const std::string& remote_ip = "127.0.0.1", int remote_port = 5001) 
        : local_ip(local_ip), remote_ip(remote_ip), local_port(local_port), remote_port(remote_port),
          is_running(false), send_pipeline(nullptr), receive_pipeline(nullptr) {}
    
    ~MirrorVideoChat() {
        stop();
    }

    bool initialize() {
        gst_init(nullptr, nullptr);
        
        // Ayna efektli gönderici pipeline
        std::string send_pipeline_desc = create_mirror_pipeline();
        
        // Alıcı pipeline (ayna efekti yok - karşı tarafın görüntüsü normal)
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
        
        std::cout << "=== Ayna Video Chat Başlatıldı ===" << std::endl;
        std::cout << "Gönderici: " << remote_ip << ":" << remote_port << std::endl;
        std::cout << "Alıcı: " << local_ip << ":" << local_port << std::endl;
        std::cout << "Kalite: " << video_width << "x" << video_height << " @ " << framerate << " fps" << std::endl;
        std::cout << "Bitrate: " << bitrate << " kbps" << std::endl;
        std::cout << "Ayna Efekti: " << (enable_mirror ? "Aktif" : "Pasif") << std::endl;
        std::cout << "Protokol: UDP/RTP" << std::endl;
        
        return true;
    }
    
    std::string create_mirror_pipeline() {
        std::string pipeline = "v4l2src device=/dev/video0 ! ";
        
        // Kamera formatı
        pipeline += "image/jpeg,width=" + std::to_string(video_width) + 
                   ",height=" + std::to_string(video_height) + 
                   ",framerate=" + std::to_string(framerate) + "/1 ! ";
        
        // JPEG decode
        pipeline += "jpegdec ! ";
        
        // Video convert
        pipeline += "videoconvert ! ";
        
        // Ayna efekti (horizontal flip) - sadece gönderici için
        if (enable_mirror) {
            // videoflip ile horizontal flip
            pipeline += "videoflip method=horizontal-flip ! ";
        }
        
        // Encoding
        pipeline += "x264enc tune=zerolatency speed-preset=" + encoding_preset + 
                   " bitrate=" + std::to_string(bitrate) + " ! ";
        
        // RTP paketleme
        pipeline += "rtph264pay ! ";
        
        // UDP gönderimi
        pipeline += "udpsink host=" + remote_ip + " port=" + std::to_string(remote_port) + " sync=false";
        
        return pipeline;
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
            
            std::cout << "Ayna video gönderimi başlatıldı..." << std::endl;
            
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
        
        // Alıcı thread'i başlat
        receive_thread = std::thread([this]() {
            GstStateChangeReturn ret = gst_element_set_state(receive_pipeline, GST_STATE_PLAYING);
            if (ret == GST_STATE_CHANGE_FAILURE) {
                std::cerr << "Alıcı pipeline PLAY durumuna geçemedi!" << std::endl;
                return;
            }
            
            std::cout << "Video alımı başlatıldı..." << std::endl;
            
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
        
        // Heartbeat thread'i başlat
        heartbeat_thread = std::thread([this]() {
            while (is_running.load()) {
                send_heartbeat();
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        });
        
        return true;
    }
    
    void stop() {
        std::cout << "Video Chat durduruluyor..." << std::endl;
        is_running = false;
        
        // Pipeline'ları durdur
        if (send_pipeline) {
            gst_element_set_state(send_pipeline, GST_STATE_NULL);
        }
        
        if (receive_pipeline) {
            gst_element_set_state(receive_pipeline, GST_STATE_NULL);
        }
        
        // Thread'leri bekle
        if (send_thread.joinable()) {
            send_thread.join();
        }
        
        if (receive_thread.joinable()) {
            receive_thread.join();
        }
        
        if (heartbeat_thread.joinable()) {
            heartbeat_thread.join();
        }
        
        // Pipeline'ları temizle
        if (send_pipeline) {
            gst_object_unref(send_pipeline);
            send_pipeline = nullptr;
        }
        
        if (receive_pipeline) {
            gst_object_unref(receive_pipeline);
            receive_pipeline = nullptr;
        }
        
        std::cout << "Ayna Video Chat durduruldu." << std::endl;
    }
    
    bool is_active() const {
        return is_running.load();
    }
    
    void set_mirror(bool enabled) {
        enable_mirror = enabled;
        std::cout << "Ayna efekti: " << (enable_mirror ? "Aktif" : "Pasif") << std::endl;
    }
    
    void set_remote(const std::string& ip, int port) {
        remote_ip = ip;
        remote_port = port;
        std::cout << "Uzak hedef güncellendi: " << remote_ip << ":" << remote_port << std::endl;
    }
    
    void show_settings() {
        std::cout << "=== Ayna Video Chat Ayarları ===" << std::endl;
        std::cout << "Çözünürlük: " << video_width << "x" << video_height << std::endl;
        std::cout << "Framerate: " << framerate << " fps" << std::endl;
        std::cout << "Bitrate: " << bitrate << " kbps" << std::endl;
        std::cout << "Ayna Efekti: " << (enable_mirror ? "Aktif" : "Pasif") << std::endl;
        std::cout << "Encoding Preset: " << encoding_preset << std::endl;
    }
    
private:
    void send_heartbeat() {
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) return;
        
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(remote_port);
        addr.sin_addr.s_addr = inet_addr(remote_ip.c_str());
        
        const char* heartbeat_msg = "HEARTBEAT";
        sendto(sock, heartbeat_msg, strlen(heartbeat_msg), 0, 
               (struct sockaddr*)&addr, sizeof(addr));
        
        close(sock);
    }
};

// Global değişken signal handler için
MirrorVideoChat* g_video_chat = nullptr;
std::atomic<bool> g_shutdown_requested(false);

void signal_handler(int signal) {
    (void)signal; // Unused parameter warning'ini bastır
    if (!g_shutdown_requested.exchange(true)) {
        std::cout << "\nProgram sonlandırılıyor..." << std::endl;
        if (g_video_chat) {
            g_video_chat->stop();
        }
    }
}

int main(int argc, char *argv[]) {
    std::cout << "=== Ayna Video Chat - Reverse Kamera ===" << std::endl;
    std::cout << "Kamera görüntüsü ayna efekti ile ters çevrilir" << std::endl;
    std::cout << "Protokol: UDP/RTP" << std::endl;
    
    // Komut satırı parametreleri
    std::string local_ip = "0.0.0.0";
    int local_port = 5000;
    std::string remote_ip = "127.0.0.1";
    int remote_port = 5001;
    bool mirror_enabled = true;
    
    if (argc >= 5) {
        local_ip = argv[1];
        local_port = std::stoi(argv[2]);
        remote_ip = argv[3];
        remote_port = std::stoi(argv[4]);
    } else if (argc >= 3) {
        remote_ip = argv[1];
        remote_port = std::stoi(argv[2]);
    }
    
    // Ayna efekti parametresi
    if (argc >= 6) {
        mirror_enabled = (std::string(argv[5]) == "true");
    }
    
    std::cout << "Kullanım: " << argv[0] << " [local_ip] [local_port] [remote_ip] [remote_port] [mirror]" << std::endl;
    std::cout << "Örnek: " << argv[0] << " 0.0.0.0 5000 192.168.1.100 5001 true" << std::endl;
    std::cout << "Varsayılan: Local: " << local_ip << ":" << local_port 
              << ", Remote: " << remote_ip << ":" << remote_port 
              << ", Mirror: " << (mirror_enabled ? "true" : "false") << std::endl;
    
    // Signal handler ayarla
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    MirrorVideoChat chat(local_ip, local_port, remote_ip, remote_port);
    g_video_chat = &chat;
    
    // Ayna ayarını yap
    chat.set_mirror(mirror_enabled);
    
    if (!chat.initialize()) {
        std::cerr << "Video Chat başlatılamadı!" << std::endl;
        return -1;
    }
    
    // Ayarları göster
    chat.show_settings();
    
    if (!chat.start()) {
        std::cerr << "Video Chat başlatılamadı!" << std::endl;
        return -1;
    }
    
    std::cout << "\n=== Ayna Video Chat Aktif ===" << std::endl;
    std::cout << "Kamera görüntünüz ayna efekti ile karşı tarafa gönderiliyor..." << std::endl;
    std::cout << "Karşı tarafın görüntüsü pencerede gösteriliyor..." << std::endl;
    std::cout << "UDP Heartbeat paketleri gönderiliyor..." << std::endl;
    std::cout << "Çıkmak için Ctrl+C..." << std::endl;
    
    // Ana döngü
    while (chat.is_active() && !g_shutdown_requested.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    return 0;
} 