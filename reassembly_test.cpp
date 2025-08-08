// reassembly_test.cpp - NovaEngine Reassembly Test Uygulaması
#include "reassembly_engine.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <random>

class ReassemblyTest {
private:
    ReassemblyEngine engine_;
    std::thread sender_thread_;
    bool running_;
    
public:
    ReassemblyTest() : running_(false) {
        // Callback'leri ayarla
        engine_.set_frame_complete_callback(
            [this](uint32_t frame_id, const std::vector<uint8_t>& frame_data) {
                on_frame_complete(frame_id, frame_data);
            });
        
        engine_.set_frame_discard_callback(
            [this](uint32_t frame_id) {
                on_frame_discard(frame_id);
            });
    }
    
    bool start() {
        // Test için 2 tünel başlat
        std::vector<std::string> tunnel_ips = {"127.0.0.1", "127.0.0.1"};
        std::vector<int> tunnel_ports = {5001, 5002};
        
        if (!engine_.initialize(tunnel_ips, tunnel_ports)) {
            std::cerr << "HATA: Engine başlatılamadı!" << std::endl;
            return false;
        }
        
        // RTT simülasyonu için tünel istatistiklerini ayarla
        engine_.update_tunnel_rtt(0, 15.0); // Tunnel 0: 15ms RTT
        engine_.update_tunnel_rtt(1, 45.0); // Tunnel 1: 45ms RTT (yavaş)
        
        engine_.run();
        running_ = true;
        
        // Simülasyon thread'ini başlat
        sender_thread_ = std::thread(&ReassemblyTest::simulate_sender, this);
        
        std::cout << "✓ Reassembly test başlatıldı" << std::endl;
        return true;
    }
    
    void stop() {
        running_ = false;
        engine_.stop();
        
        if (sender_thread_.joinable()) {
            sender_thread_.join();
        }
        
        std::cout << "✓ Reassembly test durduruldu" << std::endl;
    }
    
private:
    void simulate_sender() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> delay_dist(10, 100); // 10-100ms arası gecikme
        std::uniform_int_distribution<> loss_dist(1, 100); // %1-100 loss simülasyonu
        
        uint32_t frame_id = 0;
        
        while (running_) {
            // Her frame için 4 slice oluştur
            const int total_slices = 4;
            std::vector<std::thread> slice_threads;
            
            for (int slice_id = 0; slice_id < total_slices; ++slice_id) {
                slice_threads.emplace_back([this, frame_id, slice_id, total_slices, &gen, &delay_dist, &loss_dist]() {
                    // Rastgele gecikme
                    std::this_thread::sleep_for(std::chrono::milliseconds(delay_dist(gen)));
                    
                    // Rastgele loss simülasyonu (%10 loss)
                    if (loss_dist(gen) <= 10) {
                        std::cout << "❌ Frame " << frame_id << " Slice " << slice_id << " kayboldu (simülasyon)" << std::endl;
                        return;
                    }
                    
                    // Slice verisi oluştur
                    std::vector<uint8_t> slice_data(1024, slice_id); // 1KB test verisi
                    
                    // Rastgele tünel seç (0 veya 1)
                    uint8_t tunnel_id = (gen() % 2);
                    
                    // Slice'ı gönder (simülasyon)
                    simulate_slice_send(frame_id, slice_id, total_slices, tunnel_id, slice_data);
                });
            }
            
            // Tüm slice thread'lerini bekle
            for (auto& thread : slice_threads) {
                thread.join();
            }
            
            frame_id++;
            
            // Frame'ler arası bekleme
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
    
    void simulate_slice_send(uint32_t frame_id, uint16_t slice_id, uint16_t total_slices, 
                           uint8_t tunnel_id, const std::vector<uint8_t>& data) {
        // Gerçek UDP gönderimi yerine simülasyon
        // Gerçek uygulamada burada UDP socket ile gönderim yapılır
        
        std::cout << "📤 Frame " << frame_id << " Slice " << (int)slice_id 
                  << "/" << total_slices << " (Tunnel " << (int)tunnel_id << ")" << std::endl;
        
        // Slice'ı engine'e gönder
        SliceInfo slice;
        slice.frame_id = frame_id;
        slice.slice_id = slice_id;
        slice.total_slices = total_slices;
        slice.tunnel_id = tunnel_id;
        slice.data = data;
        slice.arrival_time = std::chrono::steady_clock::now();
        
        engine_.process_slice(slice);
    }
    
    void on_frame_complete(uint32_t frame_id, const std::vector<uint8_t>& frame_data) {
        std::cout << "🎯 FRAME TAMAMLANDI: " << frame_id 
                  << " (" << frame_data.size() << " bytes)" << std::endl;
        
        // Gerçek uygulamada burada frame'i decode edip video sink'e gönderirsiniz
        // Örnek: GStreamer pipeline'a frame_data'yı gönder
    }
    
    void on_frame_discard(uint32_t frame_id) {
        std::cout << "❌ FRAME ATILDI: " << frame_id << std::endl;
        
        // Gerçek uygulamada burada frame drop istatistiklerini günceller
        // ve gerekirse FEC parametrelerini ayarlarsınız
    }
};

int main() {
    std::cout << "========================================================\n";
    std::cout << "        NovaEngine Reassembly Test\n";
    std::cout << "========================================================\n";
    std::cout << "Bu test, multi-path UDP slice reassembly sistemini simüle eder.\n";
    std::cout << "- 2 farklı RTT'li tünel (15ms ve 45ms)\n";
    std::cout << "- %10 paket kaybı simülasyonu\n";
    std::cout << "- Adaptif bekleme (RTT farkına göre)\n";
    std::cout << "- FEC recovery (placeholder)\n";
    std::cout << "========================================================\n\n";
    
    ReassemblyTest test;
    
    if (!test.start()) {
        std::cerr << "Test başlatılamadı!" << std::endl;
        return 1;
    }
    
    std::cout << "Test çalışıyor... Çıkmak için Ctrl+C basın.\n";
    
    // Ana thread'de bekle
    try {
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } catch (const std::exception& e) {
        std::cout << "\nTest durduruluyor..." << std::endl;
    }
    
    test.stop();
    return 0;
}
