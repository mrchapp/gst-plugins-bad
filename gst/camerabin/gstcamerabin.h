/*
 * GStreamer
 * Copyright (C) 2008 Nokia Corporation <multimedia@maemo.org>
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

#ifndef __GST_CAMERABIN_H__
#define __GST_CAMERABIN_H__

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gstbin.h>
#include <gst/interfaces/photography.h>

#include <gst/camerasrc/gstcamerabin-enum.h>
#include "camerabinimage.h"
#include "camerabinvideo.h"

G_BEGIN_DECLS
/* #defines don't like whitespacey bits */
#define GST_TYPE_CAMERABIN \
  (gst_camerabin_get_type())
#define GST_CAMERABIN(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_CAMERABIN,GstCameraBin))
#define GST_CAMERABIN_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_CAMERABIN,GstCameraBinClass))
#define GST_IS_CAMERABIN(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_CAMERABIN))
#define GST_IS_CAMERABIN_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_CAMERABIN))
typedef struct _GstCameraBin GstCameraBin;
typedef struct _GstCameraBinClass GstCameraBinClass;

/**
 * GstCameraBin:
 *
 * The opaque #GstCameraBin structure.
 */

struct _GstCameraBin
{
  GstPipeline parent;

  /* private */
  GString *filename;
  GstCameraBinMode mode;
  GstCameraBinFlags flags;
  gboolean stop_requested;        /* TRUE if capturing stop needed */
  gboolean paused;                /* TRUE if capturing paused */
  gboolean block_viewfinder;      /* TRUE if viewfinder blocks after capture */

  gboolean video_capture_caps_update;

  /* Image tags are collected here first before sending to imgbin */
  GstTagList *event_tags;

  /* Caps used to create preview image */
  GstCaps *preview_caps;

  /* Caps used to create video preview image */
  GstCaps *video_preview_caps;

  /* concurrency control */
  GMutex *capture_mutex;
  GCond *cond;
  gboolean capturing;
  gboolean eos_handled;

  /* pad names for output and input selectors */
  GstPad *pad_src_view;
  GstPad *pad_src_img;
  GstPad *pad_src_vid;
  GstPad *pad_src_queue;

  /* note:  maybe img_queue moves into srcbin..
   */
  GstElement *img_queue;        /* queue for decoupling capture from
                                   image-postprocessing and saving */


  GstElement *srcbin;           /* bin that holds camera src elements */
  GstElement *imgbin;           /* bin that holds image capturing elements */
  GstElement *vidbin;           /*  bin that holds video capturing elements */
  GstElement *active_bin;       /* image or video bin that is currently in use */
  GstElement *preview_pipeline; /* pipeline for creating preview images */
  GstElement *video_preview_pipeline;   /* pipeline for creating video preview image */

  GstBuffer *video_preview_buffer;      /* buffer for storing video preview */

  /* view finder elements */
  GstElement *aspect_filter;
  GstElement *view_scale;
  GstElement *view_sink;

  /* Application configurable elements */
  GstElement *app_vf_sink;
  GstElement *app_viewfinder_filter;

  /* Cache the photography interface settings */
  GstPhotoSettings photo_settings;

  /* Buffer probe id for captured image handling */
  gulong image_captured_id;
};

/**
 * GstCameraBinClass:
 *
 * The #GstCameraBin class structure.
 */
struct _GstCameraBinClass
{
  GstPipelineClass parent_class;

  /* action signals */

  void (*capture_start) (GstCameraBin * camera);
  void (*capture_stop) (GstCameraBin * camera);
  void (*capture_pause) (GstCameraBin * camera);
  void (*set_video_resolution_fps) (GstCameraBin * camera, gint width,
      gint height, gint fps_n, gint fps_d);
  void (*set_image_resolution) (GstCameraBin * camera, gint width, gint height);

  /* signals (callback) */

    gboolean (*img_done) (GstCameraBin * camera, const gchar * filename);
};

GType gst_camerabin_get_type (void);

G_END_DECLS
#endif /* #ifndef __GST_CAMERABIN_H__ */
