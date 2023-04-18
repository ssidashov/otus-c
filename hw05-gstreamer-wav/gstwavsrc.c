/* GStreamer
 * Copyright (C) 2023 FIXME <fixme@example.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */
/**
 * SECTION:element-gstwavsrc
 *
 * The wavsrc element does WAV Homework stuff.
 *
 * <refsect2>
 * <title>wavsrc location=test.wav</title>
 * |[
 * gst-launch-1.0 -v -m wavsrc location=test.wav !
 * audio/x-raw,format=S16LE,channels=1,rate=48000 ! autoaudiosink
 * ]|
 * Pipeline plays wav file on out device.
 * </refsect2>
 */

#include <glib.h>
#include <gst/audio/audio-format.h>
#include <gst/gst.h>
#include <gst/gstbuffer.h>
#include <gst/gstcaps.h>
#include <gst/gstformat.h>
#include <gst/gstinfo.h>
#include <gst/gstpad.h>
#include <gst/gstquery.h>
#include <stdint.h>
#include <stdlib.h>

#include "wav_file.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstwavsrc.h"
#include <gst/base/gstbasesrc.h>
#include <gst/gst.h>

#include <math.h>
#include <stdio.h>

GST_DEBUG_CATEGORY_STATIC(gst_wavsrc_debug_category);
#define GST_CAT_DEFAULT gst_wavsrc_debug_category

/* prototypes */

static void gst_wavsrc_set_property(GObject *object, guint property_id,
                                    const GValue *value, GParamSpec *pspec);
static void gst_wavsrc_get_property(GObject *object, guint property_id,
                                    GValue *value, GParamSpec *pspec);
static void gst_wavsrc_dispose(GObject *object);
static void gst_wavsrc_finalize(GObject *object);

static GstCaps *gst_wavsrc_get_caps(GstBaseSrc *src, GstCaps *filter);
static gboolean gst_wavsrc_start(GstBaseSrc *src);
static gboolean gst_wavsrc_stop(GstBaseSrc *src);
static gboolean gst_wavsrc_get_size(GstBaseSrc *src, guint64 *size);
static gboolean gst_wavsrc_is_seekable(GstBaseSrc *src);
static gboolean gst_wavsrc_query(GstBaseSrc *src, GstQuery *query);
static GstFlowReturn gst_wavsrc_fill(GstBaseSrc *src, guint64 offset,
                                     guint size, GstBuffer *buf);

static void gst_wavsrc_uri_handler_init(gpointer g_iface, gpointer iface_data);

#define DEFAULT_BLOCKSIZE 4 * 1024

enum { PROP_0, PROP_LOCATION };

/* pad templates */

static GstStaticPadTemplate gst_wavsrc_src_template = GST_STATIC_PAD_TEMPLATE(
    "src", GST_PAD_SRC, GST_PAD_ALWAYS, GST_STATIC_CAPS("audio/x-raw"));

/* class initialization */

#define _do_init                                                               \
  G_IMPLEMENT_INTERFACE(GST_TYPE_URI_HANDLER, gst_wavsrc_uri_handler_init);    \
  GST_DEBUG_CATEGORY_INIT(gst_wavsrc_debug_category, "wavsrc", 0,              \
                          "wavsrc element");

G_DEFINE_TYPE_WITH_CODE(GstWavSrc, gst_wavsrc, GST_TYPE_BASE_SRC, _do_init)

static void gst_wavsrc_class_init(GstWavSrcClass *klass) {
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
  GstBaseSrcClass *base_src_class = GST_BASE_SRC_CLASS(klass);

  /* Setting up pads and setting metadata should be moved to
     base_class_init if you intend to subclass this class. */
  gst_element_class_add_static_pad_template(GST_ELEMENT_CLASS(klass),
                                            &gst_wavsrc_src_template);

  gst_element_class_set_static_metadata(
      GST_ELEMENT_CLASS(klass), "Wav file source", "Generic",
      "Read wav audio from file",
      "Sergey Sidashov <sergey.sidashov@gmail.com>");

  gobject_class->set_property = gst_wavsrc_set_property;
  gobject_class->get_property = gst_wavsrc_get_property;
  gobject_class->dispose = gst_wavsrc_dispose;
  gobject_class->finalize = gst_wavsrc_finalize;
  base_src_class->get_caps = GST_DEBUG_FUNCPTR(gst_wavsrc_get_caps);
  base_src_class->start = GST_DEBUG_FUNCPTR(gst_wavsrc_start);
  base_src_class->stop = GST_DEBUG_FUNCPTR(gst_wavsrc_stop);
  base_src_class->get_size = GST_DEBUG_FUNCPTR(gst_wavsrc_get_size);
  base_src_class->is_seekable = GST_DEBUG_FUNCPTR(gst_wavsrc_is_seekable);
  base_src_class->query = GST_DEBUG_FUNCPTR(gst_wavsrc_query);
  base_src_class->fill = GST_DEBUG_FUNCPTR(gst_wavsrc_fill);

  g_object_class_install_property(
      gobject_class, PROP_LOCATION,
      g_param_spec_string("location", "File Location",
                          "Location of the wav file to read", "",
                          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS |
                              GST_PARAM_MUTABLE_READY));
}

