#include <stdio.h>
#include <stdlib.h>
#include <gst/gst.h>

int main(int argc, char *argv[])
{
    GstElement *pipeline, *source0, *source1, *source2, *sink, *mixer;
    GstBus *bus;
    GstMessage *msg;
    GstStateChangeReturn ret;

    if (argc < 3) {
        printf("need pattern indices\n");
        return -1;
    }

    /* Initialize GStreamer */
    gst_init (&argc, &argv);

    /* Create the elements */
    source0 = gst_element_factory_make ("videotestsrc",  "source0");
    source1 = gst_element_factory_make ("videotestsrc",  "source1");
    source2 = gst_element_factory_make ("videotestsrc",  "source2");
    mixer  =  gst_element_factory_make ("videomixer",    "mixer");
    sink   =  gst_element_factory_make ("autovideosink", "sink");

    /* Create the empty pipeline */
    pipeline = gst_pipeline_new ("test-pipeline");

    if (!pipeline || !source0 || !source1 || !source2 || !sink) {
        g_printerr ("Not all elements could be created.\n");
        return -1;
    }

    /* Build the pipeline */
    gst_bin_add_many (GST_BIN (pipeline), source0, source1, source2, mixer, sink, NULL);
    if (gst_element_link (mixer, sink) != TRUE) {
        g_printerr ("Elements could not be linked.\n");
        gst_object_unref (pipeline);
        return -1;
    }

    /* source0 */
    {
        GstPad *vmix_pad, *src_pad;
        GstCaps *caps =  gst_caps_new_simple(
                            "video/x-raw",
                            "framerate", GST_TYPE_FRACTION, 10, 1,
                            "width", G_TYPE_INT, 40,
                            "height", G_TYPE_INT, 80,
                            NULL);

        g_object_set (source0, "pattern", atoi(argv[1]), NULL);

        /* connect the sources to a requested pad on the mixer */
        vmix_pad = gst_element_get_request_pad(mixer, "sink_%u");
        g_print("Obtained request pad %s for mixer\n", gst_pad_get_name(vmix_pad));

        src_pad = gst_element_get_static_pad(source0, "src");

        g_object_set(vmix_pad, "xpos",   0, NULL);
        g_object_set(vmix_pad, "ypos",   0, NULL);
        g_object_set(vmix_pad, "zorder", 2, NULL);

        gst_element_link_filtered(gst_pad_get_parent_element(src_pad),
                                  gst_pad_get_parent_element(vmix_pad),
                                  caps);

        gst_caps_unref(caps);
        gst_object_unref(src_pad);
    }

    /* source1 */
    {
        GstPad *vmix_pad, *src_pad;
        GstCaps *caps = gst_caps_new_simple(
                            "video/x-raw",
                            "framerate", GST_TYPE_FRACTION, 10, 1,
                            "width", G_TYPE_INT, 200,
                            "height", G_TYPE_INT, 150,
                            NULL);

        g_object_set (source1, "pattern", atoi(argv[2]), NULL);

        /* connect the sources to a requested pad on the mixer */
        vmix_pad = gst_element_get_request_pad(mixer, "sink_%u");
        g_print("Obtained request pad %s for mixer\n", gst_pad_get_name(vmix_pad));

        src_pad = gst_element_get_static_pad(source1, "src");

        g_object_set(vmix_pad, "xpos",  20, NULL);
        g_object_set(vmix_pad, "ypos",  20, NULL);
        g_object_set(vmix_pad, "zorder", 1, NULL);

        gst_element_link_filtered(gst_pad_get_parent_element(src_pad),
                                  gst_pad_get_parent_element(vmix_pad),
                                  caps);

        gst_caps_unref(caps);
        gst_object_unref(src_pad);
    }

    /* source2 */
    {
        GstPad *vmix_pad, *src_pad;
        GstCaps *caps = gst_caps_new_simple(
                            "video/x-raw",
                            "framerate", GST_TYPE_FRACTION, 10, 1,
                            "width", G_TYPE_INT, 640,
                            "height", G_TYPE_INT, 360,
                            NULL);

        g_object_set (source2, "pattern", atoi(argv[3]), NULL);

        /* connect the sources to a requested pad on the mixer */
        vmix_pad = gst_element_get_request_pad(mixer, "sink_%u");
        g_print("Obtained request pad %s for mixer\n", gst_pad_get_name(vmix_pad));

        src_pad = gst_element_get_static_pad(source2, "src");

        g_object_set(vmix_pad, "xpos",   0, NULL);
        g_object_set(vmix_pad, "ypos",   0, NULL);
        g_object_set(vmix_pad, "zorder", 0, NULL);

        gst_element_link_filtered(gst_pad_get_parent_element(src_pad),
                                  gst_pad_get_parent_element(vmix_pad),
                                  caps);

        gst_caps_unref(caps);
        gst_object_unref(src_pad);
    }

    ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr ("Unable to set the pipeline to the playing state.\n");
        gst_object_unref (pipeline);
        return -1;
    }

    /* Wait until error or EOS */
    bus = gst_element_get_bus (pipeline);
    msg = gst_bus_timed_pop_filtered (bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);

    GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(pipeline), GST_DEBUG_GRAPH_SHOW_ALL, "multi-src");

    /* Parse message */
    if (msg != NULL) {
        GError *err;
        gchar *debug_info;

        switch (GST_MESSAGE_TYPE (msg)) {
            case GST_MESSAGE_ERROR:
                gst_message_parse_error (msg, &err, &debug_info);
                g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
                g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
                g_clear_error (&err);
                g_free (debug_info);
                break;
            case GST_MESSAGE_EOS:
                g_print ("End-Of-Stream reached.\n");
                break;
            default:
                /* We should not reach here because we only asked for ERRORs and EOS */
                g_printerr ("Unexpected message received.\n");
                break;
        }
        gst_message_unref (msg);
    }

    /* Free resources */
    gst_object_unref (bus);
    gst_element_set_state (pipeline, GST_STATE_NULL);
    gst_object_unref (pipeline);
    return 0;
}
