// heartbeat_manager.cpp - Bağlantı kontrolü implementation
#include "heartbeat_manager.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <cstring>

HeartbeatManager::HeartbeatManager(const std::string& ip, int port) 
    : remote_ip(ip), remote_port(port), is_running(false) {}

void HeartbeatManager::start() {
    is_running = true;
    heartbeat_thread = std::thread([this]() {
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock < 0) return;
        
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(remote_port);
        addr.sin_addr.s_addr = inet_addr(remote_ip.c_str());
        
        while (is_running.load()) {
            const char* msg = "HEARTBEAT";
            sendto(sock, msg, strlen(msg), 0,
                   (struct sockaddr*)&addr, sizeof(addr));
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        close(sock);
    });
}

void HeartbeatManager::stop() {
    is_running = false;
    if (heartbeat_thread.joinable()) {
        heartbeat_thread.join();
    }
} 