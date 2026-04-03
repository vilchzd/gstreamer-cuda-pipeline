#include <cuda_runtime.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>

#include "blur-kernel.h"

using namespace std;
using namespace chrono;

GMainLoop *global_loop = nullptr;
GstAppSrc *appsrc_display = nullptr;
static unsigned char *d_input = nullptr;
static unsigned char *d_output = nullptr;
atomic<bool> filter_enabled(false);
atomic<bool> running(true);

static int frame_count = 0;
static steady_clock::time_point last_time = steady_clock::now();

//--------------------------- Bus Callback ----------------------------------//
static void on_message(GstBus* bus, GstMessage* message, gpointer user_data) {
    GMainLoop* loop = (GMainLoop*)user_data;

    switch (GST_MESSAGE_TYPE(message)) {

        case GST_MESSAGE_ERROR: {
            GError* err;
            gchar* debug;

            gst_message_parse_error(message, &err, &debug);
            g_printerr("ERROR: %s\n", err->message);

            g_error_free(err);
            g_free(debug);

            g_main_loop_quit(loop);
            break;
        }

        case GST_MESSAGE_EOS:
            g_print("End of stream\n");
            g_main_loop_quit(loop);
            break;

        default:
            break;
    }
}

//--------------------------- Appsink Callback ------------------------------//
static GstFlowReturn new_sample(GstAppSink* appsink, gpointer user_data) {

    GstSample *sample = gst_app_sink_pull_sample(appsink);
    if (!sample) return GST_FLOW_ERROR;

    GstBuffer *buffer = gst_sample_get_buffer(sample);
    GstCaps *caps = gst_sample_get_caps(sample);
    GstStructure *structure = gst_caps_get_structure(caps, 0);

    int width, height;
    gst_structure_get_int(structure, "width", &width);
    gst_structure_get_int(structure, "height", &height);   

    static bool caps_set = false;
    if (!caps_set) {
        GstCaps *newcaps = gst_caps_copy(caps);
        gst_app_src_set_caps(appsrc_display, newcaps);
        gst_caps_unref(newcaps);
        caps_set = true;
    }
    
    GstMapInfo map;
    if (!gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        g_printerr("Failed to map input buffer\n");
        gst_sample_unref(sample);
        return GST_FLOW_ERROR;
    }

    GstBuffer *out_buffer = nullptr;

    if (filter_enabled) { 

        static size_t buffer_size = 0;
        if (d_input == nullptr) {
            buffer_size = map.size;
            //Allocating buffer in GPU memory"
            cudaMalloc(&d_input, buffer_size);
            cudaMalloc(&d_output, buffer_size);
        }

        out_buffer = gst_buffer_new_allocate(NULL, map.size, NULL);

        GstMapInfo out_map;
        if (!gst_buffer_map(out_buffer, &out_map, GST_MAP_WRITE)) {
            g_printerr("Failed to map output buffer\n");
            gst_buffer_unmap(buffer, &map);
            gst_sample_unref(sample);
            return GST_FLOW_ERROR;
        }

        gpu_wrapper_blurBGR(map.data, out_map.data, d_input, d_output, width, height, buffer_size, GRID_SIZE);  //<<--------------------- CUDA kernel 

        gst_buffer_unmap(buffer, &map);
        gst_buffer_unmap(out_buffer, &out_map);

    } else {
        out_buffer = gst_buffer_ref(buffer);;
    }

    gst_buffer_unmap(buffer, &map);
    
    GST_BUFFER_PTS(out_buffer) = GST_BUFFER_PTS(buffer);                     
    GST_BUFFER_DURATION(out_buffer) = GST_BUFFER_DURATION(buffer);

    GstFlowReturn ret = gst_app_src_push_buffer(appsrc_display, out_buffer);

    gst_sample_unref(sample);

    frame_count++;
    auto now = steady_clock::now();
    auto elapsed = duration_cast<seconds>(now - last_time).count();
    if (elapsed >= 1) {
        cout << "\033[s";        
        cout << "\033[4;1H";  
        cout << "FPS: " << frame_count / elapsed << "     ";   
        cout << "\033[u";           
        cout << flush;
        frame_count = 0;
        last_time = now;
    }

    if (ret != GST_FLOW_OK) {
        g_printerr("Push buffer failed: %d\n", ret);
    }

    return GST_FLOW_OK;
}

