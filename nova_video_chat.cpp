// nova_video_chat.cpp - Tümleşik Video Chat Uygulaması
// Tüm modlar: temel, ayna, self-view, otomatik kalite, gelişmiş
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
#include <vector>
#include <algorithm>
#include <termios.h>
#include <unistd.h>

// Kullanıcıya hem karşı tarafı hem de kendi kamerasını gösterecek pipeline için tee kullanılacak

class UnifiedVideoChat {
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
    int bitrate = 2000;
    std::string encoding_preset = "ultrafast";
    bool enable_mirror = false;
    bool enable_selfview = false;
    bool enable_auto = false;
    bool enable_advanced = false;

public:
    UnifiedVideoChat(const std::string& local_ip = "0.0.0.0", int local_port = 5000,
                     const std::string& remote_ip = "127.0.0.1", int remote_port = 5001)
        : local_ip(local_ip), remote_ip(remote_ip), local_port(local_port), remote_port(remote_port),
          is_running(false), send_pipeline(nullptr), receive_pipeline(nullptr) {}
    
    ~UnifiedVideoChat() {
        stop();
    }

    void set_mode(bool mirror, bool selfview, bool auto_mode, bool advanced) {
        enable_mirror = mirror;
        enable_selfview = selfview;
        enable_auto = auto_mode;
        enable_advanced = advanced;
    }

    bool initialize() {
        gst_init(nullptr, nullptr);
        if (enable_auto) {
            // Otomatik kalite/framerate tespiti (örnek: v4l2-ctl ile dışarıdan alınabilir)
            // Burada sabit bırakıyoruz, istenirse geliştirilebilir
            video_width = 1280;
            video_height = 720;
            framerate = 30;
            bitrate = 4000;
        } else if (enable_advanced) {
            video_width = 1280;
            video_height = 720;
            framerate = 60;
            bitrate = 4000;
        }
        std::string send_pipeline_desc = create_send_pipeline();
        std::string receive_pipeline_desc = create_receive_pipeline();
        GError *error = nullptr;
        send_pipeline = gst_parse_launch(send_pipeline_desc.c_str(), &error);
        if (!send_pipeline) {
            std::cerr << "Gönderici pipeline oluşturulamadı: " << (error ? error->message : "Bilinmeyen hata") << std::endl;
            if (error) g_error_free(error);
            return false;
        }
        receive_pipeline = gst_parse_launch(receive_pipeline_desc.c_str(), &error);
        if (!receive_pipeline) {
            std::cerr << "Alıcı pipeline oluşturulamadı: " << (error ? error->message : "Bilinmeyen hata") << std::endl;
            if (error) g_error_free(error);
            gst_object_unref(send_pipeline);
            return false;
        }
        std::cout << "=== Nova Video Chat Başlatıldı ===" << std::endl;
        std::cout << "Gönderici: " << remote_ip << ":" << remote_port << std::endl;
        std::cout << "Alıcı: " << local_ip << ":" << local_port << std::endl;
        std::cout << "Çözünürlük: " << video_width << "x" << video_height << " @ " << framerate << " fps" << std::endl;
        std::cout << "Bitrate: " << bitrate << " kbps" << std::endl;
        std::cout << "Ayna: " << (enable_mirror ? "Aktif" : "Kapalı") << ", SelfView: " << (enable_selfview ? "Aktif" : "Kapalı") << std::endl;
        std::cout << "Otomatik: " << (enable_auto ? "Aktif" : "Kapalı") << ", Gelişmiş: " << (enable_advanced ? "Aktif" : "Kapalı") << std::endl;
        return true;
    }

