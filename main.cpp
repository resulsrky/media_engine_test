// File: zero_copy_appsink_display.cpp
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn_superres.hpp>
#include <iostream>
#include <chrono>
#include <thread>
#include <deque> // Added for fps_history
using namespace cv;

static CascadeClassifier face_cascade;
static dnn_superres::DnnSuperResImpl sr;
static int frame_count = 0;
static auto start_time = std::chrono::high_resolution_clock::now();
static std::deque<double> fps_history;
static const int FPS_HISTORY_SIZE = 10;

// Intel HD Graphics için adaptif kalite ayarları
static struct {
    double scale_factor = 1.1;
    int min_neighbors = 3;
    Size min_size = Size(30, 30);
    bool adaptive_quality = true;
} intel_settings;

// Intel HD Graphics için adaptif kalite ayarlama
static void adjustQualityForPerformance(double current_fps) {
    if (!intel_settings.adaptive_quality) return;
    
    fps_history.push_back(current_fps);
    if (fps_history.size() > FPS_HISTORY_SIZE) {
        fps_history.pop_front();
    }
    
    if (fps_history.size() >= 5) {
        double avg_fps = 0;
        for (double fps : fps_history) {
            avg_fps += fps;
        }
        avg_fps /= fps_history.size();
        
        // Intel HD Graphics için performans bazlı kalite ayarlama
        if (avg_fps < 20) {
            // Düşük FPS - kaliteyi düşür
            intel_settings.scale_factor = std::min(1.2, intel_settings.scale_factor + 0.05);
            intel_settings.min_neighbors = std::max(2, intel_settings.min_neighbors - 1);
            intel_settings.min_size = Size(40, 40);
            std::cout << "Intel HD Graphics: Reducing quality for better performance (FPS: " << avg_fps << ")" << std::endl;
        } else if (avg_fps > 28) {
            // Yüksek FPS - kaliteyi artır
            intel_settings.scale_factor = std::max(1.05, intel_settings.scale_factor - 0.02);
            intel_settings.min_neighbors = std::min(5, intel_settings.min_neighbors + 1);
            intel_settings.min_size = Size(25, 25);
            std::cout << "Intel HD Graphics: Increasing quality (FPS: " << avg_fps << ")" << std::endl;
        }
    }
}

static GstFlowReturn on_new_sample(GstAppSink *appsink, gpointer) {
    GstSample *sample = gst_app_sink_pull_sample(appsink);
    if (!sample) return GST_FLOW_ERROR;

    // boyutları caps'ten al
    GstCaps *caps = gst_sample_get_caps(sample);
    GstStructure *st = gst_caps_get_structure(caps, 0);
    int w=0, h=0;
    gst_structure_get_int(st, "width", &w);
    gst_structure_get_int(st, "height", &h);

    // buffer'ı map et → Mat oluştur → HEMEN clone al → sonra unmap
    GstBuffer *buffer = gst_sample_get_buffer(sample);
    GstMapInfo map; gst_buffer_map(buffer, &map, GST_MAP_READ);
    Mat bgr_view(h, w, CV_8UC3, (void*)map.data);   // video/x-raw,format=BGR
    Mat frame = bgr_view.clone();                   // <-- güvenli kopya
    gst_buffer_unmap(buffer, &map);
    gst_sample_unref(sample);

    // FPS hesaplama
    frame_count++;
    auto current_time = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - start_time).count();
    if (elapsed >= 1) {
        double fps = frame_count / (double)elapsed;
        std::cout << "FPS: " << fps << std::endl;
        
        // Adaptif kalite sistemini geçici olarak kaldırdık (FPS için)
        // adjustQualityForPerformance(fps);
        
        frame_count = 0;
        start_time = current_time;
    }

    // AI: yüz tespiti (basit ve hızlı)
    std::vector<Rect> faces;
    
    // En basit parametreler (maksimum FPS için)
    face_cascade.detectMultiScale(frame, faces, 1.1, 3, 0, Size(30, 30));
    
    // Yüzleri işaretle
    for (auto &r : faces) {
        rectangle(frame, r, {0,255,0}, 2);
    }

    // Görüntüyü göster
    imshow("Intel HD Super Resolution", frame);
    waitKey(1);
    return GST_FLOW_OK;
}

