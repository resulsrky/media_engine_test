// video_chat_auto.cpp - Otomatik Optimize Video Chat
// Kamera framerate'ini donanıma göre otomatik optimize eder
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
#include <vector>
#include <algorithm>

class AutoOptimizedVideoChat {
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
    std::thread heartbeat_thread;
    
    // Otomatik optimize edilen ayarlar
    int video_width = 1280;
    int video_height = 720;
    int framerate = 30;
    int bitrate = 4000;
    std::string encoding_preset = "ultrafast";
    std::string camera_format = "image/jpeg";
    
    // Kamera desteklenen formatlar
    struct CameraFormat {
        std::string format;
        int width, height, fps;
        bool is_supported;
    };
    std::vector<CameraFormat> supported_formats;

public:
    AutoOptimizedVideoChat(const std::string& local_ip = "0.0.0.0", int local_port = 5000,
                           const std::string& remote_ip = "127.0.0.1", int remote_port = 5001) 
        : local_ip(local_ip), remote_ip(remote_ip), local_port(local_port), remote_port(remote_port),
          is_running(false), send_pipeline(nullptr), receive_pipeline(nullptr) {}
    
    ~AutoOptimizedVideoChat() {
        stop();
    }

    bool detect_camera_capabilities() {
        std::cout << "=== Kamera Yetenekleri Algılanıyor ===" << std::endl;
        
        // v4l2-ctl ile kamera formatlarını al
        std::string cmd = "v4l2-ctl --device=/dev/video0 --list-formats-ext 2>/dev/null";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            std::cerr << "Kamera formatları alınamadı!" << std::endl;
            return false;
        }
        
