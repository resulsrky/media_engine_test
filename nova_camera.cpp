// nova_camera.cpp - Tek Kamera Uygulaması
// Tüm görevleri otomatik olarak gerçekleştirir
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
#include <termios.h>

// Terminal kontrolü için
typedef struct termios termios_t;
termios_t orig_termios;

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    termios_t raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

class NovaCamera {
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
    
    // Otomatik optimize edilmiş ayarlar
    int video_width = 1280;
    int video_height = 720;
    int framerate = 30;  // 30fps geri getirildi
    int bitrate = 500;   // 2000kbps'den 500kbps'e düşürüldü (donma için)
    std::string encoding_preset = "ultrafast";
    bool use_gpu_acceleration = true;  // GPU acceleration aktif

public:
    NovaCamera(const std::string& local_ip = "0.0.0.0", int local_port = 5000,
               const std::string& remote_ip = "127.0.0.1", int remote_port = 5001)
        : local_ip(local_ip), remote_ip(remote_ip), local_port(local_port), remote_port(remote_port),
          is_running(false), send_pipeline(nullptr), receive_pipeline(nullptr) {}
    
    ~NovaCamera() {
        stop();
    }

    bool initialize() {
        gst_init(nullptr, nullptr);
        
        // GPU detection
        bool gpu_available = false;
        GstElement *test_gpu = gst_element_factory_make("vaapih264enc", "test");
        if (test_gpu) {
            gst_object_unref(test_gpu);
            gpu_available = true;
            std::cout << "✓ GPU acceleration (VAAPI) bulundu" << std::endl;
        } else {
            use_gpu_acceleration = false;
            std::cout << "⚠ GPU acceleration bulunamadı, CPU encoding kullanılacak" << std::endl;
        }
        
        std::cout << "=== Nova Camera - Tek Kamera Uygulaması ===" << std::endl;
        std::cout << "Otomatik olarak tüm görevleri gerçekleştirir:" << std::endl;
        std::cout << "✓ Kamera görüntüsü alımı" << std::endl;
        std::cout << "✓ Ayna efekti (doğal görünüm)" << std::endl;
        std::cout << "✓ Kendini görme penceresi" << std::endl;
        std::cout << "✓ Karşı tarafı görme penceresi" << std::endl;
        std::cout << "✓ UDP video gönderimi" << std::endl;
        std::cout << "✓ UDP video alımı" << std::endl;
        std::cout << "✓ Otomatik kalite optimizasyonu" << std::endl;
        std::cout << "✓ ESC ile düzgün kapanma" << std::endl;
        
        std::string send_pipeline_desc = create_send_pipeline();
        std::string receive_pipeline_desc = create_receive_pipeline();
        
        GError *error = nullptr;
        
        send_pipeline = gst_parse_launch(send_pipeline_desc.c_str(), &error);
        if (!send_pipeline) {
            std::cerr << "Gönderici pipeline oluşturulamadı: " << (error ? error->message : "Bilinmeyen hata") << std::endl;
            if (error) g_error_free(error);
            
            // GPU başarısız olduysa CPU'ya geç
            if (use_gpu_acceleration) {
                std::cout << "GPU encoding başarısız, CPU encoding deneniyor..." << std::endl;
                use_gpu_acceleration = false;
                send_pipeline_desc = create_send_pipeline();
                send_pipeline = gst_parse_launch(send_pipeline_desc.c_str(), &error);
                if (!send_pipeline) {
                    std::cerr << "CPU encoding de başarısız: " << (error ? error->message : "Bilinmeyen hata") << std::endl;
                    if (error) g_error_free(error);
                    return false;
                }
            } else {
                return false;
            }
        }
        
        receive_pipeline = gst_parse_launch(receive_pipeline_desc.c_str(), &error);
        if (!receive_pipeline) {
            std::cerr << "Alıcı pipeline oluşturulamadı: " << (error ? error->message : "Bilinmeyen hata") << std::endl;
            if (error) g_error_free(error);
            gst_object_unref(send_pipeline);
            return false;
        }
        
        std::cout << "\n=== Nova Camera Başlatıldı ===" << std::endl;
        std::cout << "Gönderici: " << remote_ip << ":" << remote_port << std::endl;
        std::cout << "Alıcı: " << local_ip << ":" << local_port << std::endl;
        std::cout << "Çözünürlük: " << video_width << "x" << video_height << " @ " << framerate << " fps" << std::endl;
        std::cout << "Bitrate: " << bitrate << " kbps" << std::endl;
        std::cout << "Ayna Efekti: Aktif (doğal görünüm)" << std::endl;
        std::cout << "Self-View: Aktif" << std::endl;
        std::cout << "GPU Acceleration: " << (use_gpu_acceleration ? "Aktif (VAAPI)" : "Pasif (CPU)") << std::endl;
        std::cout << "Protokol: UDP/RTP" << std::endl;
        
        return true;
    }
    
