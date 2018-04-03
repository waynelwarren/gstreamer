#include <stdio.h>
#include <gst/gst.h>
#include <glib.h>

typedef struct _CustomData {
  GstElement *pipeline;
  GstElement *source;
  GstElement *demuxer;
  GstElement *mixer;
  GstElement *sink;
  int count;
} CustomData;

static gboolean print_field (GQuark field, const GValue * value, gpointer pfx)
{
    gchar *str = gst_value_serialize (value);

    g_print ("%s  %15s: %s\n", (gchar *) pfx, g_quark_to_string (field), str);
    g_free (str);
    return TRUE;
}

static void print_caps (const GstCaps * caps, const gchar * pfx)
{
    guint i;

    g_return_if_fail (caps != NULL);

    if (gst_caps_is_any (caps)) {
        g_print ("%sANY\n", pfx);
        return;
    }
    if (gst_caps_is_empty (caps)) {
        g_print ("%sEMPTY\n", pfx);
        return;
    }

    for (i = 0; i < gst_caps_get_size (caps); i++) {
        GstStructure *structure = gst_caps_get_structure (caps, i);

        g_print ("%s%s\n", pfx, gst_structure_get_name (structure));
        gst_structure_foreach (structure, print_field, (gpointer) pfx);
    }
}

/* Shows the CURRENT capabilities of the requested pad in the given element */
static void print_pad_capabilities (GstElement *element, gchar *pad_name)
{
    GstPad *pad = NULL;
    GstCaps *caps = NULL;

    /* Retrieve pad */
    pad = gst_element_get_static_pad (element, pad_name);
    if (!pad) {
        g_printerr ("Could not retrieve pad '%s'\n", pad_name);
        return;
    }

    /* Retrieve negotiated caps (or acceptable caps if negotiation is not finished yet) */
    caps = gst_pad_get_current_caps (pad);
    if (!caps)
        caps = gst_pad_query_caps (pad, NULL);

    /* Print and free */
    g_print ("===========================\nCaps for the %s pad of %s:\n", pad_name, GST_ELEMENT_NAME(element));
    print_caps (caps, "      ");
    gst_caps_unref (caps);
    gst_object_unref (pad);
}


static gboolean bus_call (GstBus *bus, GstMessage *msg, gpointer data)
{
    GMainLoop *loop = (GMainLoop *) data;

    // g_print("[%s:%d] ", GST_MESSAGE_TYPE_NAME(msg), GST_MESSAGE_TYPE(msg));
    switch (GST_MESSAGE_TYPE (msg)) {

        case GST_MESSAGE_STREAM_STATUS:
            // g_print("%s stream status\n", GST_MESSAGE_SRC_NAME(msg));
            break;

        case GST_MESSAGE_DURATION_CHANGED:
            // g_print("%s duration changed\n", GST_MESSAGE_SRC_NAME(msg));
            break;
            
        case GST_MESSAGE_LATENCY:
            // g_print("%s latency changed\n", GST_MESSAGE_SRC_NAME(msg));
            break;
            
        case GST_MESSAGE_EOS:
            g_print ("End of stream\n");
            g_main_loop_quit (loop);
            break;

        case GST_MESSAGE_ERROR: {
            gchar  *debug;
            GError *error;

            gst_message_parse_error (msg, &error, &debug);

            g_printerr ("Error: %s\n"
                        "       %s\n", debug, error->message);
            g_free (debug);
            g_error_free (error);

            g_main_loop_quit (loop);
            break;
        }

        case GST_MESSAGE_STATE_CHANGED:
            {
                GstState old_state, new_state, pending_state;
                gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);
                // g_print ("%s state changed from %s to %s, pending=%s\n", GST_MESSAGE_SRC_NAME(msg),
                    // gst_element_state_get_name(old_state), gst_element_state_get_name(new_state),
                    // gst_element_state_get_name(pending_state));
            }
            break;

        default:
            break;

        // g_print("\n");
    }

    return TRUE;
}

static void on_no_more_pads(GstElement *element, gpointer data)
{
    CustomData *d = (CustomData *)data;
    GstStateChangeReturn ret;

    g_print("No more pads, %s\n", GST_ELEMENT_NAME(element));
    GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(d->pipeline), GST_DEBUG_GRAPH_SHOW_ALL, "mkv-done");

    ret = gst_element_set_state (d->pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr ("Unable to set the pipeline to the playing state.\n");
        gst_object_unref(d->pipeline);
        return;
    }
    GST_DEBUG_BIN_TO_DOT_FILE(GST_BIN(d->pipeline), GST_DEBUG_GRAPH_SHOW_ALL, "mkv-playing");
}

