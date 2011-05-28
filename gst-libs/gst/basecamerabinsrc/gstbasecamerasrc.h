/*
 * GStreamer
 * Copyright (C) 2010 Texas Instruments, Inc
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


#ifndef __GST_BASE_CAMERA_SRC_H__
#define __GST_BASE_CAMERA_SRC_H__

#include <gst/gst.h>
#include <gst/gstbin.h>
#include <gst/interfaces/photography.h>
#include <gst/interfaces/colorbalance.h>
#include "gstcamerabin-enum.h"
#include "gstcamerabinpreview.h"

G_BEGIN_DECLS
#define GST_TYPE_BASE_CAMERA_SRC \
  (gst_base_camera_src_get_type())
#define GST_BASE_CAMERA_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_BASE_CAMERA_SRC,GstBaseCameraBinSrc))
#define GST_BASE_CAMERA_SRC_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_BASE_CAMERA_SRC, GstBaseCameraBinSrcClass))
#define GST_BASE_CAMERA_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_BASE_CAMERA_SRC,GstBaseCameraBinSrcClass))
#define GST_IS_BASE_CAMERA_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_BASE_CAMERA_SRC))
#define GST_IS_BASE_CAMERA_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_BASE_CAMERA_SRC))
#define GST_BASE_CAMERA_SRC_CAST(obj) \
  ((GstBaseCameraBinSrc *) (obj))
GType gst_base_camera_src_get_type (void);

typedef struct _GstBaseCameraBinSrc GstBaseCameraBinSrc;
typedef struct _GstBaseCameraBinSrcClass GstBaseCameraBinSrcClass;

#define GST_BASE_CAMERA_SRC_VIEWFINDER_PAD_NAME "vfsrc"
#define GST_BASE_CAMERA_SRC_IMAGE_PAD_NAME "imgsrc"
#define GST_BASE_CAMERA_SRC_VIDEO_PAD_NAME "vidsrc"

#define GST_BASE_CAMERA_SRC_PREVIEW_MESSAGE_NAME "preview-image"

/**
 * GstBaseCameraBinSrc:
 */
struct _GstBaseCameraBinSrc
{
  GstBin parent;

  GstCameraBinMode mode;

  gboolean capturing;
  GMutex *capturing_mutex;

  /* Preview convert pipeline */
  GstCaps *preview_caps;
  gboolean post_preview;
  GstElement *preview_filter;
  GstCameraBinPreviewPipelineData *preview_pipeline;
  gboolean preview_filter_changed;

  /* Resolution of the buffers configured to camerabin */
  gint width;
  gint height;

  gfloat zoom;
  gfloat max_zoom;

  gpointer _gst_reserved[GST_PADDING_LARGE];
};


/**
 * GstBaseCameraBinSrcClass:
 * @construct_pipeline: construct pipeline must be implemented by derived class
 * @setup_pipeline: configure pipeline for the chosen settings
 * @set_zoom: set the zoom
 * @set_mode: set the mode
 */
struct _GstBaseCameraBinSrcClass
{
  GstBinClass parent;

  /* construct pipeline must be implemented by derived class */
  gboolean    (*construct_pipeline)  (GstBaseCameraBinSrc *self);

  /* optional */
  gboolean    (*setup_pipeline)      (GstBaseCameraBinSrc *self);

  /* set the zoom */
  void        (*set_zoom)            (GstBaseCameraBinSrc *self, gfloat zoom);

  /* set the mode */
  gboolean    (*set_mode)            (GstBaseCameraBinSrc *self,
                                      GstCameraBinMode mode);

  /* set preview caps */
  gboolean    (*set_preview)         (GstBaseCameraBinSrc *self,
                                      GstCaps *preview_caps);

  /* */
  GstCaps *   (*get_allowed_input_caps) (GstBaseCameraBinSrc * self);

  void (*private_start_capture) (GstBaseCameraBinSrc * src);
  void (*private_stop_capture) (GstBaseCameraBinSrc * src);
  gboolean (*start_capture) (GstBaseCameraBinSrc * src);
  void (*stop_capture) (GstBaseCameraBinSrc * src);

  gpointer _gst_reserved[GST_PADDING_LARGE];
};


#define MIN_ZOOM 1.0f
#define MAX_ZOOM 10.0f
#define ZOOM_1X MIN_ZOOM

GstPhotography * gst_base_camera_src_get_photography (GstBaseCameraBinSrc *self);
GstColorBalance * gst_base_camera_src_get_color_balance (GstBaseCameraBinSrc *self);

gboolean gst_base_camera_src_set_mode (GstBaseCameraBinSrc *self, GstCameraBinMode mode);
void gst_base_camera_src_setup_zoom (GstBaseCameraBinSrc * self);
void gst_base_camera_src_setup_preview (GstBaseCameraBinSrc * self, GstCaps * preview_caps);
GstCaps * gst_base_camera_src_get_allowed_input_caps (GstBaseCameraBinSrc * self);
void gst_base_camera_src_finish_capture (GstBaseCameraBinSrc *self);


void gst_base_camera_src_post_preview (GstBaseCameraBinSrc *self, GstBuffer * buf);
// XXX add methods to get/set img capture and vid capture caps..

#endif /* __GST_BASE_CAMERA_SRC_H__ */
