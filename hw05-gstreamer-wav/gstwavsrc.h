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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _GST_WAVSRC_H_
#define _GST_WAVSRC_H_

#include "wav_file.h"
#include <gst/base/gstbasesrc.h>

G_BEGIN_DECLS

#define GST_TYPE_WAVSRC (gst_wavsrc_get_type())
#define GST_WAVSRC(obj)                                                        \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_WAVSRC, GstWavSrc))
#define GST_WAVSRC_CLASS(klass)                                                \
  (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_WAVSRC, GstWavSrcClass))
#define GST_IS_WAVSRC(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_WAVSRC))
#define GST_IS_WAVSRC_CLASS(obj)                                               \
  (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_WAVSRC))

typedef struct _GstWavSrc GstWavSrc;
typedef struct _GstWavSrcClass GstWavSrcClass;

struct _GstWavSrc {
  GstBaseSrc base_wavsrc;
  gchar *filename; /* filename */
  gchar *uri;      /* caching the URI */
  gint fd;         /* open file descriptor */
  guint64 current_pos;
  GstCaps *caps;
  WavFile *wav;
};

struct _GstWavSrcClass {
  GstBaseSrcClass base_wavsrc_class;
};

GType gst_wavsrc_get_type(void);

G_END_DECLS

#endif
