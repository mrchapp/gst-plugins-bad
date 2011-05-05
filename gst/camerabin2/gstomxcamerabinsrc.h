/*
 * GStreamer
 * Copyright (C) 2011 Texas Instruments, Inc
 * Author: Alessandro Decina <alessandro.decina@collabora.co.uk>
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */


#ifndef __GST_OMX_CAMERA_BIN_SRC_H__
#define __GST_OMX_CAMERA_BIN_SRC_H__

#include <gst/gst.h>
#include <gst/basecamerabinsrc/gstbasecamerasrc.h>
#include <gst/basecamerabinsrc/gstcamerabinpreview.h>
#include "camerabingeneral.h"

G_BEGIN_DECLS
#define GST_TYPE_OMX_CAMERA_BIN_SRC \
  (gst_omx_camera_bin_src_get_type())
#define GST_OMX_CAMERA_BIN_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_OMX_CAMERA_BIN_SRC,GstOmxCameraBinSrc))
#define GST_OMX_CAMERA_BIN_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_OMX_CAMERA_BIN_SRC,GstOmxCameraBinSrcClass))
#define GST_IS_OMX_CAMERA_BIN_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_OMX_CAMERA_BIN_SRC))
#define GST_IS_OMX_CAMERA_BIN_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_OMX_CAMERA_BIN_SRC))
    GType gst_omx_camera_bin_src_get_type (void);

typedef struct _GstOmxCameraBinSrc GstOmxCameraBinSrc;
typedef struct _GstOmxCameraBinSrcClass GstOmxCameraBinSrcClass;


/**
 * GstOmxCameraBinSrc:
 *
 */
struct _GstOmxCameraBinSrc
{
  GstBaseCameraSrc parent;

  GstCameraBinMode mode;

  GstPad *vfsrc;
  GstPad *imgsrc;
  GstPad *vidsrc;

  /* video recording controls */
  gint video_rec_status;

  /* image capture controls */
  gint image_capture_count;

  /* source elements */
  GstElement *video_source;
  GstElement *src_filter;

  GstElement *vfsrc_stride;
  GstElement *vidsrc_stride;
  GstElement *imgsrc_stride;

  gboolean elements_created;

  guint src_event_probe_id;

  GstPadEventFunction srcpad_event_func;

  /* For changing caps without losing timestamps */
  gboolean drop_newseg;

  /* Caps that videosrc supports */
  GstCaps *allowed_caps;

  /* Caps applied to capsfilters when taking still image */
  GstCaps *image_capture_caps;
  gboolean image_renegotiate;
  gboolean video_renegotiate;
};


/**
 * GstOmxCameraBinSrcClass:
 *
 */
struct _GstOmxCameraBinSrcClass
{
  GstBaseCameraSrcClass parent;
};

gboolean gst_omx_camera_bin_src_plugin_init (GstPlugin * plugin);

#endif /* __GST_OMX_CAMERA_BIN_SRC_H__ */