static void gst_wavsrc_init(GstWavSrc *wavsrc) {
  wavsrc->filename = NULL;
  wavsrc->uri = NULL;
  wavsrc->fd = 0;
  wavsrc->caps = NULL;
  wavsrc->duration = 0;
  gst_base_src_set_blocksize(GST_BASE_SRC(wavsrc), DEFAULT_BLOCKSIZE);
}

static gboolean gst_wavsrc_set_location(GstWavSrc *src, const gchar *location) {
  GstState state;

  /* the element must be stopped in order to do this */
  GST_OBJECT_LOCK(src);
  state = GST_STATE(src);
  if (state != GST_STATE_READY && state != GST_STATE_NULL)
    goto wrong_state;
  GST_OBJECT_UNLOCK(src);

  g_free(src->filename);
  g_free(src->uri);

  /* clear the filename if we get a NULL */
  if (location == NULL) {
    src->filename = NULL;
    src->uri = NULL;
  } else {
    /* we store the filename as received by the application. On Windows this
     * should be UTF8 */
    src->filename = g_strdup(location);
    src->uri = gst_filename_to_uri(location, NULL);
    GST_INFO("filename : %s", src->filename);
    GST_INFO("uri      : %s", src->uri);
  }
  g_object_notify(G_OBJECT(src), "location");

  return TRUE;

  /* ERROR */
wrong_state : {
  g_warning("Changing the `location' property on wavsrc when a file is "
            "open is not supported.");
  GST_OBJECT_UNLOCK(src);
  return FALSE;
}
}

static gboolean gst_wavsrc_calculate_duration(GstWavSrc *wav) {
  if (wav->wav == NULL) {
    return FALSE;
  }
  if (wav->duration > 0) {
    return TRUE;
  }
  gsize sample_rate_bps = wav_file_get_sample_rate(wav->wav);

  if (wav_file_get_sample_rate(wav->wav) > 0) {
    gsize length_samples = wav_file_get_length(wav->wav);
    GST_INFO_OBJECT(wav, "Got datasize %" G_GUINT64_FORMAT, length_samples);
    wav->duration =
        gst_util_uint64_scale_ceil(length_samples, GST_SECOND, sample_rate_bps);
    GST_INFO_OBJECT(wav, "Got duration (bps) %" GST_TIME_FORMAT,
                    GST_TIME_ARGS(wav->duration));
    return TRUE;
  }
  return FALSE;
}

void gst_wavsrc_set_property(GObject *object, guint property_id,
                             const GValue *value, GParamSpec *pspec) {
  GstWavSrc *wavsrc = GST_WAVSRC(object);

  GST_DEBUG_OBJECT(wavsrc, "set_property");

  switch (property_id) {
  case PROP_LOCATION:
    gst_wavsrc_set_location(wavsrc, g_value_get_string(value));
    break;

  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    break;
  }
}

void gst_wavsrc_get_property(GObject *object, guint property_id, GValue *value,
                             GParamSpec *pspec) {
  GstWavSrc *wavsrc = GST_WAVSRC(object);

  GST_DEBUG_OBJECT(wavsrc, "get_property");

  switch (property_id) {
  case PROP_LOCATION:
    g_value_set_string(value, wavsrc->filename);
    break;
  default:
    G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    break;
  }
}

