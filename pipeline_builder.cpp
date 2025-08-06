// pipeline_builder.cpp - GStreamer pipeline builder implementation
#include "pipeline_builder.h"
#include "gpu_detector.h"

PipelineBuilder::PipelineBuilder(const VideoConfig& cfg) : config(cfg) {}

std::string PipelineBuilder::buildSenderPipeline(const std::string& remote_ip, int remote_port) {
    std::string pipeline = "v4l2src device=/dev/video0 ! ";
    
    // Video format ayarları - kamera destekli format
    pipeline += "image/jpeg,width=" + std::to_string(config.width) + 
               ",height=" + std::to_string(config.height) + 
               ",framerate=" + std::to_string(config.framerate) + "/1 ! ";
    pipeline += "jpegdec ! ";
    
    // Video işleme
    pipeline += "videoconvert ! ";
    
    // Ayna efekti
    if (config.enable_mirror) {
        pipeline += "videoflip method=horizontal-flip ! ";
    }
    
    // Tee for self-view and streaming
    pipeline += "tee name=t ! ";
    pipeline += "queue max-size-buffers=2 ! autovideosink sync=false t. ! ";
    pipeline += "queue max-size-buffers=8 ! "; // Buffer artırıldı
    
    // GPU encoding - hızlı hareket optimizasyonu
    if (config.enable_gpu) {
        std::string gpu_codec = GPUDetector::getBestGPUCodec();
        if (gpu_codec == "h264_nvenc") {
            pipeline += "nvenc preset=p7 bitrate=" + std::to_string(config.bitrate) + 
                       " gop-size=" + std::to_string(config.gop_size) + 
                       " max-bitrate=" + std::to_string(config.bitrate * 2) + 
                       " rc-mode=vbr_hq ! ";
        } else if (gpu_codec == "h264_vaapi") {
            pipeline += "vaapih264enc bitrate=" + std::to_string(config.bitrate) + 
                       " keyframe-period=" + std::to_string(config.gop_size) + 
                       " tune=high-compression ! ";
        } else if (gpu_codec == "h264_qsv") {
            pipeline += "qsvh264enc bitrate=" + std::to_string(config.bitrate) + 
                       " gop-size=" + std::to_string(config.gop_size) + 
                       " low-power=false ! ";
        } else if (gpu_codec == "h264_v4l2") {
            pipeline += "v4l2h264enc bitrate=" + std::to_string(config.bitrate) + 
                       " gop-size=" + std::to_string(config.gop_size) + " ! ";
        } else {
            // CPU encoding - hızlı hareket optimizasyonu
            pipeline += "x264enc tune=zerolatency speed-preset=ultrafast bitrate=" + std::to_string(config.bitrate) + 
                       " key-int-max=" + std::to_string(config.gop_size) + 
                       " bframes=0 ref=1 ! ";
        }
    } else {
        // CPU encoding - hızlı hareket optimizasyonu
        pipeline += "x264enc tune=zerolatency speed-preset=ultrafast bitrate=" + std::to_string(config.bitrate) + 
                   " key-int-max=" + std::to_string(config.gop_size) + 
                   " bframes=0 ref=1 ! ";
    }
    
    pipeline += "h264parse ! rtph264pay ! ";
    pipeline += "udpsink host=" + remote_ip + " port=" + std::to_string(remote_port) + " sync=false";
    
    return pipeline;
}

std::string PipelineBuilder::buildReceiverPipeline(int local_port) {
    std::string pipeline = "udpsrc port=" + std::to_string(local_port) + " ! ";
    pipeline += "application/x-rtp,media=video,clock-rate=90000,encoding-name=H264 ! ";
    pipeline += "rtph264depay ! h264parse ! ";
    
    // GPU decoding
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
    
    pipeline += "videoconvert ! xvimagesink sync=false";
    
    return pipeline;
} 