        char buffer[256];
        std::string result = "";
        while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
            result += buffer;
        }
        pclose(pipe);
        
        // Formatları parse et
        parse_camera_formats(result);
        
        // En iyi formatı seç
        select_optimal_format();
        
        return true;
    }
    
    void parse_camera_formats(const std::string& output) {
        std::cout << "Kamera formatları analiz ediliyor..." << std::endl;
        
        // Basit parsing - gerçek uygulamada daha gelişmiş parsing yapılabilir
        if (output.find("MJPG") != std::string::npos) {
            supported_formats.push_back({"image/jpeg", 1280, 720, 30, true});
            supported_formats.push_back({"image/jpeg", 1280, 720, 60, false}); // Test et
            supported_formats.push_back({"image/jpeg", 1920, 1080, 30, false}); // Test et
        }
        
        if (output.find("YUYV") != std::string::npos) {
            supported_formats.push_back({"video/x-raw", 1280, 720, 30, true});
            supported_formats.push_back({"video/x-raw", 1280, 720, 60, false}); // Test et
        }
        
        // Varsayılan formatlar
        if (supported_formats.empty()) {
            supported_formats.push_back({"image/jpeg", 1280, 720, 30, true});
            supported_formats.push_back({"image/jpeg", 640, 480, 30, true});
        }
        
        std::cout << "Bulunan formatlar:" << std::endl;
        for (const auto& fmt : supported_formats) {
            std::cout << "  " << fmt.format << " " << fmt.width << "x" << fmt.height 
                      << " @ " << fmt.fps << "fps" << std::endl;
        }
    }
    
    void select_optimal_format() {
        std::cout << "En optimal format seçiliyor..." << std::endl;
        
        // En yüksek kaliteyi destekleyen formatı seç
        CameraFormat* best_format = nullptr;
        int best_score = 0;
        
        for (auto& fmt : supported_formats) {
            if (!fmt.is_supported) continue;
            
            int score = fmt.width * fmt.height * fmt.fps;
            if (score > best_score) {
                best_score = score;
                best_format = &fmt;
            }
        }
        
        if (best_format) {
            camera_format = best_format->format;
            video_width = best_format->width;
            video_height = best_format->height;
            framerate = best_format->fps;
            
            // Bitrate'i framerate'e göre ayarla
            if (framerate >= 60) {
                bitrate = 6000;  // 6 Mbps for 60fps
            } else if (framerate >= 30) {
                bitrate = 4000;  // 4 Mbps for 30fps
            } else {
                bitrate = 2000;  // 2 Mbps for lower fps
            }
            
            std::cout << "Seçilen optimal format:" << std::endl;
            std::cout << "  Format: " << camera_format << std::endl;
            std::cout << "  Çözünürlük: " << video_width << "x" << video_height << std::endl;
            std::cout << "  Framerate: " << framerate << " fps" << std::endl;
            std::cout << "  Bitrate: " << bitrate << " kbps" << std::endl;
        } else {
            std::cout << "Optimal format bulunamadı, varsayılan ayarlar kullanılıyor." << std::endl;
        }
    }

    bool initialize() {
        gst_init(nullptr, nullptr);
        
        // Kamera yeteneklerini algıla
        if (!detect_camera_capabilities()) {
            std::cerr << "Kamera yetenekleri algılanamadı!" << std::endl;
            return false;
        }
        
        // Otomatik optimize edilmiş pipeline oluştur
        std::string send_pipeline_desc = create_optimized_pipeline();
        
        // Yüksek performanslı alıcı pipeline
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
        
        std::cout << "=== Otomatik Optimize Video Chat Başlatıldı ===" << std::endl;
        std::cout << "Gönderici: " << remote_ip << ":" << remote_port << std::endl;
        std::cout << "Alıcı: " << local_ip << ":" << local_port << std::endl;
        std::cout << "Optimize Edilen Kalite: " << video_width << "x" << video_height 
                  << " @ " << framerate << " fps" << std::endl;
        std::cout << "Bitrate: " << bitrate << " kbps" << std::endl;
        std::cout << "Protokol: UDP/RTP" << std::endl;
        
        return true;
    }
    
    std::string create_optimized_pipeline() {
        std::string pipeline = "v4l2src device=/dev/video0 ! ";
        
        // Format string'i oluştur
        if (camera_format == "image/jpeg") {
            pipeline += camera_format + ",width=" + std::to_string(video_width) + 
                       ",height=" + std::to_string(video_height) + 
                       ",framerate=" + std::to_string(framerate) + "/1 ! ";
            pipeline += "jpegdec ! ";
        } else {
            pipeline += camera_format + ",width=" + std::to_string(video_width) + 
                       ",height=" + std::to_string(video_height) + 
                       ",framerate=" + std::to_string(framerate) + "/1 ! ";
        }
        
        // Encoding pipeline
        pipeline += "videoconvert ! ";
        pipeline += "x264enc tune=zerolatency speed-preset=" + encoding_preset + 
                   " bitrate=" + std::to_string(bitrate) + " ! ";
        pipeline += "rtph264pay ! ";
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
        
        // Heartbeat thread'i başlat
        heartbeat_thread = std::thread([this]() {
            while (is_running) {
                send_heartbeat();
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
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
        
        if (heartbeat_thread.joinable()) {
            heartbeat_thread.join();
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
    
    // Mevcut ayarları göster
    void show_current_settings() {
        std::cout << "=== Mevcut Optimize Ayarlar ===" << std::endl;
        std::cout << "Format: " << camera_format << std::endl;
        std::cout << "Çözünürlük: " << video_width << "x" << video_height << std::endl;
        std::cout << "Framerate: " << framerate << " fps" << std::endl;
        std::cout << "Bitrate: " << bitrate << " kbps" << std::endl;
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
AutoOptimizedVideoChat* g_video_chat = nullptr;

void signal_handler(int signal) {
    if (g_video_chat) {
        std::cout << "\nProgram sonlandırılıyor..." << std::endl;
        g_video_chat->stop();
    }
    exit(0);
}

int main(int argc, char *argv[]) {
    std::cout << "=== Otomatik Optimize Video Chat ===" << std::endl;
    std::cout << "Kamera framerate'i donanıma göre otomatik optimize edilir" << std::endl;
    std::cout << "Protokol: UDP/RTP" << std::endl;
    
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
    std::cout << "Örnek: " << argv[0] << " 0.0.0.0 5000 203.0.113.1 5001" << std::endl;
    std::cout << "Varsayılan: Local: " << local_ip << ":" << local_port 
              << ", Remote: " << remote_ip << ":" << remote_port << std::endl;
    
    // Signal handler ayarla
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    AutoOptimizedVideoChat chat(local_ip, local_port, remote_ip, remote_port);
    g_video_chat = &chat;
    
    if (!chat.initialize()) {
        std::cerr << "Video Chat başlatılamadı!" << std::endl;
        return -1;
    }
    
    // Mevcut ayarları göster
    chat.show_current_settings();
    
    if (!chat.start()) {
        std::cerr << "Video Chat başlatılamadı!" << std::endl;
        return -1;
    }
    
    std::cout << "\n=== Otomatik Optimize Video Chat Aktif ===" << std::endl;
    std::cout << "Kamera görüntünüz karşı tarafa gönderiliyor..." << std::endl;
    std::cout << "Karşı tarafın görüntüsü pencerede gösteriliyor..." << std::endl;
    std::cout << "UDP Heartbeat paketleri gönderiliyor..." << std::endl;
    std::cout << "Çıkmak için Ctrl+C..." << std::endl;
    
    // Ana döngü
    while (chat.is_active()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    return 0;
} 