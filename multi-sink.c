#include <stdio.h>
#include <gst/gst.h>
#include <glib.h>

typedef struct _CustomData {
  GstElement *pipeline;
  GstElement *source;
  GstElement *demuxer;
  int count;
} CustomData;

static gboolean bus_call (GstBus *bus, GstMessage *msg, gpointer data)
{
    GMainLoop *loop = (GMainLoop *) data;

    switch (GST_MESSAGE_TYPE (msg)) {

        case GST_MESSAGE_EOS:
            g_print ("End of stream\n");
            g_main_loop_quit (loop);
            break;

        case GST_MESSAGE_ERROR: {
            gchar  *debug;
            GError *error;

            gst_message_parse_error (msg, &error, &debug);
            g_free (debug);

            g_printerr ("Error: %s\n", error->message);
            g_error_free (error);

            g_main_loop_quit (loop);
            break;
        }
        default:
            break;
    }

    return TRUE;
}

static void on_pad_added (GstElement *element, GstPad *pad, gpointer data)
{
    GstElement *queue, *parser, *decoder, *sink;
    CustomData *d = (CustomData *)data;
    char ename[20];

    if (d->count >= 3)
        return;

    sprintf(ename, "queue%02d", d->count);        queue   = gst_element_factory_make ("queue",         ename);
    sprintf(ename, "h264-parser%02d", d->count);  parser  = gst_element_factory_make ("h264parse",     ename);
    sprintf(ename, "h264-decoder%02d", d->count); decoder = gst_element_factory_make ("avdec_h264",    ename);
    sprintf(ename, "video-sink%02d", d->count);   sink    = gst_element_factory_make ("autovideosink", ename);

    if (!queue || !parser || !decoder || !sink) {
        g_printerr("Failed to create an element in on_pad_added()\n");
        return;
    }
    d->count++;

    gst_bin_add_many (GST_BIN (d->pipeline), queue, parser, decoder, sink, NULL);
    gst_element_sync_state_with_parent(queue);
    gst_element_sync_state_with_parent(parser);
    gst_element_sync_state_with_parent(decoder);
    gst_element_sync_state_with_parent(sink);

    gst_element_link_many(element, queue, parser, decoder, sink, NULL);
}

int main (int argc, char *argv[])
{
    GMainLoop *loop;
    CustomData data;
    GstBus *bus;
    guint bus_watch_id;

    /* Initialisation */
    gst_init (&argc, &argv);

    loop = g_main_loop_new (NULL, FALSE);

    /* Check input arguments */
    if (argc != 2) {
        g_printerr ("Usage: multi-sink <filename>.mkv\n");
        return -1;
    }

    /* Create gstreamer elements */
    data.pipeline = gst_pipeline_new ("video-player");
    data.source   = gst_element_factory_make ("filesrc",       "file-source");
    data.demuxer  = gst_element_factory_make ("matroskademux", "matroska-demuxer");
    data.count = 0;

    if (!data.pipeline || !data.source || !data.demuxer) {
        g_printerr ("One element could not be created. Exiting.\n");
        return -1;
    }

    g_object_set (G_OBJECT (data.source), "location", argv[1], NULL);

    bus = gst_pipeline_get_bus (GST_PIPELINE (data.pipeline));
    bus_watch_id = gst_bus_add_watch (bus, bus_call, loop);
    gst_object_unref (bus);

    gst_bin_add_many (GST_BIN (data.pipeline), data.source, data.demuxer, NULL);

    gst_element_link (data.source, data.demuxer);
    g_signal_connect (data.demuxer, "pad-added", G_CALLBACK (on_pad_added), &data);

    g_print ("Now playing: %s\n", argv[1]);
    gst_element_set_state (data.pipeline, GST_STATE_PLAYING);

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