#define CAM_WIDTH  200
#define CAM_HEIGHT 150
static void on_pad_added (GstElement *element, GstPad *pad, gpointer data)
{
    GstElement *inqueue, *parser, *decoder, *outqueue;
    CustomData *d = (CustomData *)data;
    char ename[20];
    GstPad *vmix_pad, *src_pad;
    /* if (d->count >= 1) return; */
    GstCaps *caps =  gst_caps_new_simple(
                        "video/x-raw",
                        "framerate", GST_TYPE_FRACTION, 30, 1,
                        "width", G_TYPE_INT, CAM_WIDTH,
                        "height", G_TYPE_INT, CAM_HEIGHT,
                        NULL);

    sprintf(ename, "inqueue%02d", d->count);      inqueue  = gst_element_factory_make ("queue",      ename);
    sprintf(ename, "h264-parser%02d", d->count);  parser   = gst_element_factory_make ("h264parse",  ename);
    sprintf(ename, "h264-decoder%02d", d->count); decoder  = gst_element_factory_make ("avdec_h264", ename);
    sprintf(ename, "inqueuout%02d", d->count);    outqueue = gst_element_factory_make ("queue",      ename);

    if (!inqueue || !parser || !decoder || !outqueue) {
        g_printerr("Failed to create an element in on_pad_added()\n");
        return;
    }

    vmix_pad = gst_element_get_request_pad(d->mixer, "sink_%u");
    g_print("Obtained request pad %s for mixer\n", gst_pad_get_name(vmix_pad));
    print_pad_capabilities(d->mixer, gst_pad_get_name(vmix_pad));

    src_pad = gst_element_get_static_pad(outqueue, "src");
    print_pad_capabilities(outqueue, "src");

    g_object_set(vmix_pad, "xpos",   0, NULL);
    g_object_set(vmix_pad, "ypos",   d->count * CAM_WIDTH, NULL);
    g_object_set(vmix_pad, "zorder", d->count, NULL);

    gst_bin_add_many (GST_BIN (d->pipeline), inqueue, parser, decoder, outqueue, NULL);
    gst_element_sync_state_with_parent(inqueue);
    gst_element_sync_state_with_parent(parser);
    gst_element_sync_state_with_parent(decoder);
    gst_element_sync_state_with_parent(outqueue);

    gst_element_link_many(element, inqueue, parser, decoder, outqueue, NULL);
    gst_element_link_filtered(gst_pad_get_parent_element(src_pad),
                              gst_pad_get_parent_element(vmix_pad),
                              caps);
    gst_caps_unref(caps);
    gst_object_unref(src_pad);

    d->count++;
}

int main (int argc, char *argv[])
{
    GMainLoop *loop;
    CustomData data;
    GstBus *bus;
    guint bus_watch_id;
    GstStateChangeReturn ret;

    /* Initialisation */
    gst_init (&argc, &argv);

    loop = g_main_loop_new (NULL, FALSE);

    /* Check input arguments */
    if (argc != 2) {
        g_printerr ("Usage: multi-sink <filename>.mkv\n");
        return -1;
    }

    /* Create the elements to start the pipeline */
    data.pipeline = gst_pipeline_new ("video-player");
    data.source   = gst_element_factory_make ("filesrc",       "file-source");
    data.demuxer  = gst_element_factory_make ("matroskademux", "matroska-demuxer");
    data.count = 0;

    if (!data.pipeline || !data.source || !data.demuxer) {
        g_printerr ("One element could not be created. Exiting.\n");
        return -1;
    }

    g_object_set (G_OBJECT (data.source), "location", argv[1], NULL);

    /* create the elements to end the pipeline */
    data.mixer =  gst_element_factory_make ("videomixer",    "mixer");
    data.sink  =  gst_element_factory_make ("autovideosink", "sink");

    if (!data.mixer || !data.sink) {
        g_printerr ("One element could not be created. Exiting.\n");
        return -1;
    }

    print_pad_capabilities(data.source, "src");
    print_pad_capabilities(data.demuxer, "sink");

    bus = gst_pipeline_get_bus (GST_PIPELINE (data.pipeline));
    bus_watch_id = gst_bus_add_watch (bus, bus_call, loop);
    gst_object_unref (bus);

    gst_bin_add_many (GST_BIN (data.pipeline), data.source, data.demuxer, data.mixer, data.sink, NULL);

    /* the first two elements, and the last two elements, only created once */
    gst_element_link (data.source, data.demuxer);
    gst_element_link (data.mixer, data.sink);

    g_signal_connect (data.demuxer, "pad-added", G_CALLBACK (on_pad_added), &data);
    g_signal_connect (data.demuxer, "no-more-pads", G_CALLBACK (on_no_more_pads), &data);

    g_print ("Now playing: %s\n", argv[1]);
    ret = gst_element_set_state (data.pipeline, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_printerr ("Unable to set the pipeline to the playing state.\n");
        gst_object_unref(data.pipeline);
        return -1;
    }

    g_print ("Running...\n");
    g_main_loop_run (loop);

    g_print ("Returned, stopping playback\n");
    gst_element_set_state (data.pipeline, GST_STATE_NULL);

    g_print ("Deleting pipeline\n");
    gst_object_unref (GST_OBJECT (data.pipeline));
    g_source_remove (bus_watch_id);
    g_main_loop_unref (loop);

    return 0;
}
