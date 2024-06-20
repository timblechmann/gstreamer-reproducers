#include <gst/gst.h>

#include <cassert>
#include <cstdio>
#include <set>
#include <thread>

using namespace std;
using namespace std::chrono_literals;

int select_stream_cb(GstElement *decodebin,
                     GstStreamCollection *collection,
                     GstStream *stream,
                     gpointer udata)
{
    return 0;
};

int main(int argc, char *argv[])
{
    GstMessage *msg;

    /* Initialize GStreamer */
    gst_init(&argc, &argv);

    /* Build the pipeline */
    GstElement *pipeline
        = gst_parse_launch("playbin3 "
                           "uri=file:///home/tim/Videos/multitrack-with-metadata.mkv",
                           NULL);

    GstBus *bus = gst_element_get_bus(pipeline);

    auto waitFor = [&](auto expectedState) {
        for (;;) {
            GstState state, pending;
            GstStateChangeReturn ret = gst_element_get_state(pipeline,
                                                             &state,
                                                             &pending,
                                                             5'000'000'000);

            fprintf(stderr, "gst_element_get_state result %d -> state %d\n", (int) ret, (int) state);

            if (ret == GST_STATE_CHANGE_SUCCESS && state == expectedState)
                return;
            if (ret == GST_STATE_CHANGE_ASYNC) {
                auto end = std::chrono::steady_clock::now();
                fprintf(stderr, "async failure\n");
            }

            std::this_thread::sleep_for(10ms);
        }
    };

    auto setStateAndWait = [&](auto expectedState) {
        gst_element_set_state(pipeline, expectedState);

        for (;;) {
            GstState state, pending;
            GstStateChangeReturn ret = gst_element_get_state(pipeline,
                                                             &state,
                                                             &pending,
                                                             5'000'000'000);

            fprintf(stderr, "gst_element_get_state result %d -> state %d\n", (int) ret, (int) state);

            if (ret == GST_STATE_CHANGE_SUCCESS && state == expectedState)
                return;
            if (ret == GST_STATE_CHANGE_ASYNC) {
                auto end = std::chrono::steady_clock::now();
                fprintf(stderr, "async failure\n");
            }

            std::this_thread::sleep_for(10ms);
        }
    };

    setStateAndWait(GST_STATE_PAUSED);

    GstStreamCollection *mediaStreams = nullptr;
    {
        GstMessage *msg = gst_bus_timed_pop_filtered(bus,
                                                     GST_CLOCK_TIME_NONE,
                                                     GstMessageType(GST_MESSAGE_STREAM_COLLECTION));

        gst_message_parse_stream_collection(msg, &mediaStreams);

        for (int i = 0; i != gst_stream_collection_get_size(mediaStreams); ++i) {
            GstStream *stream = gst_stream_collection_get_stream(mediaStreams, i);
            fprintf(stderr,
                    "stream id: %s %s\n",
                    gst_stream_get_stream_id(stream),
                    gst_stream_type_get_name(gst_stream_get_stream_type(stream)));
        }
    }

    auto selectStreams = [&](std::initializer_list<int> indices) {
        GList *selectedStreams{};

        std::set<int> set{indices.begin(), indices.end()};

        fprintf(stderr, "selecting streams:\n");

        for (int i = 0; i != gst_stream_collection_get_size(mediaStreams); ++i) {
            if (!set.contains(i))
                continue;
            GstStream *stream = gst_stream_collection_get_stream(mediaStreams, i);
            selectedStreams = g_list_append(selectedStreams,
                                            (gchar *) gst_stream_get_stream_id(stream));
            fprintf(stderr,
                    "    selecting id: %s %s\n",
                    gst_stream_get_stream_id(stream),
                    gst_stream_type_get_name(gst_stream_get_stream_type(stream)));
        }

        GstEvent *evt = gst_event_new_select_streams(selectedStreams);
        gst_element_send_event(pipeline, evt);
    };

    auto selectOddStreams = [&] {
        fprintf(stderr, "selecting odd streams\n");
        selectStreams({1, 3, 5});
    };

    auto selectEvenStreams = [&] {
        fprintf(stderr, "selecting even streams\n");
        selectStreams({0, 2, 4});
    };

    constexpr int scenario = 9;

    switch (scenario) {
    case 0: {
        setStateAndWait(GST_STATE_PLAYING);

        std::this_thread::sleep_for(200ms);
        selectOddStreams();
        std::this_thread::sleep_for(1s);
        selectEvenStreams();
        std::this_thread::sleep_for(2s);

        break;
    }
    case 1: {
        // even -> odd streams
        // audio stream switches, video stream does not

        setStateAndWait(GST_STATE_PLAYING);
        std::this_thread::sleep_for(200ms);
        selectEvenStreams();
        std::this_thread::sleep_for(1s);
        selectOddStreams();
        std::this_thread::sleep_for(2s);

        break;
    }
    case 2: {
        // odd streams: no change (no subtitles)

        setStateAndWait(GST_STATE_PLAYING);
        std::this_thread::sleep_for(200ms);
        selectOddStreams();
        std::this_thread::sleep_for(5s);
        break;
    }
    case 3: {
        // even streams: audio changes, video does not. subtitles are shown

        setStateAndWait(GST_STATE_PLAYING);
        std::this_thread::sleep_for(200ms);
        selectEvenStreams();
        std::this_thread::sleep_for(5s);
        break;
    }
    case 4: {
        // select streams before playing
        // tracks selection not applied after 1s

        std::this_thread::sleep_for(200ms);
        selectEvenStreams();

        setStateAndWait(GST_STATE_PLAYING);

        std::this_thread::sleep_for(5s);
        break;
    }
    case 5: {
        // select streams before playing
        // no subtitles

        std::this_thread::sleep_for(200ms);
        selectOddStreams();

        setStateAndWait(GST_STATE_PLAYING);

        std::this_thread::sleep_for(5s);
        break;
    }
    case 6: {
        // no change while paused.

        std::this_thread::sleep_for(200ms);
        selectEvenStreams();

        std::this_thread::sleep_for(5s);
        break;
    }
    case 7: {
        // deselect any stream
        // gst_event_new_select_streams: assertion 'streams != NULL' failed

        setStateAndWait(GST_STATE_PLAYING);
        std::this_thread::sleep_for(200ms);
        selectStreams({});

        std::this_thread::sleep_for(5s);
        break;
    }
    case 8: {
        // only audio stream:
        // is_selection_done: assertion failed: (stream_is_requested (dbin, slot->active_stream_id))

        setStateAndWait(GST_STATE_PLAYING);
        std::this_thread::sleep_for(200ms);
        selectStreams({1});

        std::this_thread::sleep_for(5s);
        break;
    }
    case 9: {
        // only video stream:
        // is_selection_done: assertion failed: (stream_is_requested (dbin, slot->active_stream_id))

        setStateAndWait(GST_STATE_PLAYING);
        std::this_thread::sleep_for(200ms);
        selectStreams({0});

        std::this_thread::sleep_for(5s);
        break;
    }
    }

    fprintf(stderr, "done\n");
    setStateAndWait(GST_STATE_NULL);

    gst_object_unref(pipeline);
    return 0;
}
