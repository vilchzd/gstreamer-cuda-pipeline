#ifndef CALLBACKS_H
#define CALLBACKS_H

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <cuda_runtime.h>

GstFlowReturn new_sample(GstAppSink* appsink, gpointer user_data);
void keyboard_inputs();
void on_message(GstBus* bus, GstMessage* message, gpointer user_data);

#endif