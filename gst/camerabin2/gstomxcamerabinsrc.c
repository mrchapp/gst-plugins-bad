/*
 * GStreamer
 * Copyright (C) 2011 Texas Instruments, Inc
 * Copyright (C) 2011 Collabora Ltd
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


#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include "gstomxcamerabinsrc.h"
#include "camerabingeneral.h"

/* FIXME: get rid of this */
enum
{
  OMX_CAMERA_MODE_PREVIEW = 0,
  OMX_CAMERA_MODE_VIDEO = 1,
  OMX_CAMERA_MODE_VIDEO_IMAGE = 2,
  OMX_CAMERA_MODE_IMAGE = 3,
  OMX_CAMERA_MODE_IMAGE_HS = 4,
};

enum GstVideoRecordingStatus
{
  GST_VIDEO_RECORDING_STATUS_DONE,
  GST_VIDEO_RECORDING_STATUS_STARTING,
  GST_VIDEO_RECORDING_STATUS_RUNNING,
  GST_VIDEO_RECORDING_STATUS_FINISHING
};


enum
{
  PROP_0,
};

GST_DEBUG_CATEGORY (omx_camera_bin_src_debug);
#define GST_CAT_DEFAULT omx_camera_bin_src_debug

GST_BOILERPLATE (GstOmxCameraBinSrc, gst_omx_camera_bin_src,
    GstBaseCameraBinSrc, GST_TYPE_BASE_CAMERA_SRC);

static void
gst_omx_camera_bin_src_dispose (GObject * object)
{
  GstOmxCameraBinSrc *self = GST_OMX_CAMERA_BIN_SRC (object);

  gst_caps_replace (&self->image_capture_caps, NULL);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_omx_camera_bin_src_finalize (GstOmxCameraBinSrc * self)
{
  G_OBJECT_CLASS (parent_class)->finalize ((GObject *) (self));
}

static void
gst_omx_camera_bin_src_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec)
{
  GstOmxCameraBinSrc *self = GST_OMX_CAMERA_BIN_SRC (object);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
      break;
  }
}

static void
gst_omx_camera_bin_src_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec)
{
  GstOmxCameraBinSrc *self = GST_OMX_CAMERA_BIN_SRC (object);

  switch (prop_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (self, prop_id, pspec);
      break;
  }
}


/**
 * gst_omx_camera_bin_src_imgsrc_probe:
 *
 * Buffer probe called before sending each buffer to image queue.
 */
static gboolean
gst_omx_camera_bin_src_imgsrc_probe (GstPad * pad, GstBuffer * buffer,
    gpointer data)
{
  GstOmxCameraBinSrc *self = GST_OMX_CAMERA_BIN_SRC (data);
  GstBaseCameraBinSrc *camerasrc = GST_BASE_CAMERA_SRC (data);
  gboolean ret = FALSE;

  GST_LOG_OBJECT (self, "Image probe, mode %d, capture count %d",
      camerasrc->mode, self->image_capture_count);

  g_mutex_lock (camerasrc->capturing_mutex);
  if (self->image_capture_count > 0) {
    ret = TRUE;
    self->image_capture_count--;

    /* post preview */
    /* TODO This can likely be optimized if the viewfinder caps is the same as
     * the preview caps, avoiding another scaling of the same buffer. */
    GST_DEBUG_OBJECT (self, "Posting preview for image");
    gst_base_camera_src_post_preview (camerasrc, buffer);

    if (self->image_capture_count == 0) {
      gst_base_camera_src_finish_capture (camerasrc);
    }
  }
  g_mutex_unlock (camerasrc->capturing_mutex);
  return ret;
}

/**
 * gst_omx_camera_bin_src_vidsrc_probe:
 *
 * Buffer probe called before sending each buffer to image queue.
 */
