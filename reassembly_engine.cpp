// reassembly_engine.cpp - NovaEngine Multi-Path UDP Slice Reassembly Implementation
#include "reassembly_engine.h"
#include <iostream>
#include <thread>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Slice header formatı (8 byte)
struct SliceHeader {
    uint32_t frame_id;      // 4 byte
    uint16_t slice_id;      // 2 byte
    uint16_t total_slices;  // 2 byte
    uint8_t tunnel_id;      // 1 byte
    uint8_t reserved;       // 1 byte (gelecek kullanım için)
} __attribute__((packed));

ReassemblyEngine::ReassemblyEngine() 
    : epoll_fd_(-1), running_(false) {
}

ReassemblyEngine::~ReassemblyEngine() {
    stop();
    if (epoll_fd_ >= 0) {
        close(epoll_fd_);
    }
}

bool ReassemblyEngine::initialize(const std::vector<std::string>& tunnel_ips, 
                                const std::vector<int>& tunnel_ports) {
    if (tunnel_ips.size() != tunnel_ports.size()) {
        std::cerr << "HATA: Tunnel IP ve port sayıları eşleşmiyor!" << std::endl;
        return false;
    }
    
    // Epoll oluştur
    epoll_fd_ = epoll_create1(0);
    if (epoll_fd_ < 0) {
        std::cerr << "HATA: Epoll oluşturulamadı: " << strerror(errno) << std::endl;
        return false;
    }
    
    // Her tünel için socket oluştur
    for (size_t i = 0; i < tunnel_ips.size(); ++i) {
        int sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock_fd < 0) {
            std::cerr << "HATA: Socket oluşturulamadı: " << strerror(errno) << std::endl;
            return false;
        }
        
        // Non-blocking yap
        int flags = fcntl(sock_fd, F_GETFL, 0);
        fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK);
        
        // Bind
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(tunnel_ports[i]);
        addr.sin_addr.s_addr = inet_addr(tunnel_ips[i].c_str());
        
        if (bind(sock_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
            std::cerr << "HATA: Bind başarısız: " << strerror(errno) << std::endl;
            close(sock_fd);
            return false;
        }
        
        // Epoll'e ekle
        struct epoll_event ev;
        ev.events = EPOLLIN;
        ev.data.fd = sock_fd;
        
        if (epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, sock_fd, &ev) < 0) {
            std::cerr << "HATA: Epoll'e socket eklenemedi: " << strerror(errno) << std::endl;
            close(sock_fd);
            return false;
        }
        
        // Mapping'leri kaydet
        uint8_t tunnel_id = static_cast<uint8_t>(i);
        socket_to_tunnel_[sock_fd] = tunnel_id;
        tunnel_to_socket_[tunnel_id] = sock_fd;
        
        // Tünel profilini oluştur
        tunnels_[tunnel_id] = TunnelProfile{tunnel_id, 0, 0, 0, 0.0, std::chrono::steady_clock::now()};
        
        std::cout << "✓ Tunnel " << (int)tunnel_id << " başlatıldı: " 
                  << tunnel_ips[i] << ":" << tunnel_ports[i] << std::endl;
    }
    
    std::cout << "✓ Reassembly Engine başlatıldı (" << tunnel_ips.size() << " tunnel)" << std::endl;
    return true;
}

void ReassemblyEngine::run() {
    if (running_) return;
    
    running_ = true;
    worker_thread_ = std::thread(&ReassemblyEngine::worker_loop, this);
    
    std::cout << "✓ Reassembly Engine çalışıyor..." << std::endl;
}

void ReassemblyEngine::stop() {
    if (!running_) return;
    
    running_ = false;
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    
    std::cout << "✓ Reassembly Engine durduruldu" << std::endl;
}

void ReassemblyEngine::worker_loop() {
    const int MAX_EVENTS = 10;
    struct epoll_event events[MAX_EVENTS];
    
    while (running_) {
        int nfds = epoll_wait(epoll_fd_, events, MAX_EVENTS, 100); // 100ms timeout
        
        if (nfds < 0) {
            if (errno == EINTR) continue; // Interrupt, devam et
            std::cerr << "HATA: Epoll wait başarısız: " << strerror(errno) << std::endl;
            break;
        }
        
        for (int i = 0; i < nfds; ++i) {
            if (events[i].events & EPOLLIN) {
                handle_socket_event(events[i].data.fd);
            }
        }
        
        // Eski frame'leri temizle
        cleanup_old_frames();
    }
}