void gst_wavsrc_dispose(GObject *object) {
  GstWavSrc *wavsrc = GST_WAVSRC(object);

  GST_DEBUG_OBJECT(wavsrc, "dispose");

  /* clean up as possible.  may be called multiple times */

  G_OBJECT_CLASS(gst_wavsrc_parent_class)->dispose(object);
}

void gst_wavsrc_finalize(GObject *object) {
  GstWavSrc *wavsrc = GST_WAVSRC(object);

  GST_DEBUG_OBJECT(wavsrc, "finalize");

  g_free(wavsrc->filename);
  g_free(wavsrc->uri);

  /* clean up object here */

  G_OBJECT_CLASS(gst_wavsrc_parent_class)->finalize(object);
}

/* get caps from subclass */
static GstCaps *gst_wavsrc_get_caps(GstBaseSrc *src, GstCaps *filter) {
  GstWavSrc *wavsrc = GST_WAVSRC(src);
  GstBaseSrcClass *bclass = GST_BASE_SRC_GET_CLASS(wavsrc);

  GST_DEBUG_OBJECT(wavsrc, "get_caps");

  GstCaps *current_caps = NULL;
  if (wavsrc->caps != NULL) {
    current_caps = wavsrc->caps;
  } else {
    GstPadTemplate *pad_template =
        gst_element_class_get_pad_template(GST_ELEMENT_CLASS(bclass), "src");
    if (pad_template != NULL) {
      current_caps = gst_pad_template_get_caps(pad_template);
    }
  }
  GstCaps *caps = current_caps;
  if (filter && current_caps != NULL) {
    GstCaps *intersection;
    intersection =
        gst_caps_intersect_full(filter, current_caps, GST_CAPS_INTERSECT_FIRST);
    caps = intersection;
    if (current_caps != NULL && current_caps != wavsrc->caps) {
      // gst_caps_unref(current_caps);
    }
  }

  return caps;
}

GstCaps *gst_wav_create_audio_caps(GstWavSrc *wavSrc) {
  GstCaps *caps = NULL;

  uint16_t format = wav_file_get_format(wavSrc->wav);
  uint16_t bits_per_sample = wav_file_get_sample_size(wavSrc->wav) * 8;
  uint16_t channel_count = wav_file_get_channel_count(wavSrc->wav);
  uint32_t sample_rate = wav_file_get_sample_rate(wavSrc->wav);
  uint16_t block_align = wav_file_get_block_align(wavSrc->wav);

  if (format == WAV_FORMAT_PCM) { // ONLY PCM SUPPORTED
    gint ba = block_align;
    gint ch = channel_count;
    gint wd, ws;
    GstAudioFormat format;

    if (ba > (32 / 8) * ch) {
      GST_WARNING("Invalid block align: %d > %d", ba, (32 / 8) * ch);
      wd = GST_ROUND_UP_8(bits_per_sample);
    } else if (ba != 0) {
      wd = ba * 8 / ch;
    } else {
      wd = GST_ROUND_UP_8(bits_per_sample);
    }

    if (bits_per_sample > 32) {
      GST_WARNING("invalid depth (%d) of pcm audio, overwriting.",
                  bits_per_sample);
      bits_per_sample = wd;
    }

    ws = wd;

    format = gst_audio_format_build_integer(wd != 8, G_LITTLE_ENDIAN, wd, ws);
    if (format == GST_AUDIO_FORMAT_UNKNOWN) {
      GST_WARNING("Unsupported raw audio format with width %d", wd);
      return NULL;
    }

    caps = gst_caps_new_simple("audio/x-raw", "format", G_TYPE_STRING,
                               gst_audio_format_to_string(format), "layout",
                               G_TYPE_STRING, "interleaved", "channels",
                               G_TYPE_INT, ch, NULL);

  } else {
    GST_WARNING("Unknown audio tag 0x%04x", format);
    return NULL;
  }

  gst_caps_set_simple(caps, "rate", G_TYPE_INT, sample_rate, "channels",
                      G_TYPE_INT, channel_count, NULL);
  return caps;
}

