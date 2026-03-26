#include <stdio.h>
#include <iostream>
#include <gst/gst.h>

using namespace std;

int main(int argc, char* argv[]) {

    GstElement* pipeline, * source, * convert, * sink;
    GstBus* bus;
    GstMessage* msg;

    gst_init(&argc, &argv);

    source = gst_element_factory_make("mfvideosrc", "source");
    sink = gst_element_factory_make("autovideosink", "sink");
    convert = gst_element_factory_make("videoconvert", "convert");
    pipeline = gst_pipeline_new("webcam-pipeline");

    if (!pipeline || !source || !convert || !sink) {
        g_printerr("Not all elements could be created.\n");
        return -1;
    }

    gst_bin_add_many(GST_BIN(pipeline), source, convert, sink, NULL); //Null, stops reading arguments 


    if (!gst_element_link_many(source, convert, sink, NULL)) {
        g_printerr("Elements could not be linked.\n");
        gst_object_unref(pipeline);
        return -1;
    }

    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    bus = gst_element_get_bus(pipeline);
    msg = gst_bus_timed_pop_filtered(bus,GST_CLOCK_TIME_NONE, (GstMessageType)(GST_MESSAGE_ERROR | GST_MESSAGE_EOS));

    if (msg != NULL) {
        gst_message_unref(msg);
    }

    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(pipeline);

    return 0;

}
