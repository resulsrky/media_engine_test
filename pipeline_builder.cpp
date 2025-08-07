// pipeline_builder.cpp - GStreamer pipeline builder implementation
#include "pipeline_builder.h"
#include "gpu_detector.h"
#include <gst/gst.h>
#include <iostream>

// GStreamer elemanının varlığını kontrol eden yardımcı fonksiyon
static bool has_element(const char* name) {
    GstElementFactory* factory = gst_element_factory_find(name);
    if (factory) {
        gst_object_unref(factory);
        return true;
    }
    return false;
}

PipelineBuilder::PipelineBuilder(const VideoConfig& cfg) : config(cfg) {}

std::string PipelineBuilder::buildSenderPipeline(const std::string& remote_ip, int remote_port) {
    std::string pipeline;

    // 1. Video Kaynağı (v4l2src) ve Format Belirleme
    pipeline = "v4l2src device=/dev/video0 ! ";

    // Kamera formatına göre pipeline'ı ayarla
    if (config.format == "MJPG") {
        pipeline += "image/jpeg,width=" + std::to_string(config.width) +
                   ",height=" + std::to_string(config.height) +
                   ",framerate=" + std::to_string(config.framerate) + "/1 ! jpegdec ! ";
    } else { // YUYV ve diğer raw formatlar için
        pipeline += "video/x-raw,width=" + std::to_string(config.width) +
                   ",height=" + std::to_string(config.height) +
                   ",framerate=" + std::to_string(config.framerate) + "/1 ! ";
    }

    // 2. İşleme (videoconvert, videoflip)
    pipeline += "videoconvert ! ";
    if (config.enable_mirror) {
        pipeline += "videoflip method=horizontal-flip ! ";
    }

    // 3. Encoder (GPU öncelikli)
    std::string encoder = GpuDetector::getOptimalGstEncoder();
    std::cout << "Kullanılacak Encoder: " << encoder << std::endl;

    if (encoder == "nvh264enc") {
        pipeline += "nvh264enc bitrate=" + std::to_string(config.bitrate / 1000) + // kbps to bps
                   " preset=p5 tune=ll rc-mode=cbr gop-size=" + std::to_string(config.gop_size) +
                   " ! ";
    } else if (encoder == "qsvh264enc") {
        pipeline += "qsvh264enc bitrate=" + std::to_string(config.bitrate) + // bps
                   " gop-size=" + std::to_string(config.gop_size) +
                   " rate-control=cbr ! ";
    } else if (encoder == "vaapih264enc") {
         pipeline += "vaapih264enc bitrate=" + std::to_string(config.bitrate) + // bps
                    " keyframe-period=" + std::to_string(config.gop_size) +
                    " rate-control=cbr ! ";
    } else { // CPU fallback: x264enc
        pipeline += "x264enc tune=zerolatency speed-preset=ultrafast bitrate=" + std::to_string(config.bitrate / 1000) + // bps to kbps
                   " key-int-max=" + std::to_string(config.gop_size) +
                   " bframes=0 ! ";
    }

    // 4. RTP Paketleme ve Gönderme
    pipeline += "video/x-h264,profile=high ! ";
    pipeline += "rtph264pay config-interval=-1 pt=96 mtu=1300 ! ";
    // Yüksek bitrate için queue boyutunu artır, eski frame'leri atarak senkron kal
    pipeline += "queue max-size-buffers=1000 max-size-bytes=0 max-size-time=0 leaky=2 ! ";
    pipeline += "udpsink host=" + remote_ip + " port=" + std::to_string(remote_port) + " sync=false async=false";

    return pipeline;
}

std::string PipelineBuilder::buildReceiverPipeline(int local_port) {
    std::string pipeline;

    // 1. UDP Kaynağı ve Buffer
    pipeline = "udpsrc port=" + std::to_string(local_port) +
               " buffer-size=5242880 " + // 5MB buffer
               "caps=\"application/x-rtp,media=video,clock-rate=90000,encoding-name=H264,payload=96\" ! ";

    // 2. Jitter Buffer (Ağ dalgalanmalarını düzeltmek için)
    // mode=1 (slave) ve artırılmış latency, daha kararlı bir deneyim sunar
    pipeline += "queue max-size-buffers=1000 max-size-bytes=0 max-size-time=0 leaky=2 ! ";
    pipeline += "rtpjitterbuffer mode=1 latency=200 drop-on-late=true ! ";

    // 3. Depayload ve Parse
    pipeline += "rtph264depay ! h264parse ! ";

    // 4. Decoder (VAAPI > avdec)
    std::string decoder = "avdec_h264";
    if (has_element("vaapih264dec")) {
        decoder = "vaapih264dec";
    } else if (has_element("nvh264dec")) { // NVIDIA için alternatif
        decoder = "nvh264dec";
    }
    std::cout << "Kullanılacak Decoder: " << decoder << std::endl;
    pipeline += decoder + " ! ";

    // 5. Görüntüleme
    pipeline += "videoconvert ! ";
    pipeline += "queue max-size-buffers=10 leaky=2 ! "; // Görüntülemeye gitmeden önce küçük bir buffer
    pipeline += "autovideosink sync=false";

    return pipeline;
}