/* start and stop processing, ideal for opening/closing the resource */
static gboolean gst_wavsrc_start(GstBaseSrc *src) {
  GstWavSrc *wavsrc = GST_WAVSRC(src);

  GST_INFO_OBJECT(wavsrc, "STARTING WavSrc");

  WavFile *wav = NULL;
  int ret = wav_file_new(wavsrc->filename, &wav);
  if (ret != 0) {
    switch (ret) {
    case WAV_FILE_ERRCODE_WRONG_FILENAME:
      GST_ELEMENT_ERROR(wavsrc, RESOURCE, NOT_FOUND, (NULL),
                        ("No such file \"%s\"", wavsrc->filename));
      break;
    case WAV_FILE_ERRCODE_FILE_FORMAT_ERROR:
      GST_ELEMENT_ERROR(wavsrc, STREAM, FAILED, (NULL), ("Wav format error"));
      break;
    case WAV_FILE_ERRCODE_FILE_OPERATION_ERROR:
      GST_ELEMENT_ERROR(wavsrc, RESOURCE, FAILED, (NULL),
                        ("File operation error"));
      break;
    }
    return FALSE;
  }

  GST_INFO_OBJECT(wavsrc,
                  "Wav file read, format: %u, bitrate: %d, channels: %u",
                  wav_file_get_format(wav), wav_file_get_sample_rate(wav),
                  wav_file_get_channel_count(wav));

  wavsrc->wav = wav;

  GstCaps *caps = gst_wav_create_audio_caps(wavsrc);
  if (caps == NULL) {
    return FALSE;
  }

  wavsrc->caps = gst_caps_ref(caps);

  GST_DEBUG_OBJECT(wavsrc, "start");

  return TRUE;
}

static gboolean gst_wavsrc_stop(GstBaseSrc *src) {
  GstWavSrc *wavsrc = GST_WAVSRC(src);

  wav_file_close(wavsrc->wav);
  g_object_unref(wavsrc->wav);
  wavsrc->wav = NULL;
  gst_caps_unref(wavsrc->caps);

  GST_DEBUG_OBJECT(wavsrc, "stop");

  return TRUE;
}

/* get the total size of the resource in bytes */
static gboolean gst_wavsrc_get_size(GstBaseSrc *src, guint64 *size) {
  GstWavSrc *wavsrc = GST_WAVSRC(src);

  if (wavsrc->wav == NULL) {
    return FALSE;
  }

  GST_DEBUG_OBJECT(wavsrc, "get_size");

  gsize the_size = wav_file_get_length(wavsrc->wav); // in samples
  gsize sample_size = wav_file_get_sample_size(wavsrc->wav);
  *size = (guint64)(the_size * sample_size);

  return TRUE;
}

/* check if the resource is seekable */
static gboolean gst_wavsrc_is_seekable(GstBaseSrc *src) {
  GstWavSrc *wavsrc = GST_WAVSRC(src);

  GST_DEBUG_OBJECT(wavsrc, "is_seekable");

  return FALSE;
}

/* notify subclasses of a query */
static gboolean gst_wavsrc_query(GstBaseSrc *src, GstQuery *query) {
  GstWavSrc *wavsrc = GST_WAVSRC(src);

  gboolean ret = FALSE;

  GST_DEBUG_OBJECT(wavsrc, "query %s",
                   gst_query_type_get_name(GST_QUERY_TYPE(query)));
  switch (GST_QUERY_TYPE(query)) {
  case GST_QUERY_URI:
    gst_query_set_uri(query, wavsrc->uri);
    ret = TRUE;
    break;
  case GST_QUERY_DURATION: {
    gint64 duration = 0;
    GstFormat format;

    gst_query_parse_duration(query, &format, NULL);

    switch (format) {
    case GST_FORMAT_BYTES: {
      format = GST_FORMAT_BYTES;
      duration = wav_file_get_length(wavsrc->wav);
      break;
    }
    case GST_FORMAT_TIME:
      if ((ret = gst_wavsrc_calculate_duration(wavsrc))) {
        duration = wavsrc->duration;
      }
      break;
    default:
      ret = FALSE;
      break;
    }
    if (ret)
      gst_query_set_duration(query, format, duration);
    break;
  } break;
  default:
    ret = FALSE;
    break;
  };

  if (!ret)
    ret = GST_BASE_SRC_CLASS(gst_wavsrc_parent_class)->query(src, query);

  return ret;

  GST_DEBUG_OBJECT(wavsrc, "query");

  return TRUE;
}