static gboolean
gst_omx_camera_bin_src_vidsrc_probe (GstPad * pad, GstBuffer * buffer,
    gpointer data)
{
  GstOmxCameraBinSrc *self = GST_OMX_CAMERA_BIN_SRC (data);
  GstBaseCameraBinSrc *camerasrc = GST_BASE_CAMERA_SRC_CAST (self);
  gboolean ret = FALSE;

  GST_LOG_OBJECT (self, "Video probe, mode %d, capture status %d",
      camerasrc->mode, self->video_rec_status);

  /* TODO do we want to lock for every buffer? */
  /*
   * Note that we can use gst_pad_push_event here because we are a buffer
   * probe.
   */
  /* TODO shouldn't access this directly */
  g_mutex_lock (camerasrc->capturing_mutex);
  if (self->video_rec_status == GST_VIDEO_RECORDING_STATUS_DONE) {
    /* NOP */
  } else if (self->video_rec_status == GST_VIDEO_RECORDING_STATUS_STARTING) {
    GST_DEBUG_OBJECT (self, "Starting video recording");
    self->video_rec_status = GST_VIDEO_RECORDING_STATUS_RUNNING;

    /* post preview */
    GST_DEBUG_OBJECT (self, "Posting preview for video");
    gst_base_camera_src_post_preview (camerasrc, buffer);

    ret = TRUE;
  } else if (self->video_rec_status == GST_VIDEO_RECORDING_STATUS_FINISHING) {
    /* send eos */
    GST_DEBUG_OBJECT (self, "Finishing video recording, pushing eos");
    gst_pad_push_event (pad, gst_event_new_eos ());
    self->video_rec_status = GST_VIDEO_RECORDING_STATUS_DONE;
    gst_base_camera_src_finish_capture (camerasrc);
  } else {
    ret = TRUE;
  }
  g_mutex_unlock (camerasrc->capturing_mutex);
  return ret;
}


static gboolean
gst_omx_camera_bin_src_construct_pipeline (GstBaseCameraBinSrc * bcamsrc)
{
  GstOmxCameraBinSrc *self = GST_OMX_CAMERA_BIN_SRC (bcamsrc);
  GstBin *cbin = GST_BIN (bcamsrc);
  gboolean ret = FALSE;
  GstPad *pad;
  GstCaps *caps;

  if (!self->elements_created) {
    GST_DEBUG_OBJECT (self, "constructing pipeline");

    self->video_source = gst_element_factory_make ("omx_camera", NULL);
    if (self->video_source == NULL)
      goto done;

    if (bcamsrc->mode == MODE_IMAGE)
      g_object_set (self->video_source, "mode",
          OMX_CAMERA_MODE_VIDEO_IMAGE, NULL);
    else
      g_object_set (self->video_source, "mode", OMX_CAMERA_MODE_VIDEO, NULL);

    self->vfsrc_filter = gst_element_factory_make ("capsfilter",
        "vfsrc-capsfilter");
    caps = gst_caps_from_string ("video/x-raw-yuv-strided");
    g_object_set (self->vfsrc_filter, "caps", caps, NULL);
    gst_caps_unref (caps);

    self->vidsrc_filter = gst_element_factory_make ("capsfilter",
        "vidsrc-capsfilter");
    caps = gst_caps_from_string ("video/x-raw-yuv-strided");
    g_object_set (self->vidsrc_filter, "caps", caps, NULL);
    gst_caps_unref (caps);

    self->vfsrc_stride =
        gst_element_factory_make ("stridetransform", "vfsrc-stride");
    self->vidsrc_stride =
        gst_element_factory_make ("stridetransform", "vidsrc-stride");
    self->imgsrc_stride =
        gst_element_factory_make ("identity", "imgsrc-stride");

    gst_bin_add_many (cbin, self->video_source, self->vfsrc_filter,
        self->vfsrc_stride, self->vidsrc_filter, self->vidsrc_stride,
        self->imgsrc_stride, NULL);

    if (!gst_element_link_pads (self->video_source, "src",
            self->vfsrc_filter, "sink"))
      goto link_error;

    if (!gst_element_link (self->vfsrc_filter, self->vfsrc_stride))
      goto link_error;

    if (!gst_element_link_pads (self->video_source, "vidsrc",
            self->vidsrc_filter, "sink"))
      goto link_error;

    if (!gst_element_link (self->vidsrc_filter, self->vidsrc_stride))
      goto link_error;

    if (!gst_element_link_pads (self->video_source, "imgsrc",
            self->imgsrc_stride, "sink"))
      goto link_error;

    pad = gst_element_get_static_pad (self->video_source, "vidsrc");
    gst_pad_add_buffer_probe (pad,
        G_CALLBACK (gst_omx_camera_bin_src_vidsrc_probe), self);
    gst_object_unref (pad);

    pad = gst_element_get_static_pad (self->video_source, "imgsrc");
    gst_pad_add_buffer_probe (pad,
        G_CALLBACK (gst_omx_camera_bin_src_imgsrc_probe), self);
    gst_object_unref (pad);

    pad = gst_element_get_static_pad (self->vfsrc_stride, "src");
    gst_ghost_pad_set_target (GST_GHOST_PAD (self->vfsrc), pad);
    gst_object_unref (pad);

    pad = gst_element_get_static_pad (self->vidsrc_stride, "src");
    gst_ghost_pad_set_target (GST_GHOST_PAD (self->vidsrc), pad);
    gst_object_unref (pad);

    pad = gst_element_get_static_pad (self->imgsrc_stride, "src");
    gst_ghost_pad_set_target (GST_GHOST_PAD (self->imgsrc), pad);
    gst_object_unref (pad);
  }

  ret = TRUE;
  self->elements_created = TRUE;

done:
  return ret;

link_error:
  GST_ERROR_OBJECT (self, "failed to link elements");
  return FALSE;
}

