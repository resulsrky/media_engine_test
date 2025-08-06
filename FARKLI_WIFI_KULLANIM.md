# Farklı WiFi Ağlarında Video Chat - Yüksek Kalite Kılavuzu

## 🌐 Farklı WiFi Ağlarında Çalışma

### 🎯 Özellikler
- **720p @ 60fps** yüksek kalite
- **4 Mbps bitrate** ile optimize edilmiş
- **UDP/RTP protokolü** düşük latency
- **Heartbeat paketleri** bağlantı kontrolü
- **NAT Traversal** desteği

## 🚀 Kurulum ve Çalıştırma

### 1. Programı Derle
```bash
cd /home/raulcto/CLionProjects/NovaEngineV2/cmake-build-debug
make -j4
```

### 2. Gelişmiş Video Chat Programı
```bash
./video_chat_advanced [local_ip] [local_port] [remote_ip] [remote_port]
```

## 📡 Farklı WiFi Ağlarında Bağlantı

### Senaryo 1: İki Farklı Ev/Ofis

**Ev A (IP: 203.0.113.1):**
```bash
# Router'da port forwarding ayarla (UDP 5000)
./video_chat_advanced 0.0.0.0 5000 203.0.113.2 5001
```

**Ev B (IP: 203.0.113.2):**
```bash
# Router'da port forwarding ayarla (UDP 5001)
./video_chat_advanced 0.0.0.0 5001 203.0.113.1 5000
```

### Senaryo 2: Mobil Hotspot ile Bağlantı

**Bilgisayar A (Mobil Hotspot):**
```bash
# Mobil cihazın IP'sini öğren
ip addr show

# Örnek: 192.168.43.100
./video_chat_advanced 0.0.0.0 5000 192.168.43.101 5001
```

**Bilgisayar B (Mobil Hotspot):**
```bash
# Örnek: 192.168.43.101
./video_chat_advanced 0.0.0.0 5001 192.168.43.100 5000
```

### Senaryo 3: İnternet Üzerinden Bağlantı

**Ev A (Public IP: 203.0.113.1):**
```bash
# Router'da port forwarding: UDP 5000 -> 192.168.1.100:5000
./video_chat_advanced 0.0.0.0 5000 203.0.113.2 5001
```

**Ev B (Public IP: 203.0.113.2):**
```bash
# Router'da port forwarding: UDP 5001 -> 192.168.1.101:5001
./video_chat_advanced 0.0.0.0 5001 203.0.113.1 5000
```

## 🔧 Router Ayarları

### Port Forwarding Kurulumu

#### 1. Router'a Giriş
```
Tarayıcıda: http://192.168.1.1 (veya router IP'si)
Kullanıcı adı: admin
Şifre: (router şifreniz)
```

#### 2. Port Forwarding Ayarları
```
Servis Adı: VideoChat_Send
Protokol: UDP
Dış Port: 5000
İç IP: 192.168.1.100 (bilgisayarınızın IP'si)
İç Port: 5000
```

```
Servis Adı: VideoChat_Receive
Protokol: UDP
Dış Port: 5001
İç IP: 192.168.1.100
İç Port: 5001
```

#### 3. Firewall Ayarları
```
UDP Port 5000: Açık
UDP Port 5001: Açık
```

## 📊 Yüksek Kalite Ayarları

### Video Kalitesi
- **Çözünürlük**: 1280x720 (720p)
- **Framerate**: 60 fps (yüksek akıcılık)
- **Bitrate**: 4 Mbps (yüksek kalite)
- **Codec**: H.264 (verimli sıkıştırma)

### Network Optimizasyonu
- **Protokol**: UDP (düşük latency)
- **Format**: RTP/H.264
- **Buffer**: Optimize edilmiş
- **Heartbeat**: 5 saniyede bir

### Performans Ayarları
```bash
# Yüksek kalite için
./video_chat_advanced 0.0.0.0 5000 203.0.113.2 5001

# Düşük bandwidth için
./video_chat_advanced 0.0.0.0 5000 203.0.113.2 5001 --quality low

# Yüksek bandwidth için
./video_chat_advanced 0.0.0.0 5000 203.0.113.2 5001 --quality high
```

## 🌍 İnternet Üzerinden Bağlantı

### 1. Public IP Öğrenme
```bash
# Public IP'nizi öğrenin
curl ifconfig.me
# veya
wget -qO- ifconfig.co
```

### 2. Port Forwarding Test
```bash
# Port açık mı test edin
telnet [public_ip] 5000
telnet [public_ip] 5001
```

### 3. Bağlantı Testi
```bash
# UDP paket testi
nc -u [remote_ip] 5000
```

## 📱 Mobil Cihazlarla Bağlantı

### Android/iOS için
1. **Mobil cihazın IP'sini öğrenin**
2. **Aynı ağda olduğunuzdan emin olun**
3. **Port forwarding gerekmez (aynı ağ)**