//...............................Key Input...................................//
void keyboard_inputs() {
    while (running) {

        cout << "\033[5;1H";
        cout << "Blur: " << (filter_enabled ? "ON" : "OFF") << "     ";
        cout << "\033[6;1H";
        cout << "\033[K";
        cout << "Command (t=toggle | q=quit): " << flush;

        char input;
        cin >> input;   

        if (input == 't') {
            filter_enabled = !filter_enabled;
            cout << "\033[5;1H";
            cout << "Blur: " << (filter_enabled ? "ON" : "OFF") << "     ";

            cout << "\033[6;1H";
            cout << "\033[K";
            cout << "Command (t=toggle | q=quit): " << flush;
        }
        else if (input == 'q') {
            cout << ">>>Stopping pipeline" << endl;
            running = false;

            if (global_loop) {
                g_main_loop_quit(global_loop);
            }
            break;
        }
    }
}

//---------------------------- Main Function --------------------------------//
int main(int argc, char *argv[]) {

    gst_init(&argc, &argv);

    //------------------------ Capture pipeline -----------------------------//
    GstElement *pipeline_capture = gst_pipeline_new("capture-pipeline");
    GstElement *source = gst_element_factory_make("mfvideosrc", "source"); 
    GstElement *convert = gst_element_factory_make("videoconvert", "convert");
    GstElement *appsink = gst_element_factory_make("appsink", "appsink");

    if (!pipeline_capture || !source || !convert || !appsink) {
        g_printerr("Failed to create capture elements.\n");
        return -1;
    }

    GstCaps *caps = gst_caps_from_string("video/x-raw,format=BGR");
    g_object_set(appsink, "emit-signals", TRUE, "sync", FALSE, "max-buffers", 1, "drop", TRUE, "caps", caps, NULL);
    gst_caps_unref(caps);
    g_signal_connect(appsink, "new-sample", G_CALLBACK(new_sample), nullptr);

    gst_bin_add_many(GST_BIN(pipeline_capture), source, convert, appsink, NULL);
    if (!gst_element_link_many(source, convert, appsink, NULL)) {
        g_printerr("Failed to link capture pipeline.\n");
        return -1;
    }


    //----------------------- Display pipeline ------------------------------//
    GstElement *pipeline_display = gst_pipeline_new("display-pipeline");
    GstElement *queue = gst_element_factory_make("queue", "queue_display");
    GstElement *convert_display = gst_element_factory_make("videoconvert", "convert_display");
    GstElement *videosink = gst_element_factory_make("autovideosink", "videosink");
    appsrc_display = GST_APP_SRC(gst_element_factory_make("appsrc", "appsrc"));

    if (!pipeline_display || !appsrc_display || !queue || !convert_display || !videosink) {
        g_printerr("Failed to create display elements.\n");
        return -1;
    }

    g_object_set(appsrc_display, "format", GST_FORMAT_TIME, "is-live", TRUE, "block", FALSE, NULL);

    g_object_set(queue, "max-size-buffers", 5, "leaky", 0, NULL);

    gst_bin_add_many(GST_BIN(pipeline_display), GST_ELEMENT(appsrc_display), queue, convert_display, videosink, NULL);
    if (!gst_element_link_many(GST_ELEMENT(appsrc_display), queue, convert_display, videosink, NULL)) {
        g_printerr("Failed to link display pipeline.\n");
        return -1;
    }

    //---------------------------- Bus ---------------------------------------//
    GstBus *bus = gst_element_get_bus(pipeline_capture);
    gst_bus_add_signal_watch(bus);

    GMainLoop *loop = g_main_loop_new(nullptr, FALSE);
    global_loop = loop;

    g_signal_connect(bus, "message", G_CALLBACK(on_message), loop);

    //------------------------ Initialize pipelines -------------------------//
    // gst_element_set_state(pipeline_capture, GST_STATE_PLAYING);
    // gst_element_set_state(pipeline_display, GST_STATE_PLAYING);

    if (gst_element_set_state(pipeline_capture, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE || 
        gst_element_set_state(pipeline_display, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Failed to start pipelines.\n");
        return -1;
    }

    cout << ">>>Starting Pipeline" << endl;
    cout << "Streaming webcam footage\n" << endl;

    std::thread key_thread(keyboard_inputs);
    //GMainLoop* loop = g_main_loop_new(nullptr, FALSE);                                   // Main loop
    g_main_loop_run(loop);

    running = false;
    key_thread.join();

    //------------------------ Cleanup -------------------------------------//
    gst_element_set_state(pipeline_capture, GST_STATE_NULL);
    gst_element_set_state(pipeline_display, GST_STATE_NULL);
    cudaFree(d_input);
    cudaFree(d_output); 
    gst_object_unref(pipeline_capture);
    gst_object_unref(pipeline_display);
    gst_object_unref(bus);
    g_main_loop_unref(loop);

    return 0;
}