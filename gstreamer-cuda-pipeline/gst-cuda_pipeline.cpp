/*
// #include <stdio.h>
// #include <iostream>
// #include <gst/gst.h>
// #include <gst/app/gstappsink.h>
// #include <gst/app/gstappsrc.h>

// using namespace std;

// int main(int argc, char* argv[]) {

//     GstElement *pipeline, *source, *convert, *sink;
//     GstElement *appsrc, *display_convert, *videosink;
//     //GstBus *bus;
//     //GstMessage *msg;

//     gst_init(&argc, &argv);

//     source = gst_element_factory_make("mfvideosrc", "source");
//     sink = gst_element_factory_make("appsink", "sink");
//     convert = gst_element_factory_make("videoconvert", "convert");

//     appsrc = gst_element_factory_make("appsrc", "mysrc");
//     display_convert = gst_element_factory_make("videoconvert", "display_convert");
//     videosink = gst_element_factory_make("autovideosink", "videosink");


//     pipeline = gst_pipeline_new("webcam-pipeline");

//     if (!pipeline || !source || !convert || !sink || !appsrc || !display_convert || !videosink) {
//         g_printerr("Not all elements could be created.\n");
//         return -1;
//     }

//     GstCaps *caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "BGR", NULL);
//     g_object_set(sink, "caps", caps, "emit-signals", FALSE, "sync", FALSE, "max-buffers", 1, "drop", TRUE, NULL);
//     g_object_set(appsrc, "format", GST_FORMAT_TIME,  "is-live", TRUE, "block", FALSE, NULL);
//     gst_caps_unref(caps);

//     gst_bin_add_many(GST_BIN(pipeline), source, convert, sink, appsrc, display_convert, videosink, NULL);
//     if (!gst_element_link_many(source, convert, sink, NULL) || !gst_element_link_many(appsrc, display_convert, videosink, NULL)) {
//         g_printerr("Elements could not be linked.\n");
//         gst_object_unref(pipeline);
//         return -1;
//     }

//     gst_element_set_state(pipeline, GST_STATE_PLAYING);
//     cout << "Streaming..." << endl;
    

//     GstSample *first_sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
//     if (!first_sample) {
//         cout << "Failed to get initial sample" << endl;
//         return -1;
//     }

//     GstCaps *sample_caps = gst_sample_get_caps(first_sample);

//     // Set caps ONCE on appsrc
//     gst_app_src_set_caps(GST_APP_SRC(appsrc), sample_caps);

//     // Push first buffer
//     GstBuffer *first_buffer = gst_sample_get_buffer(first_sample);
//     gst_buffer_ref(first_buffer);

//     GST_BUFFER_PTS(first_buffer) = gst_util_uint64_scale(g_get_monotonic_time(), GST_USECOND, 1);
//     GST_BUFFER_DURATION(first_buffer) = gst_util_uint64_scale_int(1, GST_SECOND, 30);

//     gst_app_src_push_buffer(GST_APP_SRC(appsrc), first_buffer);

//     gst_sample_unref(first_sample);

//     while (true) {

//         GstSample *sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
        
//         if (!sample) {
//             cout << "No sample received" << endl;
//             break;
//         }

//         GstBuffer *buffer = gst_sample_get_buffer(sample);
//         GstCaps *sample_caps = gst_sample_get_caps(sample);
//         GstStructure *structure = gst_caps_get_structure(sample_caps, 0);

//         // int width, height;
//         // gst_structure_get_int(structure, "width", &width);
//         // gst_structure_get_int(structure, "height", &height);

//         gst_buffer_ref(buffer);

//         //st_app_src_push_buffer(GST_APP_SRC(appsrc), buffer);
        
//         GST_BUFFER_PTS(buffer) = gst_util_uint64_scale(g_get_monotonic_time(), GST_USECOND, 1);
//         GST_BUFFER_DURATION(buffer) = gst_util_uint64_scale_int(1, GST_SECOND, 30);

//         GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(appsrc), buffer);

//         if (ret != GST_FLOW_OK) {
//             cout << "Push failed: " << ret << endl;
//         }

//         gst_sample_unref(sample);
//     }

//     gst_element_set_state(pipeline, GST_STATE_NULL);
//     gst_object_unref(pipeline);

//     return 0;

// }
*/
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <iostream>
#include <chrono>

using namespace std;
using namespace std::chrono;

GstAppSrc* appsrc_display = nullptr;

/*
static int frame_count = 0;
static steady_clock::time_point last_time = steady_clock::now();
*/