    std::string create_send_pipeline() {
        // Basitleştirilmiş ve stabil pipeline
        std::string pipeline = "v4l2src device=/dev/video0 ! ";
        pipeline += "image/jpeg,width=" + std::to_string(video_width) + 
                   ",height=" + std::to_string(video_height) + 
                   ",framerate=" + std::to_string(framerate) + "/1 ! ";
        pipeline += "jpegdec ! videoconvert ! ";
        pipeline += "videoflip method=horizontal-flip ! "; // Ayna efekti
        pipeline += "tee name=t ! queue max-size-buffers=2 ! autovideosink sync=false t. ! ";
        pipeline += "queue max-size-buffers=2 ! ";
        
        // GPU acceleration seçimi (fallback ile)
        if (use_gpu_acceleration) {
            // VAAPI (Intel GPU) dene, bulunamazsa CPU'ya geç
            pipeline += "vaapih264enc bitrate=" + std::to_string(bitrate) + " ! ";
            pipeline += "h264parse ! ";
        } else {
            // CPU encoding
            pipeline += "x264enc tune=zerolatency speed-preset=ultrafast bitrate=" + std::to_string(bitrate) + " ! ";
        }
        
        pipeline += "rtph264pay ! ";
        pipeline += "udpsink host=" + remote_ip + " port=" + std::to_string(remote_port) + " sync=false";
        
        return pipeline;
    }
    
    std::string create_receive_pipeline() {
        // Basitleştirilmiş alıcı pipeline
        return "udpsrc port=" + std::to_string(local_port) + " ! "
               "application/x-rtp,media=video,clock-rate=90000,encoding-name=H264 ! "
               "rtph264depay ! h264parse ! "
               "avdec_h264 ! "  // CPU decoding (daha güvenilir)
               "videoconvert ! xvimagesink sync=false";
    }
    