static gboolean
gst_omx_camera_bin_src_set_mode (GstBaseCameraBinSrc * bcamsrc,
    GstCameraBinMode mode)
{
  GstOmxCameraBinSrc *self = GST_OMX_CAMERA_BIN_SRC (bcamsrc);

  GST_INFO_OBJECT (self, "mode %d", (gint) mode);

  if (self->video_source) {
    if (mode == MODE_IMAGE) {
      g_object_set (self->video_source, "mode", OMX_CAMERA_MODE_VIDEO_IMAGE,
          NULL);
    } else {
      g_object_set (self->video_source, "mode", OMX_CAMERA_MODE_VIDEO, NULL);
    }
  }
  self->mode = mode;

  return TRUE;
}

static void
gst_omx_camera_bin_src_set_zoom (GstBaseCameraBinSrc * bcamsrc, gfloat zoom)
{
  GstOmxCameraBinSrc *self = GST_OMX_CAMERA_BIN_SRC (bcamsrc);
  gint omx_zoom;

  GST_INFO_OBJECT (self, "setting zoom %f", zoom);

  omx_zoom = zoom * 80;
  if (omx_zoom < 100)
    omx_zoom = 100;

  g_object_set (G_OBJECT (self->video_source), "zoom", omx_zoom, NULL);
}

static GstCaps *
gst_omx_camera_bin_src_get_allowed_input_caps (GstBaseCameraBinSrc * bcamsrc)
{
  GstOmxCameraBinSrc *self = GST_OMX_CAMERA_BIN_SRC (bcamsrc);
  GstCaps *caps = NULL;
  GstPad *pad = NULL, *peer_pad = NULL;
  GstState state;
  GstElement *videosrc;

  videosrc = self->video_source;

  if (!videosrc) {
    GST_WARNING_OBJECT (self, "no videosrc, can't get allowed caps");
    goto failed;
  }

  if (self->allowed_caps) {
    GST_DEBUG_OBJECT (self, "returning cached caps");
    goto done;
  }

  pad = gst_element_get_static_pad (videosrc, "src");

  if (!pad) {
    GST_WARNING_OBJECT (self, "no srcpad in videosrc");
    goto failed;
  }

  state = GST_STATE (videosrc);

  /* Make this function work also in NULL state */
  if (state == GST_STATE_NULL) {
    GST_DEBUG_OBJECT (self, "setting videosrc to ready temporarily");
    peer_pad = gst_pad_get_peer (pad);
    if (peer_pad) {
      gst_pad_unlink (pad, peer_pad);
    }
    /* Set videosrc to READY to open video device */
    gst_element_set_locked_state (videosrc, TRUE);
    gst_element_set_state (videosrc, GST_STATE_READY);
  }

  self->allowed_caps = gst_pad_get_caps (pad);

  /* Restore state and re-link if necessary */
  if (state == GST_STATE_NULL) {
    GST_DEBUG_OBJECT (self, "restoring videosrc state %d", state);
    /* Reset videosrc to NULL state, some drivers seem to need this */
    gst_element_set_state (videosrc, GST_STATE_NULL);
    if (peer_pad) {
      gst_pad_link (pad, peer_pad);
      gst_object_unref (peer_pad);
    }
    gst_element_set_locked_state (videosrc, FALSE);
  }

  gst_object_unref (pad);

done:
  if (self->allowed_caps) {
    caps = gst_caps_copy (self->allowed_caps);
  }
  GST_DEBUG_OBJECT (self, "allowed caps:%" GST_PTR_FORMAT, caps);
failed:
  return caps;
}

static gboolean
gst_omx_camera_bin_src_start_capture (GstBaseCameraBinSrc * camerasrc)
{
  GstOmxCameraBinSrc *src = GST_OMX_CAMERA_BIN_SRC (camerasrc);

  if (src->mode == MODE_IMAGE) {
    src->image_capture_count = 1;
  } else if (src->mode == MODE_VIDEO) {
    if (src->video_rec_status == GST_VIDEO_RECORDING_STATUS_DONE) {
      src->video_rec_status = GST_VIDEO_RECORDING_STATUS_STARTING;
    }
  } else {
    g_assert_not_reached ();
    return FALSE;
  }
  return TRUE;
}