/* ask the subclass to fill the buffer with data from offset and size */
static GstFlowReturn gst_wavsrc_fill(GstBaseSrc *src, guint64 offset,
                                     guint size, GstBuffer *buf) {
  GstWavSrc *wavsrc = GST_WAVSRC(src);
  GST_DEBUG_OBJECT(wavsrc, "fill");
  GstMapInfo info;
  guint8 *data;
  GST_DEBUG_OBJECT(wavsrc, "Fill, size: %u, offset: %lu", size, offset);
  if (!gst_buffer_map(buf, &info, GST_MAP_WRITE)) {
    GST_ERROR_OBJECT(wavsrc, "Cannot map GST buffer");
    return GST_FLOW_ERROR;
  }
  data = info.data;
  gsize sample_size = wav_file_get_sample_size(wavsrc->wav);
  gssize read_size =
      wav_file_read_samples(wavsrc->wav, data, size / sample_size);
  gst_buffer_unmap(buf, &info);
  if (read_size < 0) {
    GST_ELEMENT_ERROR(wavsrc, RESOURCE, READ, ("Error reading wav samples"),
                      GST_ERROR_SYSTEM);
    return GST_FLOW_ERROR;
  }

  GST_BUFFER_OFFSET(buf) = offset;
  GST_BUFFER_OFFSET_END(buf) = offset + read_size;

  if (wav_file_eof(wavsrc->wav)) {
    return GST_FLOW_EOS;
  }

  return GST_FLOW_OK;
}

/*** GSTURIHANDLER INTERFACE *************************************************/

static GstURIType gst_wavsrc_uri_get_type(__attribute__((unused)) GType type) {
  return GST_URI_SRC;
}

static const gchar *const *gst_wavsrc_uri_get_protocols(__attribute__((unused))
                                                        GType type) {
  static const gchar *protocols[] = {"file", NULL};

  return protocols;
}

static gchar *gst_wavsrc_uri_get_uri(GstURIHandler *handler) {
  GstWavSrc *src = GST_WAVSRC(handler);

  return g_strdup(src->uri);
}

static gboolean gst_wavsrc_uri_set_uri(GstURIHandler *handler, const gchar *uri,
                                       GError **err) {
  gchar *location, *hostname = NULL;
  gboolean ret = FALSE;
  GstWavSrc *src = GST_WAVSRC(handler);

  if (strcmp(uri, "file://") == 0) {
    gst_wavsrc_set_location(src, NULL);
    return TRUE;
  }

  location = g_filename_from_uri(uri, &hostname, err);

  if (!location || (err != NULL && *err != NULL)) {
    GST_WARNING_OBJECT(src, "Invalid URI '%s' for filesrc: %s", uri,
                       (err != NULL && *err != NULL) ? (*err)->message
                                                     : "unknown error");
    goto beach;
  }

  if ((hostname) && (strcmp(hostname, "localhost"))) {
    /* Only 'localhost' is permitted */
    GST_WARNING_OBJECT(src, "Invalid hostname '%s' for filesrc", hostname);
    g_set_error(err, GST_URI_ERROR, GST_URI_ERROR_BAD_URI,
                "File URI with invalid hostname '%s'", hostname);
    goto beach;
  }
  ret = gst_wavsrc_set_location(src, location);

beach:
  if (location)
    g_free(location);
  if (hostname)
    g_free(hostname);

  return ret;
}

static void gst_wavsrc_uri_handler_init(gpointer g_iface,
                                        __attribute__((unused))
                                        gpointer iface_data) {
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *)g_iface;

  iface->get_type = gst_wavsrc_uri_get_type;
  iface->get_protocols = gst_wavsrc_uri_get_protocols;
  iface->get_uri = gst_wavsrc_uri_get_uri;
  iface->set_uri = gst_wavsrc_uri_set_uri;
}

static gboolean plugin_init(GstPlugin *plugin) {

  /* Remember to set the rank if it's an element that is meant
     to be autoplugged by decodebin. */
  return gst_element_register(plugin, "wavsrc", GST_RANK_NONE, GST_TYPE_WAVSRC);
}

#ifndef VERSION
#define VERSION "0.0.1"
#endif
#ifndef PACKAGE
#define PACKAGE "wavsrc"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "wavsrc"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "https://github.com/ssidashov/otus-c"
#endif

GST_PLUGIN_DEFINE(GST_VERSION_MAJOR, GST_VERSION_MINOR, wavsrc,
                  "Read wav audio from file", plugin_init, VERSION, "LGPL",
                  PACKAGE_NAME, GST_PACKAGE_ORIGIN)
