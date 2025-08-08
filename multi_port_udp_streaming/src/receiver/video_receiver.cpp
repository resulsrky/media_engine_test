#include "video_receiver.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <chrono>

namespace udp_streaming {

VideoReceiver::VideoReceiver(const std::vector<uint16_t>& ports)
    : pipeline_(nullptr), appsrc_(nullptr), decoder_(nullptr), appsink_(nullptr)
    , is_running_(false), expected_sequence_(0) {
    
    config_.ports = ports;
    
    std::cout << "VideoReceiver oluşturuldu:" << std::endl;
    std::cout << "  Dinleme portları: ";
    for (size_t i = 0; i < config_.ports.size(); ++i) {
        std::cout << config_.ports[i];
        if (i < config_.ports.size() - 1) std::cout << ", ";
    }
    std::cout << std::endl;
}

VideoReceiver::~VideoReceiver() {
    stop();
}

bool VideoReceiver::initialize() {
    try {
        setup_sockets();
        setup_gstreamer();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "VideoReceiver initialize hatası: " << e.what() << std::endl;
        return false;
    }
}

void VideoReceiver::setup_sockets() {
    for (uint16_t port : config_.ports) {
        auto socket = std::make_unique<asio::ip::udp::socket>(io_context_);
        socket->open(asio::ip::udp::v4());
        socket->set_option(asio::socket_base::reuse_address(true));
        socket->bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), port));
        
        sockets_.push_back(std::move(socket));
        
        std::cout << "Socket oluşturuldu ve dinleniyor: 0.0.0.0:" << port << std::endl;
    }
}

void VideoReceiver::setup_gstreamer() {
    gst_init(nullptr, nullptr);
    
    // GStreamer pipeline oluştur
    std::stringstream pipeline_str;
    pipeline_str << "appsrc name=src ! "
                 << "video/x-h264,profile=high ! "
                 << config_.decoder << " ! "
                 << "videoconvert ! "
                 << "video/x-raw,width=" << config_.width 
                 << ",height=" << config_.height 
                 << ",framerate=" << config_.framerate << "/1 ! "
                 << "autovideosink sync=false";
    
    std::cout << "GStreamer pipeline: " << pipeline_str.str() << std::endl;
    
    GError* error = nullptr;
    pipeline_ = gst_parse_launch(pipeline_str.str().c_str(), &error);
    
    if (!pipeline_) {
        std::string error_msg = error ? error->message : "Bilinmeyen hata";
        g_error_free(error);
        throw std::runtime_error("GStreamer pipeline oluşturulamadı: " + error_msg);
    }
    
    // Appsrc'i al
    appsrc_ = gst_bin_get_by_name(GST_BIN(pipeline_), "src");
    if (!appsrc_) {
        throw std::runtime_error("Appsrc bulunamadı");
    }
    
    // Appsrc ayarları
    gst_app_src_set_stream_type(GST_APP_SRC(appsrc_), GST_APP_STREAM_TYPE_STREAM);
    g_signal_connect(appsrc_, "need-data", G_CALLBACK(on_need_data), this);
    
    std::cout << "GStreamer pipeline başarıyla oluşturuldu" << std::endl;
}

void VideoReceiver::gstreamer_loop() {
    GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "GStreamer pipeline PLAY durumuna geçirilemedi" << std::endl;
        return;
    }
    
    std::cout << "GStreamer pipeline başlatıldı" << std::endl;
    
    // GStreamer mesaj döngüsü
    GstBus* bus = gst_element_get_bus(pipeline_);
    while (is_running_.load()) {
        GstMessage* msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE,
            (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));
        
        if (msg) {
            switch (GST_MESSAGE_TYPE(msg)) {
                case GST_MESSAGE_ERROR: {
                    GError* err;
                    gchar* debug;
                    gst_message_parse_error(msg, &err, &debug);
                    std::cerr << "GStreamer hatası: " << err->message << std::endl;
                    g_clear_error(&err);
                    g_free(debug);
                    break;
                }
                case GST_MESSAGE_EOS:
                    std::cout << "GStreamer EOS" << std::endl;
                    break;
                default:
                    break;
            }
            gst_message_unref(msg);
        }
    }
    
    gst_element_set_state(pipeline_, GST_STATE_NULL);
    gst_object_unref(bus);
}

void VideoReceiver::receive_packet() {
    for (size_t i = 0; i < sockets_.size(); ++i) {
        if (!sockets_[i]->is_open()) continue;
        
        Packet packet;
        asio::ip::udp::endpoint sender_endpoint;
        
        try {
            size_t received = sockets_[i]->receive_from(
                asio::buffer(&packet, sizeof(packet)), sender_endpoint);
            
            if (received > 0) {
                packet.to_host_order();
                process_packet(packet, sender_endpoint);
            }
        } catch (const std::exception& e) {
            // Socket hatası, devam et
        }
    }
}

