
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
    //    - videoconvert ile RGB'ye çevir
    //    - autovideosink ile göster (tensor_filter olmadan)
    std::string pipeline_desc =
        "v4l2src device=/dev/video0 ! "
        "image/jpeg,width=1280,height=720,framerate=30/1 ! "
        "jpegdec ! "
        "videoconvert ! "
        "autovideosink sync=false";

    // 4) Pipeline'ı oluştur ve hata yakala
    GError *error = nullptr;
    GstElement *pipeline = gst_parse_launch(pipeline_desc.c_str(), &error);
    if (!pipeline) {
        std::cerr << "Pipeline oluşturulamadı: " << error->message << std::endl;
        if (error) g_error_free(error);
        return -1;
    }

    // 5) Play'e al
    GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "Pipeline PLAY durumuna geçemedi!" << std::endl;
        gst_object_unref(pipeline);
        return -1;
    }

    // 6) Hata veya EOS gelene kadar bekle
    GstBus *bus = gst_element_get_bus(pipeline);
    bool terminate = false;
    while (!terminate) {
        GstMessage *msg = gst_bus_timed_pop_filtered(
            bus, GST_CLOCK_TIME_NONE,
            (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS | GST_MESSAGE_STATE_CHANGED)
        );
        if (!msg) continue;

        switch (GST_MESSAGE_TYPE(msg)) {
            case GST_MESSAGE_ERROR: {
                GError *err;
                gchar *dbg;
                gst_message_parse_error(msg, &err, &dbg);
                std::cerr << "Hata: " << err->message
                          << " (kaynak: " << GST_OBJECT_NAME(msg->src) << ")\n";
                if (dbg) std::cerr << "Debug: " << dbg << std::endl;
                g_clear_error(&err);
                g_free(dbg);
                terminate = true;
                break;
            }
            case GST_MESSAGE_EOS:
                std::cout << "Stream sonuna ulaşıldı.\n";
                terminate = true;
                break;
            case GST_MESSAGE_STATE_CHANGED: {
                GstState old_state, new_state, pending_state;
                gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
                if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline)) {
                    std::cout << "Pipeline durumu: " << gst_element_state_get_name(old_state) 
                              << " -> " << gst_element_state_get_name(new_state) << std::endl;
                }
                break;
            }
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
