// pipeline_builder.h - GStreamer pipeline builder
#pragma once
#include "video_config.h"
#include <string>

class PipelineBuilder {
private:
    VideoConfig config;
    
public:
    PipelineBuilder(const VideoConfig& cfg);
    std::string buildSenderPipeline(const std::string& remote_ip, int remote_port);
    std::string buildReceiverPipeline(int local_port);
}; 