// pipeline_builder.cpp - GStreamer pipeline builder implementation
#include "pipeline_builder.h"
#include "gpu_detector.h"
#include <gst/gst.h>

static bool has_element(const char* name) {
    GstElementFactory* f = gst_element_factory_find(name);
    if (f) {
        gst_object_unref(f);
        return true;
    }
    return false;
}

static std::string choose_gpu_encoder() {
    if (has_element("nvh264enc")) return "nvh264enc";
    if (has_element("vaapih264enc")) return "vaapih264enc";
    if (has_element("qsvh264enc")) return "qsvh264enc";
    if (has_element("msdkh264enc")) return "msdkh264enc";
    return "";
}

static std::string choose_gpu_decoder() {
    if (has_element("vah264dec")) return "vah264dec";
    return has_element("avdec_h264") ? std::string("avdec_h264") : std::string("avdec_h264");
}

PipelineBuilder::PipelineBuilder(const VideoConfig& cfg) : config(cfg) {}

std::string PipelineBuilder::buildSenderPipeline(const std::string& remote_ip, int remote_port) {
    std::string pipeline = "v4l2src device=/dev/video0 ! ";
    
    pipeline += "image/jpeg,width=" + std::to_string(config.width) + 
               ",height=" + std::to_string(config.height) + 
               ",framerate=" + std::to_string(config.framerate) + "/1 ! ";
    pipeline += "jpegdec ! ";
    
    // Encoder seçimi (GPU öncelikli)
    std::string gpu_enc = choose_gpu_encoder();
    bool use_vaapi_stage = (gpu_enc == "vaapih264enc");

    // Postprocess (sadece ilgili GPU zinciri aktifken VAAPI postproc)
    if (use_vaapi_stage && has_element("vaapipostproc")) {
        pipeline += "vaapipostproc ! ";
    } else {
        pipeline += "videoconvert ! ";
    }
    
    if (config.enable_mirror) {
        pipeline += "videoflip method=horizontal-flip ! ";
    }
    
    pipeline += "tee name=t ! ";
    pipeline += "queue max-size-buffers=8 max-size-time=1000000000 max-size-bytes=0 ! ";
    pipeline += "videoconvert ! autovideosink sync=false t. ! ";
    pipeline += "queue max-size-buffers=16 max-size-time=2000000000 max-size-bytes=0 ! ";
    
    if (!gpu_enc.empty()) {
        if (gpu_enc == "nvh264enc") {
            pipeline += "nvh264enc preset=hp zerolatency=true bitrate=" + std::to_string(config.bitrate) +
                       " rc=constqp qp=" + std::to_string(config.qp) +
                       " gop-size=" + std::to_string(config.gop_size) + " ! ";
        } else if (gpu_enc == "vaapih264enc") {
            pipeline += "vaapih264enc tune=low-power=false rate-control=cbr bitrate=" + std::to_string(config.bitrate) +
                       " keyframe-period=" + std::to_string(config.gop_size) + " ! ";
        } else if (gpu_enc == "qsvh264enc") {
            pipeline += "qsvh264enc bitrate=" + std::to_string(config.bitrate) +
                       " gop-size=" + std::to_string(config.gop_size) + " ! ";
        } else if (gpu_enc == "msdkh264enc") {
            pipeline += "msdkh264enc bitrate=" + std::to_string(config.bitrate) +
                       " gop-size=" + std::to_string(config.gop_size) + " ! ";
        }
    } else {
        pipeline += "x264enc tune=zerolatency speed-preset=veryfast bitrate=" + std::to_string(config.bitrate) + 
                   " key-int-max=" + std::to_string(config.gop_size) + 
                   " bframes=0 ref=1 cabac=" + std::string(config.enable_cabac ? "1" : "0") +
                   " ! ";
    }
    
    pipeline += "h264parse config-interval=1 ! ";
    pipeline += "rtph264pay pt=96 config-interval=1 mtu=1200 ! ";
    pipeline += "queue max-size-buffers=8 max-size-time=1000000000 max-size-bytes=0 ! ";
    pipeline += "udpsink host=" + remote_ip + " port=" + std::to_string(remote_port) + 
               " sync=false async=false";
    
    return pipeline;
}

std::string PipelineBuilder::buildReceiverPipeline(int local_port) {
    std::string pipeline = "udpsrc port=" + std::to_string(local_port) + 
                          " caps=\"application/x-rtp,media=video,clock-rate=90000,encoding-name=H264\" ! ";
    pipeline += "queue max-size-buffers=10 max-size-time=1500000000 max-size-bytes=0 ! ";
    pipeline += "rtph264depay ! ";
    pipeline += "h264parse ! ";
    
    std::string dec = choose_gpu_decoder();
    bool use_vaapi_stage = (dec == "vah264dec");
    pipeline += dec + " ! ";
    
    if (use_vaapi_stage && has_element("vaapipostproc")) {
        pipeline += "vaapipostproc ! ";
    } else {
        pipeline += "videoconvert ! ";
    }
    
    pipeline += "queue max-size-buffers=12 max-size-time=1500000000 max-size-bytes=0 ! ";
    if (use_vaapi_stage && has_element("vaapisink")) {
        pipeline += "vaapisink sync=false async=false";
    } else if (has_element("xvimagesink")) {
        pipeline += "xvimagesink sync=false async=false";
    } else {
        pipeline += "autovideosink sync=false async=false";
    }
    
    return pipeline;
} 