int main(int argc, char** argv){
    std::cout << "Starting Intel HD Super Resolution application..." << std::endl;
    
    gst_init(&argc, &argv);
    std::cout << "GStreamer initialized" << std::endl;

    // Intel HD Graphics için thread sayısını optimize et
    int num_threads = std::thread::hardware_concurrency();
    if (num_threads > 0) {
        cv::setNumThreads(num_threads);
        std::cout << "Using " << num_threads << " threads for Intel HD Graphics optimization" << std::endl;
    }

    // modeller (build dizininden bir üstteki models/)
    std::cout << "Loading face cascade..." << std::endl;
    if(!face_cascade.load("../models/haarcascade_frontalface_default.xml")){
        std::cerr<<"Cascade yok\n"; return -1;
    }
    std::cout << "Face cascade loaded successfully" << std::endl;
    
    // Super resolution modelini geçici olarak kaldırdık (FPS için)
    // std::cout << "Loading super resolution model..." << std::endl;
    // try {
    //     sr.readModel("../models/EDSR_x2.pb");
    //     sr.setModel("edsr", 2);
    //     std::cout << "Super resolution model loaded successfully" << std::endl;
    // } catch (const cv::Exception& e) {
    //     std::cerr << "Error loading super resolution model: " << e.what() << std::endl;
    //     return -1;
    // }

    // Terminaldeki çalışan pipeline'ın aynısı (appsink ile)
    std::cout << "Creating optimized GStreamer pipeline..." << std::endl;
    const char* desc =
        "v4l2src device=/dev/video0 ! "
        "videoconvert ! "
        "video/x-raw,format=BGR ! "
        "appsink name=sink emit-signals=true max-buffers=1 drop=true sync=false";

    GError *err=nullptr;
    GstElement *pipe = gst_parse_launch(desc, &err);
    if(!pipe){ 
        std::cerr<<"Pipeline yok: "<<err->message<<"\n"; 
        if(err) g_error_free(err);
        return -1; 
    }
    std::cout << "Pipeline created successfully" << std::endl;

    GstAppSink *sink = GST_APP_SINK(gst_bin_get_by_name(GST_BIN(pipe),"sink"));
    g_signal_connect(sink, "new-sample", G_CALLBACK(on_new_sample), nullptr);

    std::cout << "Setting pipeline to PLAYING state..." << std::endl;
    GstStateChangeReturn ret = gst_element_set_state(pipe, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "Failed to set pipeline to PLAYING state" << std::endl;
        return -1;
    }
    std::cout << "Pipeline is now PLAYING" << std::endl;

    // Bus döngüsü
    GstBus *bus = gst_element_get_bus(pipe);
    std::cout << "Starting main loop..." << std::endl;
    std::cout << "Press Ctrl+C to exit..." << std::endl;
    
    // Pencere açılana kadar bekle
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    
    while (true) {
        // Pencere görünürlüğünü kontrol et
        if (getWindowProperty("Intel HD Super Resolution", WND_PROP_VISIBLE) < 1) {
            std::cout << "Window closed, exiting..." << std::endl;
            break;
        }
        
        GstMessage *m = gst_bus_timed_pop_filtered(
            bus, 100000000, (GstMessageType)(GST_MESSAGE_ERROR|GST_MESSAGE_EOS|GST_MESSAGE_WARNING));
        if (m) { 
            std::cout << "Pipeline message: " << GST_MESSAGE_TYPE_NAME(m) << std::endl;
            
            if (GST_MESSAGE_TYPE(m) == GST_MESSAGE_ERROR) {
                GError *err;
                gchar *debug;
                gst_message_parse_error(m, &err, &debug);
                std::cout << "Error: " << err->message << std::endl;
                std::cout << "Debug: " << debug << std::endl;
                g_error_free(err);
                g_free(debug);
            }
            
            gst_message_unref(m); 
            break; 
        }
        
        // Kısa bir bekleme
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    std::cout << "Cleaning up..." << std::endl;
    gst_element_set_state(pipe, GST_STATE_NULL);
    gst_object_unref(bus);
    gst_object_unref(pipe);
    destroyAllWindows();
    std::cout << "Application finished" << std::endl;
    return 0;
}