    bool start() {
        if (!send_pipeline || !receive_pipeline) {
            std::cerr << "Pipeline'lar başlatılamadı!" << std::endl;
            return false;
        }
        
        is_running = true;
        
        // Gönderici thread'i (kamera + self-view + UDP gönderimi)
        send_thread = std::thread([this]() {
            GstStateChangeReturn ret = gst_element_set_state(send_pipeline, GST_STATE_PLAYING);
            if (ret == GST_STATE_CHANGE_FAILURE) {
                std::cerr << "Gönderici pipeline PLAY durumuna geçemedi!" << std::endl;
                return;
            }
            
            std::cout << "✓ Kamera görüntüsü alınıyor..." << std::endl;
            std::cout << "✓ Ayna efekti uygulanıyor..." << std::endl;
            std::cout << "✓ Kendini görme penceresi açıldı..." << std::endl;
            std::cout << "✓ UDP video gönderimi başlatıldı..." << std::endl;
            
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
        
        // Alıcı thread'i (karşı taraf görüntüsü)
        receive_thread = std::thread([this]() {
            GstStateChangeReturn ret = gst_element_set_state(receive_pipeline, GST_STATE_PLAYING);
            if (ret == GST_STATE_CHANGE_FAILURE) {
                std::cerr << "Alıcı pipeline PLAY durumuna geçemedi!" << std::endl;
                return;
            }
            
            std::cout << "✓ Karşı taraf görüntüsü penceresi açıldı..." << std::endl;
            std::cout << "✓ UDP video alımı başlatıldı..." << std::endl;
            
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
        
        // Heartbeat thread'i (bağlantı kontrolü)
        heartbeat_thread = std::thread([this]() {
            while (is_running.load()) {
                send_heartbeat();
                std::this_thread::sleep_for(std::chrono::seconds(5));
            }
        });
        
        return true;
    }
    
    void stop() {
        static std::atomic<bool> already_stopping(false);
        if (already_stopping.exchange(true)) return;
        
        std::cout << "\n=== Nova Camera Kapatılıyor ===" << std::endl;
        is_running = false;
        
        // Pipeline'ları durdur
        if (send_pipeline) {
            gst_element_set_state(send_pipeline, GST_STATE_NULL);
            GstState cur, pending;
            gst_element_get_state(send_pipeline, &cur, &pending, GST_CLOCK_TIME_NONE);
        }
        
        if (receive_pipeline) {
            gst_element_set_state(receive_pipeline, GST_STATE_NULL);
            GstState cur, pending;
            gst_element_get_state(receive_pipeline, &cur, &pending, GST_CLOCK_TIME_NONE);
        }
        
        // Thread'leri bekle
        if (send_thread.joinable() && std::this_thread::get_id() != send_thread.get_id()) {
            send_thread.join();
        }
        
        if (receive_thread.joinable() && std::this_thread::get_id() != receive_thread.get_id()) {
            receive_thread.join();
        }
        
        if (heartbeat_thread.joinable() && std::this_thread::get_id() != heartbeat_thread.get_id()) {
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
        
        std::cout << "✓ Nova Camera durduruldu." << std::endl;
    }
    
    bool is_active() const {
        return is_running.load();
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

// Global değişkenler
NovaCamera* g_camera = nullptr;
std::atomic<bool> g_shutdown_requested(false);

void signal_handler(int signal) {
    (void)signal;
    if (!g_shutdown_requested.exchange(true)) {
        std::cout << "\nProgram sonlandırılıyor..." << std::endl;
        if (g_camera) g_camera->stop();
        exit(0);
    }
}

int main(int argc, char *argv[]) {
    std::cout << "=== Nova Camera - Tek Kamera Uygulaması ===" << std::endl;
    std::cout << "Tüm görevleri otomatik olarak gerçekleştirir" << std::endl;
    
    // Komut satırı parametreleri (basit)
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
    
    NovaCamera camera(local_ip, local_port, remote_ip, remote_port);
    g_camera = &camera;
    
    if (!camera.initialize()) {
        std::cerr << "Nova Camera başlatılamadı!" << std::endl;
        return -1;
    }
    
    if (!camera.start()) {
        std::cerr << "Nova Camera başlatılamadı!" << std::endl;
        return -1;
    }
    
    std::cout << "\n=== Nova Camera Aktif ===" << std::endl;
    std::cout << "✓ Tüm görevler otomatik olarak çalışıyor..." << std::endl;
    std::cout << "✓ Kamera görüntüsü ayna efekti ile işleniyor..." << std::endl;
    std::cout << "✓ Kendini görme penceresi açık..." << std::endl;
    std::cout << "✓ Karşı taraf görüntüsü penceresi açık..." << std::endl;
    std::cout << "✓ UDP video gönderimi/alımı aktif..." << std::endl;
    std::cout << "✓ Heartbeat paketleri gönderiliyor..." << std::endl;
    std::cout << "Çıkmak için ESC tuşuna basın..." << std::endl;
    
    // ESC tuşunu yakalamak için terminali raw moda al
    enable_raw_mode();
    bool esc_pressed = false;
    
    while (camera.is_active() && !g_shutdown_requested.load()) {
        fd_set set;
        struct timeval timeout = {0, 100000}; // 100ms
        FD_ZERO(&set);
        FD_SET(STDIN_FILENO, &set);
        int rv = select(STDIN_FILENO+1, &set, NULL, NULL, &timeout);
        if (rv > 0 && FD_ISSET(STDIN_FILENO, &set)) {
            char c = 0;
            ssize_t n = read(STDIN_FILENO, &c, 1);
            if (n > 0 && c == 27) { // ESC
                esc_pressed = true;
                break;
            }
        }
    }
    
    disable_raw_mode();
    
    if (esc_pressed) {
        std::cout << "\nESC tuşuna basıldı, program kapatılıyor..." << std::endl;
        if (g_camera) g_camera->stop();
        exit(0);
    }
    
    return 0;
} 