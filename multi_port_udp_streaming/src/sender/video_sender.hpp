#pragma once

#include <asio.hpp>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include "common/packet.hpp"

namespace udp_streaming {

class VideoSender {
private:
    // Asio components
    asio::io_context io_context_;
    std::vector<std::unique_ptr<asio::ip::udp::socket>> sockets_;
    std::vector<asio::ip::udp::endpoint> endpoints_;
    
    // GStreamer components
    GstElement* pipeline_;
    GstElement* appsrc_;
    GstElement* encoder_;
    GstElement* appsink_;
    
    // Threading
    std::thread io_thread_;
    std::thread gst_thread_;
    std::atomic<bool> is_running_;
    
    // Packet management
    std::queue<Packet> packet_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    uint32_t sequence_number_;
    
    // Configuration
    struct Config {
        int width = 1280;
        int height = 720;
        int framerate = 30;
        int bitrate = 2000000; // 2 Mbps
        std::string encoder = "x264enc";
        std::vector<uint16_t> ports = {5000, 5001, 5002, 5003};
        std::string remote_ip = "127.0.0.1";
    } config_;
    
    // Methods
    void setup_sockets();
    void setup_gstreamer();
    void gstreamer_loop();
    void send_packet(const Packet& packet);
    void packetize_video_data(const uint8_t* data, size_t size, uint32_t frame_id);
    static void on_new_sample(GstElement* sink, VideoSender* sender);
    static void on_need_data(GstElement* src, guint size, VideoSender* sender);
    
public:
    VideoSender(const std::string& remote_ip, const std::vector<uint16_t>& ports);
    ~VideoSender();
    
    bool initialize();
    bool start();
    void stop();
    bool is_running() const { return is_running_.load(); }
    
    // Configuration
    void set_resolution(int width, int height);
    void set_framerate(int fps);
    void set_bitrate(int bitrate);
    void set_encoder(const std::string& encoder);
};

} // namespace udp_streaming