    std::string create_send_pipeline() {
        std::string pipeline = "v4l2src device=/dev/video0 ! ";
        pipeline += "image/jpeg,width=" + std::to_string(video_width) + ",height=" + std::to_string(video_height) + ",framerate=" + std::to_string(framerate) + "/1 ! ";
        pipeline += "jpegdec ! videoconvert ! ";
        if (enable_mirror) {
            pipeline += "videoflip method=horizontal-flip ! ";
        }
        if (enable_selfview) {
            // tee ile hem UDP'ye hem local sink'e gönder
            pipeline += "tee name=t ! queue ! x264enc tune=zerolatency speed-preset=" + encoding_preset + " bitrate=" + std::to_string(bitrate) + " ! rtph264pay ! udpsink host=" + remote_ip + " port=" + std::to_string(remote_port) + " sync=false t. ! queue ! autovideosink sync=false";
        } else {
            pipeline += "x264enc tune=zerolatency speed-preset=" + encoding_preset + " bitrate=" + std::to_string(bitrate) + " ! rtph264pay ! udpsink host=" + remote_ip + " port=" + std::to_string(remote_port) + " sync=false";
        }
        return pipeline;
    }

    std::string create_receive_pipeline() {
        return "udpsrc port=" + std::to_string(local_port) + " ! application/x-rtp,media=video,clock-rate=90000,encoding-name=H264 ! rtph264depay ! h264parse ! avdec_h264 ! videoconvert ! xvimagesink sync=false";
    }

    bool start() {
        if (!send_pipeline || !receive_pipeline) {
            std::cerr << "Pipeline'lar başlatılamadı!" << std::endl;
            return false;
        }
        is_running = true;
        send_thread = std::thread([this]() {
            GstStateChangeReturn ret = gst_element_set_state(send_pipeline, GST_STATE_PLAYING);
            if (ret == GST_STATE_CHANGE_FAILURE) {
                std::cerr << "Gönderici pipeline PLAY durumuna geçemedi!" << std::endl;
                return;
            }
            std::cout << "Video gönderimi başlatıldı..." << std::endl;
            GstBus *bus = gst_element_get_bus(send_pipeline);
            while (is_running.load()) {
                GstMessage *msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
                if (!msg) continue;
                switch (GST_MESSAGE_TYPE(msg)) {
                    case GST_MESSAGE_ERROR: {
                        GError *err; gchar *dbg;
                        gst_message_parse_error(msg, &err, &dbg);
                        std::cerr << "Gönderici Hatası: " << err->message << std::endl;
                        g_clear_error(&err); g_free(dbg); break;
                    }
                    case GST_MESSAGE_EOS:
                        std::cout << "Gönderici stream sonu." << std::endl; break;
                    default: break;
                }
                gst_message_unref(msg);
            }
            gst_object_unref(bus);
        });
        receive_thread = std::thread([this]() {
            GstStateChangeReturn ret = gst_element_set_state(receive_pipeline, GST_STATE_PLAYING);
            if (ret == GST_STATE_CHANGE_FAILURE) {
                std::cerr << "Alıcı pipeline PLAY durumuna geçemedi!" << std::endl;
                return;
            }
            std::cout << "Video alımı başlatıldı..." << std::endl;
            GstBus *bus = gst_element_get_bus(receive_pipeline);
            while (is_running.load()) {
                GstMessage *msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
                if (!msg) continue;
                switch (GST_MESSAGE_TYPE(msg)) {
                    case GST_MESSAGE_ERROR: {
                        GError *err; gchar *dbg;
                        gst_message_parse_error(msg, &err, &dbg);
                        std::cerr << "Alıcı Hatası: " << err->message << std::endl;
                        g_clear_error(&err); g_free(dbg); break;
                    }
                    case GST_MESSAGE_EOS:
                        std::cout << "Alıcı stream sonu." << std::endl; break;
                    default: break;
                }
                gst_message_unref(msg);
            }
            gst_object_unref(bus);
        });
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
        std::cout << "Video Chat durduruluyor..." << std::endl;
        is_running = false;
        // Önce pipeline'ları NULL state'e çek
        if (send_pipeline) {
            gst_element_set_state(send_pipeline, GST_STATE_NULL);
            // NULL state'e geçişi bekle
            GstState cur, pending;
            gst_element_get_state(send_pipeline, &cur, &pending, GST_CLOCK_TIME_NONE);
        }
        if (receive_pipeline) {
            gst_element_set_state(receive_pipeline, GST_STATE_NULL);
            GstState cur, pending;
            gst_element_get_state(receive_pipeline, &cur, &pending, GST_CLOCK_TIME_NONE);
        }
        // Sonra thread'leri join et
        if (send_thread.joinable() && std::this_thread::get_id() != send_thread.get_id()) send_thread.join();
        if (receive_thread.joinable() && std::this_thread::get_id() != receive_thread.get_id()) receive_thread.join();
        if (heartbeat_thread.joinable() && std::this_thread::get_id() != heartbeat_thread.get_id()) heartbeat_thread.join();
        // En son pipeline'ları unref et
        if (send_pipeline) { gst_object_unref(send_pipeline); send_pipeline = nullptr; }
        if (receive_pipeline) { gst_object_unref(receive_pipeline); receive_pipeline = nullptr; }
        std::cout << "Video Chat durduruldu." << std::endl;
    }

