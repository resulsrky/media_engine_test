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
    
    // Tee for self-view and streaming - buffer optimize edilmiş
    pipeline += "tee name=t ! ";
    pipeline += "queue max-size-buffers=8 max-size-time=1000000000 max-size-bytes=0 ! "; // 1 saniye buffer
    pipeline += "videoconvert ! autovideosink sync=false t. ! ";
    pipeline += "queue max-size-buffers=16 max-size-time=2000000000 max-size-bytes=0 ! "; // 2 saniye buffer
    
    // CPU encoding - basitleştirilmiş (GPU sorunları için)
    pipeline += "x264enc tune=zerolatency speed-preset=ultrafast bitrate=" + std::to_string(config.bitrate) + 
               " key-int-max=" + std::to_string(config.gop_size) + 
               " bframes=0 ref=1 ! ";
    
    // RTP ve network optimizasyonu - sabit paket boyutu
    pipeline += "h264parse config-interval=1 ! ";
    pipeline += "rtph264pay pt=96 config-interval=1 mtu=1200 ! "; // 1200 byte sabit paket
    pipeline += "queue max-size-buffers=8 max-size-time=1000000000 max-size-bytes=0 ! "; // Network buffer
    pipeline += "udpsink host=" + remote_ip + " port=" + std::to_string(remote_port) + 
               " sync=false async=false";
    
    return pipeline;
}

std::string PipelineBuilder::buildReceiverPipeline(int local_port) {
    std::string pipeline = "udpsrc port=" + std::to_string(local_port) + 
                          " caps=\"application/x-rtp,media=video,clock-rate=90000,encoding-name=H264\" ! ";
    pipeline += "queue max-size-buffers=10 max-size-time=1500000000 max-size-bytes=0 ! "; // Network receive buffer
    pipeline += "rtph264depay ! ";
    pipeline += "h264parse ! ";
    
    // CPU decoding - basitleştirilmiş (GPU sorunları için)
    pipeline += "avdec_h264 ! ";
    
    // Video display optimizasyonu - buffer artırıldı
    pipeline += "videoconvert ! ";
    pipeline += "queue max-size-buffers=12 max-size-time=1500000000 max-size-bytes=0 ! "; // 1.5 saniye buffer
    pipeline += "xvimagesink sync=false async=false";
    
    return pipeline;
} 