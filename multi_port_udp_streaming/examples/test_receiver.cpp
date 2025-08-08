#include <iostream>
#include <string>
#include <vector>
#include <csignal>
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
    std::cout << "    Multi-Port UDP Video Streaming Engine - Test Receiver" << std::endl;
    std::cout << "========================================================" << std::endl;
    
    uint16_t port = (argc > 1) ? static_cast<uint16_t>(std::stoi(argv[1])) : 5000;
    
    std::cout << "Ayarlar:" << std::endl;
    std::cout << "  Dinleme portu: " << port << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;
    
    try {
        asio::io_context io_context;
        asio::ip::udp::socket socket(io_context);
        socket.open(asio::ip::udp::v4());
        socket.set_option(asio::socket_base::reuse_address(true));
        socket.bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), port));
        
        std::cout << "Test paketleri dinleniyor..." << std::endl;
        
        while (g_running.load()) {
            Packet packet;
            asio::ip::udp::endpoint sender_endpoint;
            
            try {
                size_t received = socket.receive_from(
                    asio::buffer(&packet, sizeof(packet)), sender_endpoint);
                
                if (received > 0) {
                    packet.to_host_order();
                    
                    if (PacketValidator::is_valid(packet)) {
                        std::cout << "Paket alındı: #" << packet.header.sequence_number 
                                 << " (IP: " << sender_endpoint.address().to_string() 
                                 << ":" << sender_endpoint.port() << ")" << std::endl;
                        
                        if (PacketValidator::is_heartbeat_packet(packet)) {
                            std::cout << "  Tür: Heartbeat" << std::endl;
                        } else if (PacketValidator::is_video_packet(packet)) {
                            std::cout << "  Tür: Video Data" << std::endl;
                            std::cout << "  Frame ID: " << packet.header.frame_id << std::endl;
                            std::cout << "  NAL Unit ID: " << packet.header.nal_unit_id << std::endl;
                        }
                    } else {
                        std::cerr << "Geçersiz paket alındı!" << std::endl;
                    }
                }
            } catch (const std::exception& e) {
                // Socket hatası, devam et
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Hata: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "Program sonlandırıldı." << std::endl;
    return 0;
}
