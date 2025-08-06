// nova_camera.h - Ana NovaCamera sınıfı
#pragma once
#include "video_config.h"
#include "video_manager.h"
#include "heartbeat_manager.h"
#include <memory>
#include <string>

class NovaCamera {
private:
    std::unique_ptr<VideoManager> video_manager;
    std::unique_ptr<HeartbeatManager> heartbeat_manager;
    VideoConfig config;
    std::string local_ip;
    std::string remote_ip;
    int local_port;
    int remote_port;
    
public:
    NovaCamera(const std::string& local_ip = "0.0.0.0", int local_port = 5000,
               const std::string& remote_ip = "127.0.0.1", int remote_port = 5001);
    
    bool initialize();
    bool start();
    void stop();
    bool is_active() const;
}; 