// reassembly_engine.h - NovaEngine Multi-Path UDP Slice Reassembly
#pragma once

#include <map>
#include <vector>
#include <queue>
#include <chrono>
#include <memory>
#include <functional>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <thread>

// Slice bilgisi
struct SliceInfo {
    uint32_t frame_id;      // Frame ID
    uint16_t slice_id;      // Slice ID (0-255)
    uint16_t total_slices;  // Toplam slice sayısı
    uint8_t tunnel_id;      // Hangi tünelden geldi
    std::vector<uint8_t> data; // Slice verisi
    std::chrono::steady_clock::time_point arrival_time;
};

// Tünel profili
struct TunnelProfile {
    uint8_t tunnel_id;
    uint32_t sent_packets;
    uint32_t received_packets;
    uint32_t lost_packets;
    double avg_rtt_ms;
    std::chrono::steady_clock::time_point last_update;
    
    // RTT hesaplama
    void update_rtt(double rtt_ms) {
        if (avg_rtt_ms == 0.0) {
            avg_rtt_ms = rtt_ms;
        } else {
            avg_rtt_ms = avg_rtt_ms * 0.9 + rtt_ms * 0.1; // Exponential moving average
        }
        last_update = std::chrono::steady_clock::now();
    }
    
    // Loss rate hesaplama
    double get_loss_rate() const {
        if (sent_packets == 0) return 0.0;
        return static_cast<double>(lost_packets) / sent_packets;
    }
};

// Frame durumu
struct FrameState {
    uint32_t frame_id;
    std::map<uint16_t, SliceInfo> slices; // slice_id -> SliceInfo
    uint16_t total_slices;
    uint16_t received_slices;
    std::chrono::steady_clock::time_point first_slice_time;
    std::chrono::steady_clock::time_point last_slice_time;
    bool fec_applied;
    bool discarded;
    
    // Eksik slice'ları bul
    std::vector<uint16_t> get_missing_slices() const {
        std::vector<uint16_t> missing;
        for (uint16_t i = 0; i < total_slices; ++i) {
            if (slices.find(i) == slices.end()) {
                missing.push_back(i);
            }
        }
        return missing;
    }
    
    // Tamamlanma durumu
    bool is_complete() const {
        return received_slices >= total_slices;
    }
    
    // Eksik slice sayısı
    uint16_t get_missing_count() const {
        return total_slices - received_slices;
    }
};

// Reassembly Engine ana sınıfı
class ReassemblyEngine {
public:
    ReassemblyEngine();
    ~ReassemblyEngine();
    
    // Ana fonksiyonlar
    bool initialize(const std::vector<std::string>& tunnel_ips, 
                   const std::vector<int>& tunnel_ports);
    void run();
    void stop();
    
    // Slice işleme
    void process_slice(const SliceInfo& slice);
    void handle_frame_completion(uint32_t frame_id);
    
    // RTT ve tünel yönetimi
    void update_tunnel_rtt(uint8_t tunnel_id, double rtt_ms);
    void update_tunnel_stats(uint8_t tunnel_id, uint32_t sent, uint32_t received, uint32_t lost);
    
    // Callback'ler
    void set_frame_complete_callback(std::function<void(uint32_t, const std::vector<uint8_t>&)> callback);
    void set_frame_discard_callback(std::function<void(uint32_t)> callback);
    
private:
    // Epoll ve socket yönetimi
    int epoll_fd_;
    std::map<int, uint8_t> socket_to_tunnel_; // socket_fd -> tunnel_id
    std::map<uint8_t, int> tunnel_to_socket_; // tunnel_id -> socket_fd
    
    // Frame ve tünel durumları
    std::map<uint32_t, FrameState> frames_; // frame_id -> FrameState
    std::map<uint8_t, TunnelProfile> tunnels_; // tunnel_id -> TunnelProfile
    
    // Adaptif bekleme parametreleri
    std::chrono::milliseconds max_wait_time_{200}; // Maksimum bekleme süresi
    std::chrono::milliseconds min_wait_time_{10};  // Minimum bekleme süresi
    
    // Callback'ler
    std::function<void(uint32_t, const std::vector<uint8_t>&)> frame_complete_callback_;
    std::function<void(uint32_t)> frame_discard_callback_;
    
    // Çalışma durumu
    bool running_;
    std::thread worker_thread_;
    
    // Private fonksiyonlar
    void worker_loop();
    void handle_socket_event(int socket_fd);
    void process_incoming_data(int socket_fd);
    
    // Adaptif bekleme
    std::chrono::milliseconds calculate_adaptive_wait_time(const FrameState& frame);
    void wait_for_missing_slices(FrameState& frame);
    
    // FEC işlemleri
    bool apply_fec_recovery(FrameState& frame);
    std::vector<uint8_t> reconstruct_frame(const FrameState& frame);
    
    // Yardımcı fonksiyonlar
    double get_max_rtt() const;
    double get_min_rtt() const;
    void cleanup_old_frames();
    void log_frame_stats(uint32_t frame_id, const FrameState& frame);
};