void ReassemblyEngine::handle_socket_event(int socket_fd) {
    process_incoming_data(socket_fd);
}

void ReassemblyEngine::process_incoming_data(int socket_fd) {
    uint8_t buffer[65536];
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    
    while (true) {
        ssize_t bytes_received = recvfrom(socket_fd, buffer, sizeof(buffer), 0,
                                         (struct sockaddr*)&client_addr, &addr_len);
        
        if (bytes_received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                break; // Non-blocking, veri yok
            }
            std::cerr << "HATA: Recvfrom başarısız: " << strerror(errno) << std::endl;
            break;
        }
        
        if (bytes_received < sizeof(SliceHeader)) {
            std::cerr << "UYARI: Çok küçük paket alındı" << std::endl;
            continue;
        }
        
        // Header'ı parse et
        SliceHeader* header = reinterpret_cast<SliceHeader*>(buffer);
        uint8_t tunnel_id = socket_to_tunnel_[socket_fd];
        
        // Slice bilgisini oluştur
        SliceInfo slice;
        slice.frame_id = ntohl(header->frame_id);
        slice.slice_id = ntohs(header->slice_id);
        slice.total_slices = ntohs(header->total_slices);
        slice.tunnel_id = tunnel_id;
        slice.arrival_time = std::chrono::steady_clock::now();
        
        // Data'yı kopyala
        size_t data_size = bytes_received - sizeof(SliceHeader);
        slice.data.assign(buffer + sizeof(SliceHeader), 
                        buffer + sizeof(SliceHeader) + data_size);
        
        // Slice'ı işle
        process_slice(slice);
        
        // Tünel istatistiklerini güncelle
        tunnels_[tunnel_id].received_packets++;
    }
}

void ReassemblyEngine::process_slice(const SliceInfo& slice) {
    auto& frame = frames_[slice.frame_id];
    
    // İlk slice ise frame'i başlat
    if (frame.slices.empty()) {
        frame.frame_id = slice.frame_id;
        frame.total_slices = slice.total_slices;
        frame.received_slices = 0;
        frame.first_slice_time = slice.arrival_time;
        frame.fec_applied = false;
        frame.discarded = false;
    }
    
    // Slice'ı ekle (eğer yoksa)
    if (frame.slices.find(slice.slice_id) == frame.slices.end()) {
        frame.slices[slice.slice_id] = slice;
        frame.received_slices++;
        frame.last_slice_time = slice.arrival_time;
        
        std::cout << "📦 Frame " << slice.frame_id << " Slice " << (int)slice.slice_id 
                  << "/" << frame.total_slices << " (Tunnel " << (int)slice.tunnel_id << ")" << std::endl;
    }
    
    // Frame tamamlandı mı kontrol et
    if (frame.is_complete()) {
        handle_frame_completion(slice.frame_id);
    } else {
        // Eksik slice'lar için adaptif bekleme
        auto missing = frame.get_missing_slices();
        if (!missing.empty()) {
            std::cout << "⏳ Frame " << slice.frame_id << " eksik: " 
                      << missing.size() << " slice" << std::endl;
            wait_for_missing_slices(frame);
        }
    }
}

