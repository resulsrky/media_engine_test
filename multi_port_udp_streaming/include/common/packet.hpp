#pragma once

#include <cstdint>
#include <cstring>
#include <array>
#include <chrono>
#include <arpa/inet.h> // For htonl, ntohl
#include <endian.h>    // For htobe64, be64toh

namespace udp_streaming {

// Packet boyutu sabitleri
constexpr size_t PACKET_HEADER_SIZE = 32;
constexpr size_t PACKET_PAYLOAD_SIZE = 1200;
constexpr size_t PACKET_TOTAL_SIZE = PACKET_HEADER_SIZE + PACKET_PAYLOAD_SIZE;

// Paket türleri
enum class PacketType : uint8_t {
    VIDEO_DATA = 0x01,
    AUDIO_DATA = 0x02,
    CONTROL = 0x03,
    HEARTBEAT = 0x04,
    FRAME_START = 0x05,
    FRAME_END = 0x06
};

// Paket başlığı - Network byte order (big-endian) için optimize edilmiş
#pragma pack(push, 1) // 1-byte alignment
struct PacketHeader {
    uint32_t magic;           // Magic number: 0xDEADBEEF
    uint32_t sequence_number; // Paket sıra numarası
    uint64_t timestamp;       // Mikrosaniye cinsinden timestamp
    uint8_t packet_type;      // PacketType enum değeri
    uint8_t port_id;          // Hangi porttan gönderildiği
    uint16_t payload_size;    // Payload boyutu
    uint32_t checksum;        // Header checksum
    uint32_t reserved;        // Gelecek kullanım için
    uint32_t frame_id;        // Frame ID (video için)
    uint32_t nal_unit_id;     // NAL unit ID (H.264 için)

    // Constructor
    PacketHeader() : magic(0xDEADBEEF), sequence_number(0), timestamp(0),
                    packet_type(0), port_id(0), payload_size(0), checksum(0),
                    reserved(0), frame_id(0), nal_unit_id(0) {}

    // Network byte order'a çevir
    void to_network_order() {
        magic = htonl(magic);
        sequence_number = htonl(sequence_number);
        timestamp = htobe64(timestamp);
        payload_size = htons(payload_size);
        checksum = htonl(checksum);
        reserved = htonl(reserved);
        frame_id = htonl(frame_id);
        nal_unit_id = htonl(nal_unit_id);
    }

    // Host byte order'a çevir
    void to_host_order() {
        magic = ntohl(magic);
        sequence_number = ntohl(sequence_number);
        timestamp = be64toh(timestamp);
        payload_size = ntohs(payload_size);
        checksum = ntohl(checksum);
        reserved = ntohl(reserved);
        frame_id = ntohl(frame_id);
        nal_unit_id = ntohl(nal_unit_id);
    }

    // Checksum hesapla
    uint32_t calculate_checksum() const {
        uint32_t sum = 0;
        const uint8_t* data = reinterpret_cast<const uint8_t*>(this);

        for (size_t i = 0; i < sizeof(PacketHeader) - sizeof(checksum); ++i) {
            sum += data[i];
        }

        return sum;
    }

    // Checksum doğrula
    bool verify_checksum() const {
        return checksum == calculate_checksum();
    }

    // Checksum güncelle
    void update_checksum() {
        checksum = calculate_checksum();
    }
};
#pragma pack(pop)

// Tam paket yapısı
struct Packet {
    PacketHeader header;
    std::array<uint8_t, PACKET_PAYLOAD_SIZE> payload;

    Packet() : header(), payload() {
        payload.fill(0);
    }

    // Paketi network byte order'a çevir
    void to_network_order() {
        header.to_network_order();
    }

    // Paketi host byte order'a çevir
    void to_host_order() {
        header.to_host_order();
    }

    // Payload'a veri kopyala
    template<typename T>
    void set_payload(const T& data, size_t offset = 0) {
        static_assert(sizeof(T) <= PACKET_PAYLOAD_SIZE, "Payload boyutu çok büyük!");
        std::memcpy(payload.data() + offset, &data, sizeof(T));
        header.payload_size = static_cast<uint16_t>(sizeof(T));
    }

    // Payload'dan veri oku
    template<typename T>
    T get_payload(size_t offset = 0) const {
        T result;
        std::memcpy(&result, payload.data() + offset, sizeof(T));
        return result;
    }

    // Raw payload verisi
    const uint8_t* get_payload_data() const {
        return payload.data();
    }

    uint8_t* get_payload_data() {
        return payload.data();
    }

    // Paket boyutu
    size_t get_total_size() const {
        return PACKET_HEADER_SIZE + header.payload_size;
    }
};

// Paket oluşturucu yardımcı sınıfı
class PacketBuilder {
public:
    static Packet create_video_packet(uint32_t sequence_number,
                                    uint32_t frame_id,
                                    uint32_t nal_unit_id,
                                    const uint8_t* data,
                                    size_t data_size,
                                    uint8_t port_id = 0) {
        Packet packet;

        auto now = std::chrono::high_resolution_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()).count();

        packet.header.sequence_number = sequence_number;
        packet.header.timestamp = timestamp;
        packet.header.packet_type = static_cast<uint8_t>(PacketType::VIDEO_DATA);
        packet.header.port_id = port_id;
        packet.header.frame_id = frame_id;
        packet.header.nal_unit_id = nal_unit_id;

        // Payload'ı kopyala
        size_t copy_size = std::min(data_size, PACKET_PAYLOAD_SIZE);
        std::memcpy(packet.payload.data(), data, copy_size);
        packet.header.payload_size = static_cast<uint16_t>(copy_size);

        // Checksum güncelle
        packet.header.update_checksum();

        return packet;
    }

    static Packet create_heartbeat_packet(uint32_t sequence_number, uint8_t port_id = 0) {
        Packet packet;

        auto now = std::chrono::high_resolution_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
            now.time_since_epoch()).count();

        packet.header.sequence_number = sequence_number;
        packet.header.timestamp = timestamp;
        packet.header.packet_type = static_cast<uint8_t>(PacketType::HEARTBEAT);
        packet.header.port_id = port_id;
        packet.header.payload_size = 0;

        packet.header.update_checksum();

        return packet;
    }
};

// Paket doğrulayıcı
class PacketValidator {
public:
    static bool is_valid(const Packet& packet) {
        // Magic number kontrolü
        if (packet.header.magic != 0xDEADBEEF) {
            return false;
        }

        // Checksum kontrolü
        if (!packet.header.verify_checksum()) {
            return false;
        }

        // Payload boyutu kontrolü
        if (packet.header.payload_size > PACKET_PAYLOAD_SIZE) {
            return false;
        }

        return true;
    }

    static bool is_video_packet(const Packet& packet) {
        return packet.header.packet_type == static_cast<uint8_t>(PacketType::VIDEO_DATA);
    }

    static bool is_heartbeat_packet(const Packet& packet) {
        return packet.header.packet_type == static_cast<uint8_t>(PacketType::HEARTBEAT);
    }
};

} // namespace udp_streaming
