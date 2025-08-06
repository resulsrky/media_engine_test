// main.cpp - Ana uygulama
#include "nova_camera.h"
#include <iostream>
#include <signal.h>
#include <termios.h>
#include <unistd.h>

// Terminal kontrolü için
typedef struct termios termios_t;
termios_t orig_termios;

void disable_raw_mode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enable_raw_mode() {
    tcgetattr(STDIN_FILENO, &orig_termios);
    termios_t raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

// Signal handler
void signal_handler(int signal) {
    (void)signal;
    std::cout << "\nProgram sonlandırılıyor..." << std::endl;
    disable_raw_mode();
    exit(0);
}

int main(int argc, char *argv[]) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    // Command line arguments
    std::string local_ip = "0.0.0.0";
    int local_port = 5000;
    std::string remote_ip = "127.0.0.1";
    int remote_port = 5001;
    
    if (argc >= 5) {
        local_ip = argv[1];
        local_port = std::stoi(argv[2]);
        remote_ip = argv[3];
        remote_port = std::stoi(argv[4]);
    }
    
    std::cout << "Kullanım: ./nova_camera [local_ip] [local_port] [remote_ip] [remote_port]" << std::endl;
    std::cout << "Örnek: ./nova_camera 0.0.0.0 5000 192.168.1.100 5001" << std::endl;
    std::cout << "Varsayılan: Local: " << local_ip << ":" << local_port 
              << ", Remote: " << remote_ip << ":" << remote_port << std::endl;
    
    NovaCamera camera(local_ip, local_port, remote_ip, remote_port);
    
    if (!camera.initialize()) {
        std::cerr << "Kamera başlatılamadı!" << std::endl;
        return 1;
    }
    
    if (!camera.start()) {
        std::cerr << "Video streaming başlatılamadı!" << std::endl;
        return 1;
    }
    
    // ESC key detection
    enable_raw_mode();
    
    while (camera.is_active()) {
        fd_set readfds;
        struct timeval timeout = {0, 100000}; // 100ms
        
        FD_ZERO(&readfds);
        FD_SET(STDIN_FILENO, &readfds);
        
        int result = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);
        
        if (result > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {
            char c;
            if (read(STDIN_FILENO, &c, 1) == 1) {
                if (c == 27) { // ESC key
                    std::cout << "\nESC tuşuna basıldı, program kapatılıyor..." << std::endl;
                    break;
                }
            }
        }
    }
    
    camera.stop();
    disable_raw_mode();
    return 0;
} 