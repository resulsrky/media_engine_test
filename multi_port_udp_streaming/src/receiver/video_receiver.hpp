#pragma once

#include <asio.hpp>
#include <thread>
#include <atomic>
#include <map>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include "common/packet.hpp"

namespace udp_streaming {

class VideoReceiver {
private:
    // Asio components
    asio::io_context io_context_;
    std::vector<std::unique_ptr<asio::ip::udp::socket>> sockets_;
    std::vector<asio::ip::udp::endpoint> endpoints_;
    
    // GStreamer components
    GstElement* pipeline_;
    GstElement* appsrc_;
    GstElement* decoder_;
    GstElement* appsink_;
    
    // Threading
    std::thread io_thread_;
    std::thread gst_thread_;
    std::atomic<bool> is_running_;
    
    // Jitter buffer
    struct PacketInfo {
        Packet packet;
        std::chrono::steady_clock::time_point arrival_time;
        bool is_complete = false;
    };
    
    std::map<uint32_t, PacketInfo> jitter_buffer_;
    std::mutex buffer_mutex_;
    std::condition_variable buffer_cv_;
    uint32_t expected_sequence_;
    
    // Configuration
    struct Config {
        int width = 1280;
        int height = 720;
        int framerate = 30;
        std::string decoder = "avdec_h264";
        std::vector<uint16_t> ports = {5000, 5001, 5002, 5003};
        int jitter_buffer_size = 100;
        int max_latency_ms = 150;
    } config_;
    
    // Methods
    void setup_sockets();
    void setup_gstreamer();
    void gstreamer_loop();
    void receive_packet();
    void process_packet(const Packet& packet, const asio::ip::udp::endpoint& sender);
    void jitter_buffer_loop();
    void reassemble_frame(uint32_t frame_id);
    static void on_new_sample(GstElement* sink, VideoReceiver* receiver);
    static void on_need_data(GstElement* src, guint size, VideoReceiver* receiver);
    
public:
    VideoReceiver(const std::vector<uint16_t>& ports);
    ~VideoReceiver();
    
    bool initialize();
    bool start();
    void stop();
    bool is_running() const { return is_running_.load(); }
    
    // Configuration
    void set_resolution(int width, int height);
    void set_framerate(int fps);
    void set_decoder(const std::string& decoder);
    void set_jitter_buffer_size(int size);
    void set_max_latency(int ms);
    
    // Statistics
    struct Stats {
        uint32_t packets_received = 0;
        uint32_t packets_lost = 0;
        uint32_t frames_decoded = 0;
        double avg_latency_ms = 0.0;
        double packet_loss_rate = 0.0;
    } stats_;
    
    Stats get_stats() const;
};

} // namespace udp_streaming
