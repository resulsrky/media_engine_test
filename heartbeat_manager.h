// heartbeat_manager.h - Bağlantı kontrolü
#pragma once
#include <string>
#include <thread>
#include <atomic>

class HeartbeatManager {
private:
    std::string remote_ip;
    int remote_port;
    std::atomic<bool> is_running;
    std::thread heartbeat_thread;
    
public:
    HeartbeatManager(const std::string& ip, int port);
    void start();
    void stop();
}; 