```bash
# Mobil cihaz IP'si: 192.168.1.50
./video_chat_advanced 0.0.0.0 5000 192.168.1.50 5001
```

## 🔍 Sorun Giderme

### Network Sorunları

#### 1. Port Açık mı Kontrol Edin
```bash
# Port dinleme durumu
sudo netstat -tuln | grep 5000
sudo netstat -tuln | grep 5001

# UDP paket testi
sudo tcpdump -i any udp port 5000
```

#### 2. Firewall Ayarları
```bash
# UFW firewall
sudo ufw allow 5000/udp
sudo ufw allow 5001/udp

# iptables
sudo iptables -A INPUT -p udp --dport 5000 -j ACCEPT
sudo iptables -A INPUT -p udp --dport 5001 -j ACCEPT
```

#### 3. Router Ayarları
- Port forwarding doğru ayarlandı mı?
- Firewall UDP paketlerini engelliyor mu?
- NAT ayarları doğru mu?

### Kalite Sorunları

#### 1. Bandwidth Testi
```bash
# İnternet hızınızı test edin
speedtest-cli

# Yerel ağ hızını test edin
iperf3 -c [remote_ip]
```

#### 2. Kalite Ayarları
```bash
# Düşük bandwidth için
chat.set_quality(640, 480, 30, 1000);  # 480p @ 30fps, 1 Mbps

# Yüksek bandwidth için
chat.set_quality(1920, 1080, 60, 8000);  # 1080p @ 60fps, 8 Mbps
```

## 🚀 Gelişmiş Kullanım

### 1. Otomatik Başlatma Scripti
```bash
#!/bin/bash
# start_video_chat.sh

REMOTE_IP=$1
REMOTE_PORT=$2

if [ -z "$REMOTE_IP" ]; then
    echo "Kullanım: $0 <remote_ip> [remote_port]"
    exit 1
fi

if [ -z "$REMOTE_PORT" ]; then
    REMOTE_PORT=5001
fi

cd /home/raulcto/CLionProjects/NovaEngineV2/cmake-build-debug
./video_chat_advanced 0.0.0.0 5000 $REMOTE_IP $REMOTE_PORT
```

### 2. Kalite Monitörü
```bash
#!/bin/bash
# monitor_quality.sh

while true; do
    echo "=== Video Chat Kalite Monitörü ==="
    echo "CPU Kullanımı: $(top -bn1 | grep "Cpu(s)" | awk '{print $2}')"
    echo "Memory Kullanımı: $(free -h | grep Mem | awk '{print $3"/"$2}')"
    echo "Network: $(cat /proc/net/dev | grep eth0 | awk '{print $2" bytes in, "$10" bytes out"}')"
    echo "UDP Paketleri: $(netstat -i | grep eth0 | awk '{print $4" packets"}')"
    sleep 5
done
```

### 3. Log Dosyası ile Çalıştırma
```bash
./video_chat_advanced 0.0.0.0 5000 203.0.113.2 5001 2>&1 | tee video_chat_$(date +%Y%m%d_%H%M%S).log
```

## 📈 Performans Optimizasyonu

### Sistem Ayarları
```bash
# CPU frekansını maksimuma çıkar
sudo cpupower frequency-set -g performance

# Network buffer'ları artır
echo 'net.core.rmem_max = 16777216' | sudo tee -a /etc/sysctl.conf
echo 'net.core.wmem_max = 16777216' | sudo tee -a /etc/sysctl.conf
sudo sysctl -p

# Real-time priority
sudo chrt -p -r 99 $$
```

### Program Ayarları
- **Threading**: Çoklu thread desteği
- **Buffer Yönetimi**: Zero-copy optimizasyonu
- **Hardware Acceleration**: GPU encoding (varsa)
- **Network Optimization**: UDP paket optimizasyonu

## 🎯 İpuçları

1. **En iyi performans için**: Kablolu bağlantı kullanın
2. **Düşük latency için**: Aynı ISP'yi kullanın
3. **Yüksek kalite için**: Yeterli bandwidth olduğundan emin olun
4. **Güvenlik için**: VPN kullanmayı düşünün
5. **Stabilite için**: Heartbeat paketlerini kontrol edin

## 🔒 Güvenlik

### Güvenlik Önlemleri
- Sadece güvendiğiniz kişilerle bağlantı kurun
- Güvenlik duvarı ayarlarını kontrol edin
- VPN kullanmayı düşünün
- Port forwarding'i sadece gerekli portlara uygulayın

### Güvenlik Testi
```bash
# Port taraması
nmap -p 5000,5001 [remote_ip]

# UDP flood testi
hping3 -2 -p 5000 -i u100 [remote_ip]
```

Bu kılavuz ile farklı WiFi ağlarında yüksek kaliteli video chat yapabilirsiniz! 🎉 