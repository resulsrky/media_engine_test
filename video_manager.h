// video_manager.h - Video işleme yöneticisi
#pragma once
#include "video_config.h"
#include "pipeline_builder.h"
#include <gst/gst.h>
#include <thread>
#include <atomic>
#include <string>
#include <memory>

class VideoManager {
private:
    GstElement* send_pipeline;
    GstElement* receive_pipeline;
    GstElement* self_view_pipeline;  // Kendi görüntüsü için ayrı pipeline
    std::atomic<bool> is_running;

    std::thread send_thread;
    std::thread receive_thread;
    std::thread self_view_thread;  // Kendi görüntüsü için thread

    const VideoConfig& config;
    PipelineBuilder builder;

    void run_pipeline_loop(GstElement* pipeline, const std::string& pipeline_name);
    std::string buildSelfViewPipeline();  // Kendi görüntüsü için pipeline oluştur

public:
    VideoManager(const VideoConfig& cfg);
    ~VideoManager();

    bool initialize(const std::string& remote_ip, int remote_port, int local_port);
    bool start();
    void stop();
    bool is_active() const;
};