void VideoReceiver::process_packet(const Packet& packet, const asio::ip::udp::endpoint& sender) {
    (void)sender; // Unused parameter
    
    if (!PacketValidator::is_valid(packet)) {
        std::cerr << "Geçersiz paket alındı" << std::endl;
        return;
    }
    
    stats_.packets_received++;
    
    // Jitter buffer'a ekle
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        
        PacketInfo info;
        info.packet = packet;
        info.arrival_time = std::chrono::steady_clock::now();
        info.is_complete = true;
        
        jitter_buffer_[packet.header.sequence_number] = info;
        
        // Buffer boyutunu kontrol et
        if (jitter_buffer_.size() > static_cast<size_t>(config_.jitter_buffer_size)) {
            auto oldest = jitter_buffer_.begin();
            jitter_buffer_.erase(oldest);
        }
    }
    
    buffer_cv_.notify_one();
}

void VideoReceiver::jitter_buffer_loop() {
    while (is_running_.load()) {
        std::unique_lock<std::mutex> lock(buffer_mutex_);
        
        // Yeni paket bekle
        buffer_cv_.wait_for(lock, std::chrono::milliseconds(100));
        
        // Sıralı paketleri işle
        while (!jitter_buffer_.empty()) {
            auto it = jitter_buffer_.find(expected_sequence_);
            if (it == jitter_buffer_.end()) {
                // Paket kaybı
                stats_.packets_lost++;
                expected_sequence_++;
                continue;
            }
            
            const Packet& packet = it->second.packet;
            
            // Video paketini GStreamer'a gönder
            if (PacketValidator::is_video_packet(packet)) {
                GstBuffer* buffer = gst_buffer_new_allocate(nullptr, packet.header.payload_size, nullptr);
                GstMapInfo map;
                
                if (gst_buffer_map(buffer, &map, GST_MAP_WRITE)) {
                    std::memcpy(map.data, packet.get_payload_data(), packet.header.payload_size);
                    gst_buffer_unmap(buffer, &map);
                    
                    GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(appsrc_), buffer);
                    if (ret != GST_FLOW_OK) {
                        std::cerr << "GStreamer buffer push hatası" << std::endl;
                    }
                }
            }
            
            jitter_buffer_.erase(it);
            expected_sequence_++;
        }
    }
}

void VideoReceiver::reassemble_frame(uint32_t frame_id) {
    (void)frame_id; // Unused parameter
    // Frame reassembly implementasyonu burada yapılacak
    // Şu anda basit paket işleme kullanıyoruz
}

void VideoReceiver::on_new_sample(GstElement* sink, VideoReceiver* receiver) {
    (void)sink; // Unused parameter
    (void)receiver; // Unused parameter
    // Bu callback şu anda kullanılmıyor
}

void VideoReceiver::on_need_data(GstElement* src, guint size, VideoReceiver* receiver) {
    (void)src; // Unused parameter
    (void)size; // Unused parameter
    (void)receiver; // Unused parameter
    // Bu callback şu anda kullanılmıyor, appsrc otomatik veri alıyor
}

bool VideoReceiver::start() {
    if (is_running_.exchange(true)) {
        std::cout << "VideoReceiver zaten çalışıyor" << std::endl;
        return true;
    }
    
    std::cout << "VideoReceiver başlatılıyor..." << std::endl;
    
    // IO thread'i başlat
    io_thread_ = std::thread([this]() {
        while (is_running_.load()) {
            receive_packet();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    
    // Jitter buffer thread'i başlat
    std::thread jitter_thread([this]() {
        jitter_buffer_loop();
    });
    
    // GStreamer thread'i başlat
    gst_thread_ = std::thread([this]() {
        gstreamer_loop();
    });
    
    std::cout << "VideoReceiver başlatıldı" << std::endl;
    return true;
}

void VideoReceiver::stop() {
    if (!is_running_.exchange(false)) {
        return;
    }
    
    std::cout << "VideoReceiver durduruluyor..." << std::endl;
    
    // GStreamer'ı durdur
    if (pipeline_) {
        gst_element_send_event(pipeline_, gst_event_new_eos());
    }
    
    // Thread'leri bekle
    if (gst_thread_.joinable()) {
        gst_thread_.join();
    }
    
    if (io_thread_.joinable()) {
        io_thread_.join();
    }
    
    // GStreamer kaynaklarını temizle
    if (appsrc_) {
        gst_object_unref(appsrc_);
        appsrc_ = nullptr;
    }
    
    if (pipeline_) {
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
    }
    
    std::cout << "VideoReceiver durduruldu" << std::endl;
}

void VideoReceiver::set_resolution(int width, int height) {
    config_.width = width;
    config_.height = height;
}

void VideoReceiver::set_framerate(int fps) {
    config_.framerate = fps;
}

void VideoReceiver::set_decoder(const std::string& decoder) {
    config_.decoder = decoder;
}

void VideoReceiver::set_jitter_buffer_size(int size) {
    config_.jitter_buffer_size = size;
}

void VideoReceiver::set_max_latency(int ms) {
    config_.max_latency_ms = ms;
}

VideoReceiver::Stats VideoReceiver::get_stats() const {
    return stats_;
}

} // namespace udp_streaming
