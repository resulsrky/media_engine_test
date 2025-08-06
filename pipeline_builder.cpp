// pipeline_builder.cpp - GStreamer pipeline builder implementation
#include "pipeline_builder.h"
#include "gpu_detector.h"

PipelineBuilder::PipelineBuilder(const VideoConfig& cfg) : config(cfg) {}

std::string PipelineBuilder::buildSenderPipeline(const std::string& remote_ip, int remote_port) {
    std::string pipeline = "v4l2src device=/dev/video0 ! ";
    
    // Video format ayarları - MJPG formatı kullan
    pipeline += "image/jpeg,width=" + std::to_string(config.width) + 
               ",height=" + std::to_string(config.height) + 
               ",framerate=" + std::to_string(config.framerate) + "/1 ! ";
    pipeline += "jpegdec ! ";
    
    // Video işleme - basitleştirilmiş
    pipeline += "videoconvert ! ";
    
    // Ayna efekti
    if (config.enable_mirror) {
        pipeline += "videoflip method=horizontal-flip ! ";
    }
    
    // Tee for self-view and streaming - optimize edilmiş
    pipeline += "tee name=t ! ";
    pipeline += "queue max-size-buffers=2 max-size-time=0 max-size-bytes=0 ! ";
    pipeline += "videoconvert ! autovideosink sync=false t. ! ";
    pipeline += "queue max-size-buffers=4 max-size-time=0 max-size-bytes=0 ! "; // Optimize buffer
    
    // CPU encoding - basitleştirilmiş (GPU sorunları için)
    pipeline += "x264enc tune=zerolatency speed-preset=ultrafast bitrate=" + std::to_string(config.bitrate) + 
               " key-int-max=" + std::to_string(config.gop_size) + 
               " bframes=0 ref=1 ! ";
    
    // RTP ve network optimizasyonu
    pipeline += "h264parse config-interval=1 ! ";
    pipeline += "rtph264pay pt=96 config-interval=1 ! ";
    pipeline += "udpsink host=" + remote_ip + " port=" + std::to_string(remote_port) + 
               " sync=false async=false";
    
    return pipeline;
}

std::string PipelineBuilder::buildReceiverPipeline(int local_port) {
    std::string pipeline = "udpsrc port=" + std::to_string(local_port) + 
                          " caps=\"application/x-rtp,media=video,clock-rate=90000,encoding-name=H264\" ! ";
    pipeline += "rtph264depay ! ";
    pipeline += "h264parse ! ";
    
    // GPU decoding - gelişmiş
    if (config.enable_gpu) {
        std::string gpu_codec = GPUDetector::getBestGPUCodec();
        if (gpu_codec == "h264_vaapi") {
            pipeline += "vaapidecode ! ";
        } else if (gpu_codec == "h264_qsv") {
            pipeline += "qsvh264dec ! ";
        } else {
            pipeline += "avdec_h264 ! "; // CPU decoding
        }
    } else {
        pipeline += "avdec_h264 ! ";
    }
    
    // Video display optimizasyonu
    pipeline += "videoconvert ! ";
    pipeline += "queue max-size-buffers=2 max-size-time=0 max-size-bytes=0 ! ";
    pipeline += "xvimagesink sync=false async=false";
    
    return pipeline;
} 