#include <stdio.h>
#include <iostream>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>

using namespace std;

int main(int argc, char* argv[]) {

    GstElement* pipeline, * source, * convert, * sink;
    GstBus* bus;
    GstMessage* msg;

    gst_init(&argc, &argv);

    source = gst_element_factory_make("mfvideosrc", "source");
    sink = gst_element_factory_make("appsink", "sink");
    convert = gst_element_factory_make("videoconvert", "convert");
    pipeline = gst_pipeline_new("webcam-pipeline");

    if (!pipeline || !source || !convert || !sink) {
        g_printerr("Not all elements could be created.\n");
        return -1;
    }

    g_object_set(sink, "emit-signals", FALSE,  "sync", FALSE, NULL);

    // Force BGR format
    GstCaps *caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "BGR", NULL);
    g_object_set(sink, "caps", caps, NULL);
    gst_caps_unref(caps);


    gst_bin_add_many(GST_BIN(pipeline), source, convert, sink, NULL); //Null, stops reading arguments 

    if (!gst_element_link_many(source, convert, sink, NULL)) {
        g_printerr("Elements could not be linked.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    cout << "Streaming..." << endl;

    while (true) {
        GstSample *sample = gst_app_sink_pull_sample(GST_APP_SINK(sink));
        if (!sample) {
            cout << "No sample received" << endl;
            break;
        }

        GstBuffer *buffer = gst_sample_get_buffer(sample);
        GstCaps *sample_caps = gst_sample_get_caps(sample);
        GstStructure *s = gst_caps_get_structure(sample_caps, 0);

        int width, height;
        gst_structure_get_int(s, "width", &width);
        gst_structure_get_int(s, "height", &height);

        GstMapInfo map;
        if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
            unsigned char* data = map.data;
            cout << "Frame: " << width << "x" << height << " | size: " << map.size << endl;
            gst_buffer_unmap(buffer, &map);
        }

        gst_sample_unref(sample);
    }

    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    return 0;

}
