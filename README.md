# NovaEngine V2 - UDP Video Streaming Engine

Yüksek performanslı UDP tabanlı video streaming motoru. Discord/Zoom benzeri serverless video konferans uygulamaları için tasarlanmıştır.

## 🎯 Hedefler

- **720p @ 30 fps** video streaming
- **2 Mbps bitrate** ile optimize edilmiş kalite
- **UDP protokolü** ile düşük latency
- **Serverless mimari** desteği
- **Yüksek performans** optimizasyonları

## 🚀 Özellikler

### Video Streaming
- Kamera yakalama (v4l2src)
- H.264 encoding (x264enc)
- RTP paketleme (rtph264pay)
- UDP streaming (udpsink)

### Performans Optimizasyonları
- SIMD optimizasyonları (march=native)
- OpenMP paralel işleme
- Threading desteği
- Zero-latency encoding

### Kalite Ayarları
- 1280x720 çözünürlük
- 30 fps framerate
- 2 Mbps bitrate
- Ultrafast encoding preset

## 📋 Gereksinimler

### Sistem Gereksinimleri
- Linux (Ubuntu 22.04+ önerilir)
- Kamera cihazı (/dev/video0)
- GStreamer 1.20+
- OpenCV 4.5+

### Paket Kurulumu
```bash
sudo apt update
sudo apt install -y \
    libgstreamer1.0-dev \
    libgstreamer-plugins-base1.0-dev \
    libgstreamer-plugins-bad1.0-dev \
    gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-ugly \
    gstreamer1.0-libav \
    gstreamer1.0-tools \
    libopencv-dev \
    v4l-utils
```

## 🔧 Derleme

```bash
# Projeyi klonla
git clone <repository-url>
cd NovaEngineV2

# Build dizini oluştur
mkdir build && cd build

# CMake ile yapılandır
cmake ..

# Derle
make -j4
```

## 🎮 Kullanım

### Video Streaming (Gönderici)
```bash
# Varsayılan hedef (127.0.0.1:5000)
./nova_engine_v2

# Özel hedef belirtme
./nova_engine_v2 <target_ip> <port>
```

### Video Alıcısı (Test)
```bash
# UDP stream'i al ve göster
./test_receiver
```

### Örnek Kullanım
```bash
# Terminal 1: Video gönder
./nova_engine_v2 192.168.1.100 5000

# Terminal 2: Video al
./test_receiver
```

## 📊 Performans

### Hedeflenen Değerler
- **Çözünürlük**: 1280x720 (720p)
- **Framerate**: 30 fps
- **Bitrate**: 2 Mbps
- **Latency**: < 100ms
- **CPU Kullanımı**: < 30%

### Optimizasyonlar
- SIMD vektör işlemleri
- OpenMP paralel işleme
- Zero-copy buffer yönetimi
- Hardware acceleration (varsa)

## 🏗️ Mimari

```
Kamera (v4l2src)
    ↓
JPEG Decode (jpegdec)
    ↓
Video Convert (videoconvert)
    ↓
H.264 Encode (x264enc)
    ↓
RTP Payload (rtph264pay)
    ↓
UDP Send (udpsink)
```

## 🔍 Test

### Kamera Testi
```bash
# Kamera cihazlarını listele
ls -la /dev/video*

# Kamera formatlarını kontrol et
v4l2-ctl --device=/dev/video0 --list-formats-ext
```

### Network Testi
```bash
# UDP port dinleme
sudo netstat -tuln | grep 5000

# Network trafiği izleme
sudo tcpdump -i lo udp port 5000
```

## 🐛 Sorun Giderme

### Kamera Sorunları
- Kamera cihazının mevcut olduğunu kontrol edin
- Format uyumluluğunu kontrol edin
- İzinleri kontrol edin (`sudo usermod -a -G video $USER`)

### Network Sorunları
- Firewall ayarlarını kontrol edin
- Port erişimini test edin
- UDP paketlerinin ulaştığını doğrulayın

### Performans Sorunları
- CPU kullanımını izleyin
- Memory kullanımını kontrol edin
- Network bandwidth'i test edin

## 📈 Gelecek Geliştirmeler

- [ ] WebRTC desteği
- [ ] Multi-client streaming
- [ ] Adaptive bitrate
- [ ] Hardware acceleration
- [ ] Web interface
- [ ] Recording özelliği
- [ ] Audio streaming
- [ ] Screen sharing

## 🤝 Katkıda Bulunma

1. Fork yapın
2. Feature branch oluşturun (`git checkout -b feature/amazing-feature`)
3. Commit yapın (`git commit -m 'Add amazing feature'`)
4. Push yapın (`git push origin feature/amazing-feature`)
5. Pull Request oluşturun

## 📄 Lisans

Bu proje MIT lisansı altında lisanslanmıştır.

## 👨‍💻 Geliştirici

NovaEngine V2 - UDP Video Streaming Engine
Hedef: Serverless video conferencing için yüksek performanslı streaming motoru
# media_engine_test