static GstFlowReturn on_new_sample(GstAppSink* appsink, gpointer user_data) {

    GstSample* sample = gst_app_sink_pull_sample(appsink);
    if (!sample) return GST_FLOW_ERROR;

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    GstCaps* caps = gst_sample_get_caps(sample);

    static bool caps_set = false;
    if (!caps_set) {
        GstCaps* newcaps = gst_caps_copy(caps);
        gst_app_src_set_caps(appsrc_display, newcaps);
        gst_caps_unref(newcaps);
        caps_set = true;
    }

   
    GstBuffer* out_buffer = gst_buffer_ref(buffer);
    GST_BUFFER_PTS(out_buffer) = GST_BUFFER_PTS(buffer);                     //Timestamps
    GST_BUFFER_DURATION(out_buffer) = GST_BUFFER_DURATION(buffer);

    GstFlowReturn ret = gst_app_src_push_buffer(appsrc_display, out_buffer);

    gst_sample_unref(sample);

    /*  
    frame_count++;
    auto now = steady_clock::now();
    auto elapsed = duration_cast<seconds>(now - last_time).count();
    if (elapsed >= 1) {
        cout << "FPS: " << frame_count / elapsed << endl;
        frame_count = 0;
        last_time = now;
    }

    */

    if (ret != GST_FLOW_OK) {
        g_printerr("Push buffer failed: %d\n", ret);
    }

    return GST_FLOW_OK;
}

int main(int argc, char* argv[]) {

    gst_init(&argc, &argv);

    //------------------------Capture pipeline ------------------------------//
    GstElement* pipeline_capture = gst_pipeline_new("capture-pipeline");
    GstElement* source = gst_element_factory_make("mfvideosrc", "source"); 
    GstElement* convert = gst_element_factory_make("videoconvert", "convert");
    GstElement* appsink = gst_element_factory_make("appsink", "appsink");

    if (!pipeline_capture || !source || !convert || !appsink) {
        g_printerr("Failed to create capture elements.\n");
        return -1;
    }

    g_object_set(appsink, "emit-signals", TRUE, "sync", FALSE, "max-buffers", 1, "drop", TRUE, NULL);

    g_signal_connect(appsink, "new-sample", G_CALLBACK(on_new_sample), nullptr);

    gst_bin_add_many(GST_BIN(pipeline_capture), source, convert, appsink, NULL);
    if (!gst_element_link_many(source, convert, appsink, NULL)) {
        g_printerr("Failed to link capture pipeline.\n");
        return -1;
    }

    //----------------------- Display pipeline ------------------------------//
    GstElement* pipeline_display = gst_pipeline_new("display-pipeline");
    appsrc_display = GST_APP_SRC(gst_element_factory_make("appsrc", "appsrc"));
    GstElement* queue = gst_element_factory_make("queue", "queue_display");
    GstElement* convert_display = gst_element_factory_make("videoconvert", "convert_display");
    GstElement* videosink = gst_element_factory_make("autovideosink", "videosink");

    if (!pipeline_display || !appsrc_display || !queue || !convert_display || !videosink) {
        g_printerr("Failed to create display elements.\n");
        return -1;
    }

    g_object_set(appsrc_display, "format", GST_FORMAT_TIME, "is-live", TRUE, "block", FALSE, NULL);

    g_object_set(queue, "max-size-buffers", 10, "leaky", 2, NULL);

    gst_bin_add_many(GST_BIN(pipeline_display), GST_ELEMENT(appsrc_display), queue, convert_display, videosink, NULL);
    if (!gst_element_link_many(GST_ELEMENT(appsrc_display), queue, convert_display, videosink, NULL)) {
        g_printerr("Failed to link display pipeline.\n");
        return -1;
    }

    //------------------------ Initialize pipelines -------------------------//
    gst_element_set_state(pipeline_capture, GST_STATE_PLAYING);
    gst_element_set_state(pipeline_display, GST_STATE_PLAYING);
    cout << "Streaming webcam footage" << endl;

    GMainLoop* loop = g_main_loop_new(nullptr, FALSE);                                   // Main loop
    g_main_loop_run(loop);

    //------------------------ Cleanup -------------------------------------//
    gst_element_set_state(pipeline_capture, GST_STATE_NULL);
    gst_element_set_state(pipeline_display, GST_STATE_NULL);
    gst_object_unref(pipeline_capture);
    gst_object_unref(pipeline_display);
    g_main_loop_unref(loop);

    return 0;
}