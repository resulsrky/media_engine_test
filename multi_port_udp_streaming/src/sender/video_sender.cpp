#include "video_sender.hpp"
#include <iostream>
#include <sstream>
#include <algorithm>

namespace udp_streaming {

VideoSender::VideoSender(const std::string& remote_ip, const std::vector<uint16_t>& ports)
    : pipeline_(nullptr), appsrc_(nullptr), encoder_(nullptr), appsink_(nullptr)
    , is_running_(false), sequence_number_(0) {
    
    config_.remote_ip = remote_ip;
    config_.ports = ports;
    
    std::cout << "VideoSender oluşturuldu:" << std::endl;
    std::cout << "  Hedef IP: " << config_.remote_ip << std::endl;
    std::cout << "  Portlar: ";
    for (size_t i = 0; i < config_.ports.size(); ++i) {
        std::cout << config_.ports[i];
        if (i < config_.ports.size() - 1) std::cout << ", ";
    }
    std::cout << std::endl;
}

VideoSender::~VideoSender() {
    stop();
}

bool VideoSender::initialize() {
    try {
        setup_sockets();
        setup_gstreamer();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "VideoSender initialize hatası: " << e.what() << std::endl;
        return false;
    }
}

void VideoSender::setup_sockets() {
    for (uint16_t port : config_.ports) {
        auto socket = std::make_unique<asio::ip::udp::socket>(io_context_);
        socket->open(asio::ip::udp::v4());
        
        asio::ip::udp::endpoint endpoint(
            asio::ip::make_address(config_.remote_ip), port);
        
        sockets_.push_back(std::move(socket));
        endpoints_.push_back(endpoint);
        
        std::cout << "Socket oluşturuldu: " << config_.remote_ip << ":" << port << std::endl;
    }
}

void VideoSender::setup_gstreamer() {
    gst_init(nullptr, nullptr);
    
    // GStreamer pipeline oluştur
    std::stringstream pipeline_str;
    pipeline_str << "v4l2src device=/dev/video0 ! "
                 << "video/x-raw,width=" << config_.width 
                 << ",height=" << config_.height 
                 << ",framerate=" << config_.framerate << "/1 ! "
                 << "videoconvert ! "
                 << config_.encoder << " tune=zerolatency speed-preset=ultrafast "
                 << "bitrate=" << (config_.bitrate / 1000) << " ! "
                 << "video/x-h264,profile=high ! "
                 << "appsink name=sink sync=false";
    
    std::cout << "GStreamer pipeline: " << pipeline_str.str() << std::endl;
    
    GError* error = nullptr;
    pipeline_ = gst_parse_launch(pipeline_str.str().c_str(), &error);
    
    if (!pipeline_) {
        std::string error_msg = error ? error->message : "Bilinmeyen hata";
        g_error_free(error);
        throw std::runtime_error("GStreamer pipeline oluşturulamadı: " + error_msg);
    }
    
    // Appsink'i al
    appsink_ = gst_bin_get_by_name(GST_BIN(pipeline_), "sink");
    if (!appsink_) {
        throw std::runtime_error("Appsink bulunamadı");
    }
    
    // Callback'leri ayarla
    g_signal_connect(appsink_, "new-sample", G_CALLBACK(on_new_sample), this);
    
    std::cout << "GStreamer pipeline başarıyla oluşturuldu" << std::endl;
}

void VideoSender::gstreamer_loop() {
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

void VideoSender::send_packet(const Packet& packet) {
    // Round-robin ile paketleri farklı portlara dağıt
    static size_t current_port = 0;
    size_t port_index = current_port % sockets_.size();
    current_port++;
    
    try {
        Packet network_packet = packet;
        network_packet.to_network_order();
        
        size_t sent = sockets_[port_index]->send_to(
            asio::buffer(&network_packet, network_packet.get_total_size()),
            endpoints_[port_index]);
        
        if (sent != network_packet.get_total_size()) {
            std::cerr << "Paket tam gönderilemedi: " << sent << "/" 
                     << network_packet.get_total_size() << " byte" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Paket gönderme hatası: " << e.what() << std::endl;
    }
}

void VideoSender::packetize_video_data(const uint8_t* data, size_t size, uint32_t frame_id) {
    const size_t max_payload_size = PACKET_PAYLOAD_SIZE;
    size_t offset = 0;
    uint32_t nal_unit_id = 0;
    
    while (offset < size) {
        size_t chunk_size = std::min(max_payload_size, size - offset);
        
        auto packet = PacketBuilder::create_video_packet(
            sequence_number_,
            frame_id,
            nal_unit_id,
            data + offset,
            chunk_size,
            static_cast<uint8_t>(sequence_number_ % config_.ports.size())
        );
        
        send_packet(packet);
        
        offset += chunk_size;
        sequence_number_++;
        nal_unit_id++;
    }
}

void VideoSender::on_new_sample(GstElement* sink, VideoSender* sender) {
    GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
    if (!sample) return;
    
    GstBuffer* buffer = gst_sample_get_buffer(sample);
    GstMapInfo map;
    
    if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        static uint32_t frame_id = 0;
        sender->packetize_video_data(map.data, map.size, frame_id++);
        gst_buffer_unmap(buffer, &map);
    }
    
    gst_sample_unref(sample);
}

void VideoSender::on_need_data(GstElement* src, guint size, VideoSender* sender) {
    (void)src;
    (void)size;
    (void)sender;
    // Bu callback şu anda kullanılmıyor, v4l2src otomatik veri sağlıyor
}

bool VideoSender::start() {
    if (is_running_.exchange(true)) {
        std::cout << "VideoSender zaten çalışıyor" << std::endl;
        return true;
    }
    
    std::cout << "VideoSender başlatılıyor..." << std::endl;
    
    // IO thread'i başlat
    io_thread_ = std::thread([this]() {
        io_context_.run();
    });
    
    // GStreamer thread'i başlat
    gst_thread_ = std::thread([this]() {
        gstreamer_loop();
    });
    
    std::cout << "VideoSender başlatıldı" << std::endl;
    return true;
}

void VideoSender::stop() {
    if (!is_running_.exchange(false)) {
        return;
    }
    
    std::cout << "VideoSender durduruluyor..." << std::endl;
    
    // GStreamer'ı durdur
    if (pipeline_) {
        gst_element_send_event(pipeline_, gst_event_new_eos());
    }
    
    // Thread'leri bekle
    if (gst_thread_.joinable()) {
        gst_thread_.join();
    }
    
    if (io_thread_.joinable()) {
        io_context_.stop();
        io_thread_.join();
    }
    
    // GStreamer kaynaklarını temizle
    if (appsink_) {
        gst_object_unref(appsink_);
        appsink_ = nullptr;
    }
    
    if (pipeline_) {
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
    }
    
    std::cout << "VideoSender durduruldu" << std::endl;
}

void VideoSender::set_resolution(int width, int height) {
    config_.width = width;
    config_.height = height;
}

void VideoSender::set_framerate(int fps) {
    config_.framerate = fps;
}

void VideoSender::set_bitrate(int bitrate) {
    config_.bitrate = bitrate;
}

void VideoSender::set_encoder(const std::string& encoder) {
    config_.encoder = encoder;
}

} // namespace udp_streaming