void ReassemblyEngine::wait_for_missing_slices(FrameState& frame) {
    auto wait_time = calculate_adaptive_wait_time(frame);
    auto start_time = std::chrono::steady_clock::now();
    
    std::cout << "🔄 Frame " << frame.frame_id << " için " 
              << wait_time.count() << "ms bekleme..." << std::endl;
    
    while (std::chrono::steady_clock::now() - start_time < wait_time) {
        if (frame.is_complete()) {
            std::cout << "✅ Frame " << frame.frame_id << " tamamlandı!" << std::endl;
            handle_frame_completion(frame.frame_id);
            return;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // Süre doldu, FEC dene
    if (!frame.is_complete() && !frame.fec_applied) {
        std::cout << "🔧 Frame " << frame.frame_id << " için FEC uygulanıyor..." << std::endl;
        if (apply_fec_recovery(frame)) {
            handle_frame_completion(frame.frame_id);
            return;
        }
    }
    
    // FEC de başarısız, frame'i at
    if (!frame.is_complete()) {
        std::cout << "❌ Frame " << frame.frame_id << " atılıyor!" << std::endl;
        frame.discarded = true;
        if (frame_discard_callback_) {
            frame_discard_callback_(frame.frame_id);
        }
    }
}

std::chrono::milliseconds ReassemblyEngine::calculate_adaptive_wait_time(const FrameState& frame) {
    double max_rtt = get_max_rtt();
    double min_rtt = get_min_rtt();
    
    if (max_rtt == 0.0 || min_rtt == 0.0) {
        return max_wait_time_; // Varsayılan
    }
    
    // RTT farkına göre adaptif bekleme
    double rtt_diff = max_rtt - min_rtt;
    auto adaptive_wait = std::chrono::milliseconds(static_cast<int>(rtt_diff * 2)); // 2x RTT farkı
    
    // Sınırlar içinde tut
    if (adaptive_wait < min_wait_time_) adaptive_wait = min_wait_time_;
    if (adaptive_wait > max_wait_time_) adaptive_wait = max_wait_time_;
    
    return adaptive_wait;
}

void ReassemblyEngine::handle_frame_completion(uint32_t frame_id) {
    auto it = frames_.find(frame_id);
    if (it == frames_.end()) return;
    
    const auto& frame = it->second;
    
    if (frame.is_complete() && !frame.discarded) {
        // Frame'i yeniden oluştur
        auto frame_data = reconstruct_frame(frame);
        
        std::cout << "🎯 Frame " << frame_id << " tamamlandı (" 
                  << frame_data.size() << " bytes)" << std::endl;
        
        // Callback çağır
        if (frame_complete_callback_) {
            frame_complete_callback_(frame_id, frame_data);
        }
        
        // Frame'i temizle
        frames_.erase(it);
    }
}

bool ReassemblyEngine::apply_fec_recovery(FrameState& frame) {
    // TODO: ISA-L FEC implementasyonu
    // Şimdilik basit bir placeholder
    std::cout << "🔧 FEC recovery uygulanıyor (placeholder)" << std::endl;
    frame.fec_applied = true;
    return false; // Şimdilik başarısız
}

std::vector<uint8_t> ReassemblyEngine::reconstruct_frame(const FrameState& frame) {
    std::vector<uint8_t> frame_data;
    
    // Slice'ları sırayla birleştir
    for (uint16_t i = 0; i < frame.total_slices; ++i) {
        auto it = frame.slices.find(i);
        if (it != frame.slices.end()) {
            frame_data.insert(frame_data.end(), 
                            it->second.data.begin(), 
                            it->second.data.end());
        }
    }
    
    return frame_data;
}

void ReassemblyEngine::update_tunnel_rtt(uint8_t tunnel_id, double rtt_ms) {
    auto it = tunnels_.find(tunnel_id);
    if (it != tunnels_.end()) {
        it->second.update_rtt(rtt_ms);
    }
}

void ReassemblyEngine::update_tunnel_stats(uint8_t tunnel_id, uint32_t sent, 
                                         uint32_t received, uint32_t lost) {
    auto it = tunnels_.find(tunnel_id);
    if (it != tunnels_.end()) {
        it->second.sent_packets = sent;
        it->second.received_packets = received;
        it->second.lost_packets = lost;
    }
}

double ReassemblyEngine::get_max_rtt() const {
    double max_rtt = 0.0;
    for (const auto& tunnel : tunnels_) {
        if (tunnel.second.avg_rtt_ms > max_rtt) {
            max_rtt = tunnel.second.avg_rtt_ms;
        }
    }
    return max_rtt;
}

double ReassemblyEngine::get_min_rtt() const {
    double min_rtt = std::numeric_limits<double>::max();
    for (const auto& tunnel : tunnels_) {
        if (tunnel.second.avg_rtt_ms > 0.0 && tunnel.second.avg_rtt_ms < min_rtt) {
            min_rtt = tunnel.second.avg_rtt_ms;
        }
    }
    return (min_rtt == std::numeric_limits<double>::max()) ? 0.0 : min_rtt;
}

void ReassemblyEngine::cleanup_old_frames() {
    auto now = std::chrono::steady_clock::now();
    auto max_age = std::chrono::seconds(5); // 5 saniye
    
    for (auto it = frames_.begin(); it != frames_.end();) {
        if (now - it->second.last_slice_time > max_age) {
            std::cout << "🗑️ Eski frame " << it->first << " temizlendi" << std::endl;
            it = frames_.erase(it);
        } else {
            ++it;
        }
    }
}

void ReassemblyEngine::set_frame_complete_callback(std::function<void(uint32_t, const std::vector<uint8_t>&)> callback) {
    frame_complete_callback_ = callback;
}

void ReassemblyEngine::set_frame_discard_callback(std::function<void(uint32_t)> callback) {
    frame_discard_callback_ = callback;
}
