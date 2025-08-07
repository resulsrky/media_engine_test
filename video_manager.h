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
    std::atomic<bool> is_running;

    std::thread send_thread;
    std::thread receive_thread;

    const VideoConfig& config;
    PipelineBuilder builder;

    void run_pipeline_loop(GstElement* pipeline, const std::string& pipeline_name);

public:
    VideoManager(const VideoConfig& cfg);
    ~VideoManager();

    bool initialize(const std::string& remote_ip, int remote_port, int local_port);
    bool start();
    void stop();
    bool is_active() const;
};