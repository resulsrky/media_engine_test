#include <iostream>
#include <string>
#include <vector>
#include <csignal>
#include "video_sender.hpp"

using namespace udp_streaming;

std::unique_ptr<VideoSender> g_sender;
std::atomic<bool> g_running(true);

void signal_handler(int signal) {
    (void)signal;
    std::cout << "\nSinyal alındı, program güvenli bir şekilde sonlandırılıyor..." << std::endl;
    g_running = false;
    if (g_sender) {
        g_sender->stop();
    }
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::cout << "========================================================" << std::endl;
    std::cout << "    Multi-Port UDP Video Streaming Engine - Sender" << std::endl;
    std::cout << "========================================================" << std::endl;
    
    if (argc < 2) {
        std::cout << "Kullanım: " << argv[0] << " <hedef_ip> [port1] [port2] [port3] [port4]" << std::endl;
        std::cout << "Örnek: " << argv[0] << " 192.168.1.5 5000 5001 5002 5003" << std::endl;
        std::cout << "Varsayılan portlar: 5000, 5001, 5002, 5003" << std::endl;
        return 1;
    }
    
    std::string remote_ip = argv[1];
    std::vector<uint16_t> ports = {5000, 5001, 5002, 5003};
    
    // Özel portlar belirtilmişse kullan
    if (argc > 2) {
        ports.clear();
        for (int i = 2; i < argc && i < 6; ++i) {
            ports.push_back(static_cast<uint16_t>(std::stoi(argv[i])));
        }
    }
    
    std::cout << "Ayarlar:" << std::endl;
    std::cout << "  Hedef IP: " << remote_ip << std::endl;
    std::cout << "  Portlar: ";
    for (size_t i = 0; i < ports.size(); ++i) {
        std::cout << ports[i];
        if (i < ports.size() - 1) std::cout << ", ";
    }
    std::cout << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;
    
    try {
        // VideoSender oluştur
        g_sender = std::make_unique<VideoSender>(remote_ip, ports);
        
        if (!g_sender->initialize()) {
            std::cerr << "VideoSender başlatılamadı!" << std::endl;
            return 1;
        }
        
        std::cout << "VideoSender başlatıldı. Video streaming başlıyor..." << std::endl;
        std::cout << "Çıkmak için Ctrl+C tuşlarına basın." << std::endl;
        
        if (!g_sender->start()) {
            std::cerr << "Video streaming başlatılamadı!" << std::endl;
            return 1;
        }
        
        // Ana döngü
        while (g_running.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Hata: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "Program sonlandırıldı." << std::endl;
    return 0;
}