    bool is_active() const { return is_running.load(); }

    void send_heartbeat() {
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) return;
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(remote_port);
        addr.sin_addr.s_addr = inet_addr(remote_ip.c_str());
        const char* heartbeat_msg = "HEARTBEAT";
        sendto(sock, heartbeat_msg, strlen(heartbeat_msg), 0, (struct sockaddr*)&addr, sizeof(addr));
        close(sock);
    }
};

UnifiedVideoChat* g_video_chat = nullptr;
std::atomic<bool> g_shutdown_requested(false);

void signal_handler(int signal) {
    (void)signal;
    if (!g_shutdown_requested.exchange(true)) {
        std::cout << "\nProgram sonlandırılıyor..." << std::endl;
        if (g_video_chat) g_video_chat->stop();
    }
}

// Terminali raw moda alıp ESC tuşunu yakalamak için yardımcı fonksiyonlar
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

void print_usage(const char* prog) {
    std::cout << "Kullanım: " << prog << " [local_ip] [local_port] [remote_ip] [remote_port] [--mirror] [--selfview] [--auto] [--advanced]" << std::endl;
    std::cout << "Örnek: " << prog << " 0.0.0.0 5000 192.168.1.100 5001 --mirror --selfview" << std::endl;
    std::cout << "Varsayılan: Local: 0.0.0.0:5000, Remote: 127.0.0.1:5001" << std::endl;
}

int main(int argc, char *argv[]) {
    std::cout << "=== Nova Video Chat - Tümleşik Uygulama ===" << std::endl;
    std::string local_ip = "0.0.0.0";
    int local_port = 5000;
    std::string remote_ip = "127.0.0.1";
    int remote_port = 5001;
    bool mirror = false, selfview = false, auto_mode = false, advanced = false;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--mirror") mirror = true;
        else if (arg == "--selfview") selfview = true;
        else if (arg == "--auto") auto_mode = true;
        else if (arg == "--advanced") advanced = true;
        else if (i+1 < argc && local_ip == "0.0.0.0") { local_ip = arg; local_port = std::stoi(argv[++i]); }
        else if (i+1 < argc && remote_ip == "127.0.0.1") { remote_ip = arg; remote_port = std::stoi(argv[++i]); }
        else { print_usage(argv[0]); return 1; }
    }
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    UnifiedVideoChat chat(local_ip, local_port, remote_ip, remote_port);
    g_video_chat = &chat;
    chat.set_mode(mirror, selfview, auto_mode, advanced);
    if (!chat.initialize()) return -1;
    if (!chat.start()) return -1;
    std::cout << "\n=== Nova Video Chat Aktif ===" << std::endl;
    std::cout << "Kamera görüntünüz karşı tarafa gönderiliyor..." << std::endl;
    if (selfview) std::cout << "Aynı anda kendi görüntünüz de ayrı pencerede gösteriliyor..." << std::endl;
    std::cout << "Karşı tarafın görüntüsü pencerede gösteriliyor..." << std::endl;
    std::cout << "UDP Heartbeat paketleri gönderiliyor..." << std::endl;
    std::cout << "Çıkmak için ESC tuşuna basın..." << std::endl;

    // ESC tuşunu yakalamak için terminali raw moda al
    enable_raw_mode();
    bool esc_pressed = false;
    while (chat.is_active() && !g_shutdown_requested.load()) {
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
        if (g_video_chat) g_video_chat->stop();
        exit(0); // Tüm thread ve process'i kesin sonlandır
    }
    return 0;
}