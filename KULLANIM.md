# Video Chat - Karşılıklı Görüntülü İletişim Kullanım Kılavuzu

## 🎯 Program Hakkında

`video_chat` programı, iki kişi arasında karşılıklı görüntülü iletişim kurmanızı sağlar. Hem video gönderir hem de karşı tarafın videosunu alır ve gösterir.

## 🚀 Hızlı Başlangıç

### 1. Aynı Ağda İki Bilgisayar Arasında

**Bilgisayar A (IP: 192.168.1.100):**
```bash
./video_chat 0.0.0.0 5000 192.168.1.101 5001
```

**Bilgisayar B (IP: 192.168.1.101):**
```bash
./video_chat 0.0.0.0 5001 192.168.1.100 5000
```

### 2. Aynı Bilgisayarda Test (Localhost)

**Terminal 1:**
```bash
./video_chat 0.0.0.0 5000 127.0.0.1 5001
```

**Terminal 2:**
```bash
./video_chat 0.0.0.0 5001 127.0.0.1 5000
```

## 📋 Detaylı Kullanım

### Komut Satırı Parametreleri

```bash
./video_chat [local_ip] [local_port] [remote_ip] [remote_port]
```

- `local_ip`: Kendi IP adresiniz (genellikle 0.0.0.0)
- `local_port`: Dinleyeceğiniz port
- `remote_ip`: Karşı tarafın IP adresi
- `remote_port`: Karşı tarafın portu

### Örnek Senaryolar

#### Senaryo 1: İki Farklı Bilgisayar
```
Bilgisayar A: 192.168.1.100
Bilgisayar B: 192.168.1.101
```

**Bilgisayar A'da:**
```bash
./video_chat 0.0.0.0 5000 192.168.1.101 5001
```

**Bilgisayar B'de:**
```bash
./video_chat 0.0.0.0 5001 192.168.1.100 5000
```

#### Senaryo 2: Aynı Bilgisayarda Test
```bash
# Terminal 1
./video_chat 0.0.0.0 5000 127.0.0.1 5001

# Terminal 2 (yeni terminal açın)
./video_chat 0.0.0.0 5001 127.0.0.1 5000
```

#### Senaryo 3: Farklı Portlar
```bash
# Bilgisayar A
./video_chat 0.0.0.0 8000 192.168.1.101 9000

# Bilgisayar B
./video_chat 0.0.0.0 9000 192.168.1.100 8000
```

## 🔧 Kurulum ve Çalıştırma

### 1. Programı Derle
```bash
cd /home/raulcto/CLionProjects/NovaEngineV2/cmake-build-debug
make -j4
```

### 2. Programı Çalıştır
```bash
./video_chat [parametreler]
```

### 3. Çıkış
Programdan çıkmak için `Ctrl+C` tuşlarına basın.

## 📊 Program Özellikleri

### Video Kalitesi
- **Çözünürlük**: 1280x720 (720p)
- **Framerate**: 30 fps
- **Bitrate**: 2 Mbps
- **Codec**: H.264

### Network
- **Protokol**: UDP
- **Format**: RTP/H.264
- **Latency**: Düşük (zero-latency encoding)

### Performans
- **CPU Kullanımı**: Optimize edilmiş
- **Memory**: Verimli buffer yönetimi
- **Threading**: Çoklu thread desteği

## 🐛 Sorun Giderme

### Kamera Sorunları
```bash
# Kamera cihazlarını kontrol et
ls -la /dev/video*

# Kamera formatlarını kontrol et
v4l2-ctl --device=/dev/video0 --list-formats-ext
```

### Network Sorunları
```bash
# Firewall ayarlarını kontrol et
sudo ufw status

# Port dinleme durumunu kontrol et
sudo netstat -tuln | grep 5000

# Network bağlantısını test et
ping [karşı_taraf_ip]
```

### Display Sorunları
```bash
# Display ayarlarını kontrol et
echo $DISPLAY

# X11 bağlantısını test et
xeyes
```

## 📱 Mobil Cihazlarla Bağlantı

### Android/iOS için
Mobil cihazlarla bağlantı için:
1. Bilgisayarınızın IP adresini öğrenin
2. Aynı WiFi ağında olduğunuzdan emin olun
3. Mobil cihazdan bilgisayarın IP'sine bağlanın

```bash
# IP adresinizi öğrenin
ip addr show

# Örnek kullanım
./video_chat 0.0.0.0 5000 [mobil_cihaz_ip] 5001
```

## 🌐 İnternet Üzerinden Bağlantı

### Port Forwarding
İnternet üzerinden bağlantı için router'ınızda port forwarding ayarlayın:

1. Router ayarlarına girin
2. Port forwarding bölümüne gidin
3. UDP port 5000 ve 5001'i bilgisayarınıza yönlendirin

### Güvenlik
- Güvenlik duvarı ayarlarını kontrol edin
- Sadece güvendiğiniz kişilerle bağlantı kurun
- VPN kullanmayı düşünün

## 🔄 Gelişmiş Kullanım

### Otomatik Başlatma
```bash
# Script oluşturun
cat > start_video_chat.sh << 'EOF'
#!/bin/bash
cd /home/raulcto/CLionProjects/NovaEngineV2/cmake-build-debug
./video_chat 0.0.0.0 5000 $1 5001
EOF

chmod +x start_video_chat.sh

# Kullanım
./start_video_chat.sh 192.168.1.101
```

### Log Dosyası
```bash
# Log ile çalıştır
./video_chat 0.0.0.0 5000 192.168.1.101 5001 2>&1 | tee video_chat.log
```

## 📞 Destek

Sorun yaşarsanız:
1. Log mesajlarını kontrol edin
2. Network bağlantısını test edin
3. Kamera cihazını kontrol edin
4. Firewall ayarlarını kontrol edin

## 🎯 İpuçları

1. **En iyi performans için**: Aynı ağda olun
2. **Düşük latency için**: Kablolu bağlantı kullanın
3. **Kalite için**: Yeterli bandwidth olduğundan emin olun
4. **Güvenlik için**: Güvenlik duvarı ayarlarını kontrol edin

## 📈 Performans Optimizasyonu

### Sistem Ayarları
```bash
# CPU frekansını kontrol et
cat /proc/cpuinfo | grep MHz

# Memory kullanımını kontrol et
free -h

# Network bandwidth'i test et
speedtest-cli
```

### Program Ayarları
- Bitrate'i düşürün (düşük bandwidth için)
- Framerate'i düşürün (düşük CPU için)
- Çözünürlüğü düşürün (düşük memory için)

Bu kılavuz ile karşılıklı görüntülü iletişim kurabilirsiniz! 🎉 