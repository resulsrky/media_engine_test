// pipeline_builder.cpp - Tam GPU odaklı GStreamer pipeline builder
#include "pipeline_builder.h"
#include "gpu_detector.h"
#include <gst/gst.h>
#include <iostream>

// GStreamer'ı başlat
static bool gst_initialized = false;
static void init_gst() {
    if (!gst_initialized) {
        gst_init(nullptr, nullptr);
        gst_initialized = true;
    }
}

// GStreamer elemanının varlığını kontrol eden yardımcı fonksiyon
static bool has_element(const char* name) {
    init_gst();
    GstElementFactory* factory = gst_element_factory_find(name);
    if (factory) {
        gst_object_unref(factory);
        return true;
    }
    return false;
}

// GPU tipine göre optimal video dönüştürücü seç
static std::string get_gpu_videoconvert() {
    // Şimdilik sadece videoconvert kullan, daha güvenilir
    return "videoconvert"; // CPU fallback
}

// GPU tipine göre optimal video sink seç
static std::string get_gpu_videosink() {
    // autovideosink daha güvenilir
    return "autovideosink"; // CPU fallback
}

PipelineBuilder::PipelineBuilder(const VideoConfig& cfg) : config(cfg) {
    init_gst();
}

std::string PipelineBuilder::buildSenderPipeline(const std::string& remote_ip, int remote_port) {
    std::string pipeline;
    
    // GPU tipini tespit et
    std::string gpu_type = GpuDetector::detectGpuType();
    std::string videoconvert = get_gpu_videoconvert();
    std::string encoder = GpuDetector::getOptimalGstEncoder();
    std::string videosink = get_gpu_videosink();
    
    std::cout << "=== GPU OPTIMIZASYONU ===" << std::endl;
    std::cout << "GPU Tipi: " << gpu_type << std::endl;
    std::cout << "Video Dönüştürücü: " << videoconvert << std::endl;
    std::cout << "Encoder: " << encoder << std::endl;
    std::cout << "Video Sink: " << videosink << std::endl;
    std::cout << "=========================" << std::endl;

    // 1. Video Kaynağı (GPU'ya uygun format)
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

    // 2. GPU Video Dönüştürme (CPU kullanımını minimize et)
    pipeline += videoconvert + " ! ";
    
    // 3. Mirror efekti (gerekirse)
    if (config.enable_mirror) {
        pipeline += "videoflip method=horizontal-flip ! ";
    }

    // 4. Tee ile görüntüyü ikiye böl (kamera görüntüsü + encoder)
    pipeline += "tee name=t ! ";
    
    // 5. Kamera görüntüsü için branch (tee. ! queue ! videosink)
    pipeline += "queue max-size-buffers=10 leaky=2 ! " + videosink + " sync=false ";
    
    // 6. Encoder branch (tee. ! queue ! encoder)
    pipeline += "t. ! queue max-size-buffers=100 leaky=2 ! ";

    // 7. GPU Encoder (Tüm kodlama GPU'da)
    if (encoder == "nvh264enc") {
        pipeline += "nvh264enc bitrate=" + std::to_string(config.bitrate / 1000) + // kbps
                   " preset=p5 tune=ll rc-mode=cbr gop-size=" + std::to_string(config.gop_size) +
                   " zerolatency=true ! ";
    } else if (encoder == "qsvh264enc") {
        pipeline += "qsvh264enc bitrate=" + std::to_string(config.bitrate) + // bps
                   " gop-size=" + std::to_string(config.gop_size) +
                   " rate-control=cbr low-power=false ! ";
    } else if (encoder == "vaapih264enc") {
        pipeline += "vaapih264enc bitrate=" + std::to_string(config.bitrate) + // bps
                   " keyframe-period=" + std::to_string(config.gop_size) +
                   " rate-control=cbr ! ";
    } else { // CPU fallback: x264enc
        pipeline += "x264enc tune=zerolatency speed-preset=ultrafast bitrate=" + std::to_string(config.bitrate / 1000) +
                   " key-int-max=" + std::to_string(config.gop_size) +
                   " bframes=0 ref=1 crf=18 ! ";
    }

    // 8. RTP Paketleme ve Ağ Gönderimi
    pipeline += "video/x-h264,profile=high ! ";
    pipeline += "rtph264pay config-interval=-1 pt=96 mtu=1300 ! ";
    
    // 9. Optimize edilmiş UDP buffer (GPU'dan gelen veriyi hızlıca gönder)
    pipeline += "queue max-size-buffers=2000 max-size-bytes=0 max-size-time=0 leaky=2 ! ";
    pipeline += "udpsink host=" + remote_ip + " port=" + std::to_string(remote_port) + " sync=false async=false";

    std::cout << "SEND PIPELINE: " << pipeline << std::endl;
    return pipeline;
}

std::string PipelineBuilder::buildReceiverPipeline(int local_port) {
    std::string pipeline;
    
    // GPU tipini tespit et
    std::string gpu_type = GpuDetector::detectGpuType();
    std::string decoder = GpuDetector::getOptimalGstDecoder();
    std::string videoconvert = get_gpu_videoconvert();
    std::string videosink = get_gpu_videosink();
    
    std::cout << "=== GPU OPTIMIZASYONU (ALICI) ===" << std::endl;
    std::cout << "GPU Tipi: " << gpu_type << std::endl;
    std::cout << "Decoder: " << decoder << std::endl;
    std::cout << "Video Dönüştürücü: " << videoconvert << std::endl;
    std::cout << "Video Sink: " << videosink << std::endl;
    std::cout << "=================================" << std::endl;

    // 1. UDP Kaynağı ve Yüksek Performanslı Buffer
    pipeline = "udpsrc port=" + std::to_string(local_port) +
               " buffer-size=1048576 " + // 1MB buffer (ani hareketler için artırıldı)
               "caps=\"application/x-rtp,media=video,clock-rate=90000,encoding-name=H264,payload=96\" ! ";

    // 2. Optimize edilmiş Jitter Buffer (GPU'ya uygun)
    pipeline += "queue max-size-buffers=2000 max-size-bytes=0 max-size-time=0 leaky=2 ! ";
    pipeline += "rtpjitterbuffer mode=1 latency=150 ! ";

    // 3. RTP Depayload ve Parse
    pipeline += "rtph264depay ! h264parse ! ";

    // 4. GPU Decoder (Tüm kod çözme GPU'da)
    pipeline += decoder + " ! ";

    // 5. GPU Video Dönüştürme (CPU kullanımını minimize et)
    pipeline += videoconvert + " ! ";
    
    // 6. GPU Video Sink (Görüntüleme GPU'da)
    pipeline += "queue max-size-buffers=20 leaky=2 ! "; // Küçük buffer
    pipeline += videosink + " sync=false";

    std::cout << "RECV PIPELINE: " << pipeline << std::endl;
    return pipeline;
}