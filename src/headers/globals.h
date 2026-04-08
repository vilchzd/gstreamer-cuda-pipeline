#ifndef GLOBALS_H
#define GLOBALS_H

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <atomic>
#include <chrono>

using namespace std;
using namespace chrono;

extern GMainLoop* global_loop;
extern GstAppSrc* appsrc_display;
extern unsigned char* d_input;
extern unsigned char* d_output;

extern int grid;
extern atomic<bool> filter_enabled;
extern atomic<bool> running;

extern int frame_count;
extern float kernel_time;
extern float total_kernel;
extern double total_lat;
extern steady_clock::time_point last_time;

#endif