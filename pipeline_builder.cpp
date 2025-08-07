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
    // Geniş uyumluluk önceliği: NVENC -> QSV -> MSDK -> VAAPI(new/legacy) -> V4L2(var.)
    if (has_element("nvh264enc")) return "nvh264enc";
    if (has_element("qsvh264enc")) return "qsvh264enc";
    if (has_element("msdkh264enc")) return "msdkh264enc";
    if (has_element("vaapih264enc")) return "vaapih264enc";
    if (has_element("vaapiencode_h264")) return "vaapiencode_h264"; // legacy isim
    if (has_element("v4l2h264enc")) return "v4l2h264enc";
    if (has_element("v4l2video0h264enc")) return "v4l2video0h264enc";
    return ""; // CPU fallback: x264enc
}

static std::string choose_decoder() {
    // VAAPI decoder varyantları + fallback
    if (has_element("vah264dec")) return "vah264dec";
    if (has_element("vaapih264dec")) return "vaapih264dec";
    return has_element("avdec_h264") ? std::string("avdec_h264") : std::string("avdec_h264");
}

PipelineBuilder::PipelineBuilder(const VideoConfig& cfg) : config(cfg) {}

std::string PipelineBuilder::buildSenderPipeline(const std::string& remote_ip, int remote_port) {
    std::string pipeline = "v4l2src device=/dev/video0 ! ";
    
    // Kamera formatı (MJPG)
    pipeline += "image/jpeg,width=" + std::to_string(config.width) + 
               ",height=" + std::to_string(config.height) + 
               ",framerate=" + std::to_string(config.framerate) + "/1 ! ";
    pipeline += "jpegdec ! ";
    
    // Evrensel postprocess
    pipeline += "videoconvert ! ";
    if (config.enable_mirror) {
        pipeline += "videoflip method=horizontal-flip ! ";
    }
    
    // Önizleme + yayın kolları (yayın kolu non-blocking/leaky)
    pipeline += "tee name=t ! ";
    pipeline += "queue max-size-buffers=8 max-size-time=1000000000 max-size-bytes=0 ! videoconvert ! autovideosink sync=false t. ! ";
    pipeline += "queue max-size-buffers=60 max-size-time=0 max-size-bytes=0 leaky=2 ! ";
    
    // Encoder seçimi (GPU öncelikli, minimal parametrelerle)
    std::string enc = choose_gpu_encoder();
    if (!enc.empty()) {
        if (enc == "nvh264enc") {
            pipeline += "nvh264enc bitrate=" + std::to_string(config.bitrate) +
                       " gop-size=" + std::to_string(config.gop_size) + " ! ";
        } else if (enc == "qsvh264enc") {
            pipeline += "qsvh264enc bitrate=" + std::to_string(config.bitrate) +
                       " gop-size=" + std::to_string(config.gop_size) + " ! ";
        } else if (enc == "msdkh264enc") {
            pipeline += "msdkh264enc bitrate=" + std::to_string(config.bitrate) +
                       " gop-size=" + std::to_string(config.gop_size) + " ! ";
        } else if (enc == "vaapih264enc" || enc == "vaapiencode_h264") {
            // VAAPI: sürüm farklarına takılmamak için property vermiyoruz
            pipeline += enc + " ! ";
        } else if (enc == "v4l2h264enc" || enc == "v4l2video0h264enc") {
            // V4L2: cihaz bağımlı - property vermiyoruz
            pipeline += enc + " ! ";
        }
    } else {
        // CPU fallback
        pipeline += "x264enc tune=zerolatency speed-preset=veryfast bitrate=" + std::to_string(config.bitrate) + 
                   " key-int-max=" + std::to_string(config.gop_size) + 
                   " bframes=0 ref=1 cabac=" + std::string(config.enable_cabac ? "1" : "0") +
                   " ! ";
    }
    
    // RTP
    pipeline += "h264parse config-interval=1 ! rtph264pay pt=96 config-interval=1 mtu=1200 ! ";
    pipeline += "queue max-size-buffers=80 max-size-time=0 max-size-bytes=0 leaky=2 ! ";
    pipeline += "udpsink host=" + remote_ip + " port=" + std::to_string(remote_port) + " sync=false async=false";
    
    return pipeline;
}

std::string PipelineBuilder::buildReceiverPipeline(int local_port) {
    std::string pipeline = "udpsrc port=" + std::to_string(local_port) + 
                          " buffer-size=2097152 caps=\"application/x-rtp,media=video,clock-rate=90000,encoding-name=H264\" ! ";
    pipeline += "queue max-size-buffers=200 max-size-time=0 max-size-bytes=0 leaky=2 ! ";
    pipeline += "rtpjitterbuffer mode=0 latency=120 drop-on-late=true do-lost=true ! ";
    pipeline += "rtph264depay ! h264parse ! ";
    
    std::string dec = choose_decoder();
    pipeline += dec + " ! ";
    
    // Evrensel postprocess + sink
    pipeline += "videoconvert ! ";
    pipeline += "queue max-size-buffers=120 max-size-time=0 max-size-bytes=0 leaky=2 ! ";
    if (has_element("xvimagesink")) {
        pipeline += "xvimagesink sync=false async=false";
    } else {
        pipeline += "autovideosink sync=false async=false";
    }
    
    return pipeline;
} 