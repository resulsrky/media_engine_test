// main.cpp - Ana uygulama
#include "nova_camera.h"
#include <iostream>
#include <csignal>
#include <termios.h>
#include <unistd.h>
#include <memory>

// Global pointer for signal handler to access the camera object
std::unique_ptr<NovaCamera> global_camera_ptr;

// Terminal kontrolü için
termios orig_termios;

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(disable_raw_mode); // Program çıkışında eski ayarların geri yüklenmesini garantile
    termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// Signal handler
void signal_handler(int signal) {
    (void)signal;
    std::cout << "\nSinyal alındı, program güvenli bir şekilde sonlandırılıyor..." << std::endl;
    if (global_camera_ptr) {
        global_camera_ptr->stop();
    }
    // disable_raw_mode atexit ile çağrılacak
    exit(0);
}

int main(int argc, char *argv[]) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Varsayılan IP ve Port ayarları
    std::string local_ip = "0.0.0.0";
    int local_port = 5001; // Genellikle alıcı portu
    std::string remote_ip = "127.0.0.1"; // !! DİKKAT: Burası arkadaşının IP'si olmalı
    int remote_port = 5001; // Genellikle arkadaşının alıcı portu

    std::cout << "========================================================" << std::endl;
    std::cout << "            Nova Camera - Görüntülü İletişim" << std::endl;
    std::cout << "========================================================" << std::endl;
    std::cout << "Kullanım: ./nova_camera [arkadaşının_IP_adresi] [kendi_portun] [arkadaşının_portu]" << std::endl;
    std::cout << "Örnek: ./nova_camera 192.168.1.100 5001 5001" << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;
    std::cout << "ÖNEMLİ: 'arkadaşının_IP_adresi' yerine, konuştuğunuz kişinin" << std::endl;
    std::cout << "        size verdiği IP adresini (örn: 192.168.1.100 veya 85.10.11.12) yazmalısınız." << std::endl;
    std::cout << "        '127.0.0.1' sadece aynı bilgisayarda test için kullanılır." << std::endl;
    std::cout << "--------------------------------------------------------" << std::endl;


    if (argc >= 2) remote_ip = argv[1];
    if (argc >= 3) local_port = std::stoi(argv[2]);
    if (argc >= 4) remote_port = std::stoi(argv[3]);

    std::cout << "Ayarlar:" << std::endl;
    std::cout << "  - Görüntü Gönderilecek IP (Arkadaşın): " << remote_ip << ":" << remote_port << std::endl;
    std::cout << "  - Görüntü Alınacak Port (Senin): " << local_ip << ":" << local_port << std::endl;
    std::cout << std::endl;

    global_camera_ptr = std::make_unique<NovaCamera>(local_ip, local_port, remote_ip, remote_port);

    if (!global_camera_ptr->initialize()) {
        std::cerr << "HATA: Kamera başlatılamadı!" << std::endl;
        return 1;
    }

    if (!global_camera_ptr->start()) {
        std::cerr << "HATA: Video yayını başlatılamadı!" << std::endl;
        return 1;
    }

    enable_raw_mode();

    std::cout << "\nProgram çalışıyor. Çıkmak için ESC tuşuna basın...\n";
    while (global_camera_ptr->is_active()) {
        char c = '\0';
        // UYARIYI GİDERMEK İÇİN YAPILAN DEĞİŞİKLİK:
        // read() fonksiyonunun sonucunu bilerek göz ardı ettiğimizi belirtiyoruz.
        (void)read(STDIN_FILENO, &c, 1);
        if (c == 27) { // ESC key
            break;
        }
    }

    global_camera_ptr->stop();
    // disable_raw_mode atexit ile otomatik çağrılacak.

    return 0;
}