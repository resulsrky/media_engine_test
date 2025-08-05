
// main.cpp
#include <gst/gst.h>
#include <iostream>

int main(int argc, char *argv[]) {
    // 1) GStreamer'ı başlat
    gst_init(&argc, &argv);

    // 2) Kullanmak istediğin model dosyasını seç (EDSR veya FSRCNN)
    //    models klasörünün çalışma dizininde olduğundan emin ol
    std::string model_path = "models/EDSR_x2.pb";
    // Eğer FSRCNN kullanacaksan:
    // std::string model_path = "models/fsrcnn_x2.pb";

    // 3) Pipeline tanımını oluştur:
    //    - v4l2src ile kameradan MJPG 720p@30 akış
    //    - jpegdec ile decode et
    //    - videoconvert ile TensorFilter için RGB'ye çevir (inference için)
    //    - tensor_filter ile SR modeli uygula
    //    - tekrar NV12'ye dön ve vaapisink ile GPU'da render et
    std::string pipeline_desc =
        "v4l2src device=/dev/video0 ! "
        "image/jpeg,width=1280,height=720,framerate=30/1 ! "
        "jpegdec ! "
        "videoconvert ! video/x-raw,format=RGB ! "
        // TensorFlow plugin'i: tensor_filter
        "tensor_filter framework=tensorflow "
            "model=" + model_path + " "
            // Modelinin input/output katman adlarını kendi modelinle güncelle
            "input_layer_names=input "
            "output_layer_names=output ! "
        // İnference sonrası çıktıyı tekrar GPU'ya ver
        "videoconvert ! video/x-raw,format=NV12 ! "
        "vaapipostproc ! video/x-raw,format=NV12 ! "
        "vaapisink sync=false";

    // 4) Pipeline’ı oluştur ve hata yakala
    GError *error = nullptr;
    GstElement *pipeline = gst_parse_launch(pipeline_desc.c_str(), &error);
    if (!pipeline) {
        std::cerr << "Pipeline oluşturulamadı: " << error->message << std::endl;
        return -1;
    }

    // 5) Play’e al
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    // 6) Hata veya EOS gelene kadar bekle
    GstBus *bus = gst_element_get_bus(pipeline);
    bool terminate = false;
    while (!terminate) {
        GstMessage *msg = gst_bus_timed_pop_filtered(
            bus, GST_CLOCK_TIME_NONE,
            (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS)
        );
        if (!msg) continue;

        switch (GST_MESSAGE_TYPE(msg)) {
            case GST_MESSAGE_ERROR: {
                GError *err;
                gchar *dbg;
                gst_message_parse_error(msg, &err, &dbg);
                std::cerr << "Hata: " << err->message
                          << " (kaynak: " << GST_OBJECT_NAME(msg->src) << ")\n";
                g_clear_error(&err);
                g_free(dbg);
                terminate = true;
                break;
            }
            case GST_MESSAGE_EOS:
                std::cout << "Stream sonuna ulaşıldı.\n";
                terminate = true;
                break;
            default:
                break;
        }
        gst_message_unref(msg);
    }

    // 7) Temizlik
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(bus);
    gst_object_unref(pipeline);
    return 0;
}
