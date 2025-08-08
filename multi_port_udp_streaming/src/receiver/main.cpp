#include <iostream>
#include <string>
#include <vector>
#include <csignal>
#include "video_receiver.hpp"

using namespace udp_streaming;

std::unique_ptr<VideoReceiver> g_receiver;
std::atomic<bool> g_running(true);

void signal_handler(int signal) {
    (void)signal;
    std::cout << "\nSinyal alındı, program güvenli bir şekilde sonlandırılıyor..." << std::endl;
    g_running = false;
    if (g_receiver) {
        g_receiver->stop();
    }
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::cout << "========================================================" << std::endl;
    std::cout << "    Multi-Port UDP Video Streaming Engine - Receiver" << std::endl;
    std::cout << "========================================================" << std::endl;
    
    std::vector<uint16_t> ports = {5000, 5001, 5002, 5003};
    
    // Özel portlar belirtilmişse kullan
    if (argc > 1) {
        ports.clear();
        for (int i = 1; i < argc && i < 5; ++i) {
            ports.push_back(static_cast<uint16_t>(std::stoi(argv[i])));
        }
    }
    
    std::cout << "Ayarlar:" << std::endl;
    std::cout << "  Dinleme portları: ";
    for (size_t i = 0; i < ports.size(); ++i) {
        std::cout << ports[i];
        if (i < ports.size() - 1) std::cout << ", ";
    }
    std::cout << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;
    
    try {
        // VideoReceiver oluştur
        g_receiver = std::make_unique<VideoReceiver>(ports);
        
        if (!g_receiver->initialize()) {
            std::cerr << "VideoReceiver başlatılamadı!" << std::endl;
            return 1;
        }
        
        std::cout << "VideoReceiver başlatıldı. Video streaming dinleniyor..." << std::endl;
        std::cout << "Çıkmak için Ctrl+C tuşlarına basın." << std::endl;
        
        if (!g_receiver->start()) {
            std::cerr << "Video streaming başlatılamadı!" << std::endl;
            return 1;
        }
        
        // Ana döngü - istatistikleri göster
        while (g_running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            
            auto stats = g_receiver->get_stats();
            std::cout << "\n--- İstatistikler ---" << std::endl;
            std::cout << "  Alınan paketler: " << stats.packets_received << std::endl;
            std::cout << "  Kayıp paketler: " << stats.packets_lost << std::endl;
            std::cout << "  Çözülen frame'ler: " << stats.frames_decoded << std::endl;
            if (stats.packets_received > 0) {
                double loss_rate = (double)stats.packets_lost / stats.packets_received * 100.0;
                std::cout << "  Paket kaybı oranı: " << loss_rate << "%" << std::endl;
            }
            std::cout << "  Ortalama latency: " << stats.avg_latency_ms << " ms" << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Hata: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "Program sonlandırıldı." << std::endl;
    return 0;
}
