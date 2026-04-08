#include <cuda_runtime.h>
#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>
#include <iostream>
#include <iomanip>
#include <chrono>

#include "globals.h"
#include "callbacks.h"
#include "blur_kernel.h"


//--------------------------- Appsink Callback ------------------------------//
GstFlowReturn new_sample(GstAppSink* appsink, gpointer user_data) {

    auto frame_start = high_resolution_clock::now();

    GstSample *sample = gst_app_sink_pull_sample(appsink);
    if (!sample) return GST_FLOW_ERROR;

    GstBuffer *buffer = gst_sample_get_buffer(sample);
    GstCaps *caps = gst_sample_get_caps(sample);
    GstStructure *structure = gst_caps_get_structure(caps, 0);

    int width, height;
    gst_structure_get_int(structure, "width", &width);
    gst_structure_get_int(structure, "height", &height); 
    size_t pixels_per_frame = width * height;
    size_t pixel_ops_per_pixel = 0;

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

        pixel_ops_per_pixel = (2 * grid + 1) * (2 * grid + 1);
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
            gst_sample_unref(sample);
            return GST_FLOW_ERROR;
        }

        gpu_wrapper_blurBGR(map.data, out_map.data, d_input, d_output, width, height, buffer_size, grid);  //<<--------------------- CUDA kernel 

        gst_buffer_unmap(out_buffer, &out_map);

    } else {
        kernel_time = 0;
        out_buffer = gst_buffer_ref(buffer);
    }

    gst_buffer_unmap(buffer, &map);
    
    GST_BUFFER_PTS(out_buffer) = GST_BUFFER_PTS(buffer);                     
    GST_BUFFER_DURATION(out_buffer) = GST_BUFFER_DURATION(buffer);

    GstFlowReturn ret = gst_app_src_push_buffer(appsrc_display, out_buffer);

    gst_sample_unref(sample);

    auto now = steady_clock::now();
    auto elapsed = duration_cast<seconds>(now - last_time).count();
    auto frame_end = high_resolution_clock::now();
    double latency = duration<double,milli>(frame_end - frame_start).count();
    total_lat += latency;
    total_kernel += kernel_time;
    frame_count++;
    
    if (elapsed >= 1) {

        cout << "\033[s";        
        cout << "\033[4;1H\033[K";      
        double pos = frame_count / elapsed * pixels_per_frame * pixel_ops_per_pixel;
        cout << "Res: " << width << "x" << height << " | Block Size: " << BLOCK_SIZE 
             << " | Grid: " << 2*grid+1 << "x" << 2*grid+1 << " |";
        cout << "\033[5;1H\033[K";  
        cout <<"FPS: " << frame_count / elapsed << " | Latency: "<<  total_lat / frame_count << "ms | Kernel Time: " << total_kernel / frame_count
             << "ms | POS: " << fixed << setprecision(2) << pos / 1e9 << " Gpx/s";
        cout << "\033[u";           
        cout << flush;
        frame_count = 0;
        total_kernel = 0;
        total_lat = 0;
        last_time = now;
    }

    if (ret != GST_FLOW_OK) {
        g_printerr("Push buffer failed: %d\n", ret);
    }

    return GST_FLOW_OK;
}

//--------------------------- Bus Callback ----------------------------------//
void on_message(GstBus* bus, GstMessage* message, gpointer user_data) {
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

//...............................Key Input...................................//
void keyboard_inputs() {
    while (running) {

        cout << "\033[6;1H\033[K";
        cout << "Blur: " << (filter_enabled ? "ON" : "OFF") << "     ";
        cout << "\033[7;1H\033[J";  
        cout << "Command (t=toggle | i=increase | u=decrease | q=quit): " << flush;

        char input;
        cin >> input;   

        if (input == 't') {
            filter_enabled = !filter_enabled;
            cout << "\033[6;1H\033[K";
            cout << "Blur: " << (filter_enabled ? "ON" : "OFF") << "     ";
            cout << "\033[7;1H\033[J"; 
            cout << "Command (t=toggle | i=increase | u=decrease | q=quit): " << flush;
        }
        else if (input == 'i') {
            if (grid < 48) {
                grid += 1;
            }
        }
        else if (input == 'u') {
            if (grid > 0) {
              grid -= 1;
            }
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
