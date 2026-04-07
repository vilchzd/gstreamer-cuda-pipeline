#include <cuda_runtime.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <atomic>

#include "globals.h"
#include "callbacks.h"
#include "blur_kernel.h"

using namespace std;
using namespace chrono;

GMainLoop *global_loop = nullptr;
GstAppSrc *appsrc_display = nullptr;
unsigned char *d_input = nullptr;
unsigned char *d_output = nullptr;
atomic<bool> filter_enabled(false);
atomic<bool> running(true);

int grid = GRID_SIZE;
int frame_count = 0;
float kernel_time = 0;
float total_kernel = 0;
double total_lat = 0;

steady_clock::time_point last_time = steady_clock::now();

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
    if (gst_element_set_state(pipeline_capture, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE || 
        gst_element_set_state(pipeline_display, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        g_printerr("Failed to start pipelines.\n");
        return -1;
    }

    cout << ">>>Starting Pipeline" << endl;
    cout << "Streaming webcam footage\n" << endl;

    thread key_thread(keyboard_inputs);
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