static void
gst_omx_camera_bin_src_stop_capture (GstBaseCameraBinSrc * camerasrc)
{
  GstOmxCameraBinSrc *src = GST_OMX_CAMERA_BIN_SRC (camerasrc);

  /* TODO shoud we access this directly? Maybe a macro is better? */
  if (src->mode == MODE_VIDEO) {
    if (src->video_rec_status == GST_VIDEO_RECORDING_STATUS_STARTING) {
      GST_DEBUG_OBJECT (src, "Aborting, had not started recording");
      src->video_rec_status = GST_VIDEO_RECORDING_STATUS_DONE;

    } else if (src->video_rec_status == GST_VIDEO_RECORDING_STATUS_RUNNING) {
      GST_DEBUG_OBJECT (src, "Marking video recording as finishing");
      src->video_rec_status = GST_VIDEO_RECORDING_STATUS_FINISHING;
    }
  } else {
    src->image_capture_count = 0;
  }
}

static GstStateChangeReturn
gst_omx_camera_bin_src_change_state (GstElement * element, GstStateChange trans)
{
  GstStateChangeReturn ret = GST_STATE_CHANGE_SUCCESS;
  GstOmxCameraBinSrc *self = GST_OMX_CAMERA_BIN_SRC (element);

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, trans);

  if (ret == GST_STATE_CHANGE_FAILURE)
    goto end;

  switch (trans) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      self->drop_newseg = FALSE;
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      break;
    case GST_STATE_CHANGE_NULL_TO_READY:
      break;
    default:
      break;
  }

end:
  return ret;
}

static void
gst_omx_camera_bin_src_base_init (gpointer g_class)
{
  GstElementClass *gstelement_class = GST_ELEMENT_CLASS (g_class);

  GST_DEBUG_CATEGORY_INIT (omx_camera_bin_src_debug, "omxcamerabinsrc",
      0, "omx_camera camerabin2 adapter");

  gst_element_class_set_details_simple (gstelement_class,
      "omx_camera camerabin2 adapter", "Source/Video",
      "omx_camera camerabin2 adapter",
      "Alessandro Decina <alessandro.decina@collabora.co.uk>");
}

static void
gst_omx_camera_bin_src_class_init (GstOmxCameraBinSrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseCameraBinSrcClass *gstbasecamerasrc_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);
  gstbasecamerasrc_class = GST_BASE_CAMERA_SRC_CLASS (klass);

  gobject_class->dispose = gst_omx_camera_bin_src_dispose;
  gobject_class->finalize =
      (GObjectFinalizeFunc) gst_omx_camera_bin_src_finalize;
  gobject_class->set_property = gst_omx_camera_bin_src_set_property;
  gobject_class->get_property = gst_omx_camera_bin_src_get_property;

  gstelement_class->change_state = gst_omx_camera_bin_src_change_state;

  gstbasecamerasrc_class->construct_pipeline =
      gst_omx_camera_bin_src_construct_pipeline;
  gstbasecamerasrc_class->set_zoom = gst_omx_camera_bin_src_set_zoom;
  gstbasecamerasrc_class->set_mode = gst_omx_camera_bin_src_set_mode;
  gstbasecamerasrc_class->get_allowed_input_caps =
      gst_omx_camera_bin_src_get_allowed_input_caps;
  gstbasecamerasrc_class->start_capture = gst_omx_camera_bin_src_start_capture;
  gstbasecamerasrc_class->stop_capture = gst_omx_camera_bin_src_stop_capture;
}

static void
gst_omx_camera_bin_src_init (GstOmxCameraBinSrc * self,
    GstOmxCameraBinSrcClass * klass)
{
  self->vfsrc =
      gst_ghost_pad_new_no_target (GST_BASE_CAMERA_SRC_VIEWFINDER_PAD_NAME,
      GST_PAD_SRC);
  gst_element_add_pad (GST_ELEMENT (self), self->vfsrc);

  self->imgsrc =
      gst_ghost_pad_new_no_target (GST_BASE_CAMERA_SRC_IMAGE_PAD_NAME,
      GST_PAD_SRC);
  gst_element_add_pad (GST_ELEMENT (self), self->imgsrc);

  self->vidsrc =
      gst_ghost_pad_new_no_target (GST_BASE_CAMERA_SRC_VIDEO_PAD_NAME,
      GST_PAD_SRC);
  gst_element_add_pad (GST_ELEMENT (self), self->vidsrc);

  self->srcpad_event_func = GST_PAD_EVENTFUNC (self->vfsrc);

  self->video_rec_status = GST_VIDEO_RECORDING_STATUS_DONE;
  self->mode = GST_BASE_CAMERA_SRC_CAST (self)->mode;
}

gboolean
gst_omx_camera_bin_src_plugin_init (GstPlugin * plugin)
{
  return gst_element_register (plugin, "omxcamerabinsrc", GST_RANK_PRIMARY + 1,
      gst_omx_camera_bin_src_get_type ());
}
