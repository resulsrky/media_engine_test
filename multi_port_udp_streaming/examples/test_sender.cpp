#include <iostream>
#include <string>
#include <vector>
#include <csignal>
#include <thread>
#include <chrono>
#include "common/packet.hpp"
#include <asio.hpp>

using namespace udp_streaming;

std::atomic<bool> g_running(true);

void signal_handler(int signal) {
    (void)signal;
    std::cout << "\nSinyal alındı, program güvenli bir şekilde sonlandırılıyor..." << std::endl;
    g_running = false;
}

int main(int argc, char* argv[]) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::cout << "========================================================" << std::endl;
    std::cout << "    Multi-Port UDP Video Streaming Engine - Test Sender" << std::endl;
    std::cout << "========================================================" << std::endl;
    
    if (argc < 2) {
        std::cout << "Kullanım: " << argv[0] << " <hedef_ip> [port]" << std::endl;
        std::cout << "Örnek: " << argv[0] << " 192.168.1.5 5000" << std::endl;
        return 1;
    }
    
    std::string remote_ip = argv[1];
    uint16_t port = (argc > 2) ? static_cast<uint16_t>(std::stoi(argv[2])) : 5000;
    
    std::cout << "Ayarlar:" << std::endl;
    std::cout << "  Hedef IP: " << remote_ip << std::endl;
    std::cout << "  Port: " << port << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;
    
    try {
        asio::io_context io_context;
        asio::ip::udp::socket socket(io_context);
        socket.open(asio::ip::udp::v4());
        
        asio::ip::udp::endpoint endpoint(
            asio::ip::make_address(remote_ip), port);
        
        std::cout << "Test paketleri gönderiliyor..." << std::endl;
        
        uint32_t sequence_number = 0;
        while (g_running.load()) {
            // Test paketi oluştur
            auto packet = PacketBuilder::create_heartbeat_packet(sequence_number);
            
            // Network byte order'a çevir
            packet.to_network_order();
            
            // Gönder
            size_t sent = socket.send_to(
                asio::buffer(&packet, packet.get_total_size()), endpoint);
            
            if (sent == packet.get_total_size()) {
                std::cout << "Paket gönderildi: #" << sequence_number << std::endl;
            } else {
                std::cerr << "Paket gönderilemedi!" << std::endl;
            }
            
            sequence_number++;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Hata: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "Program sonlandırıldı." << std::endl;
    return 0;
}
