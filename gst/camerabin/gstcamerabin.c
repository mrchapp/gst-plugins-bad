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

/**
 * SECTION:element-camerabin
 *
 * GstCameraBin is a high-level camera object that encapsulates the gstreamer
 * internals and provides a task based API for the application. It consists of
 * three main data paths: view-finder, image capture and video capture.
 *
 * <informalfigure>
 *   <mediaobject>
 *     <imageobject><imagedata fileref="camerabin.png"/></imageobject>
 *     <textobject><phrase>CameraBin structure</phrase></textobject>
 *     <caption><para>Structural decomposition of CameraBin object.</para></caption>
 *   </mediaobject>
 * </informalfigure>
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m camerabin
 * ]|
 * </refsect2>
 * <refsect2>
 * <title>Image capture</title>
 * <para>
 * Taking still images is initiated with the #GstCameraBin::capture-start action
 * signal. Once the image has been captured, "image-captured" gst message is
 * posted to the bus and capturing another image is possible. If application 
 * has set #GstCameraBin:preview-caps property, then a "preview-image" gst
 * message is posted to bus containing preview image formatted according to
 * specified caps. Eventually when image has been saved
 * #GstCameraBin::image-done signal is emitted.
 * 
 * Available resolutions can be taken from the #GstCameraBin:video-source-caps
 * property. Image capture resolution can be set with 
 * #GstCameraBin::set-image-resolution action signal.
 * </para>
 * </refsect2>
 * <refsect2>
 * <title>Video capture</title>
 * <para>
 * Video capture is started with the #GstCameraBin::capture-start action signal
 * too. In addition to image capture one can use #GstCameraBin::capture-pause to
 * pause recording and #GstCameraBin::capture-stop to end recording.
 * 
 * Available resolutions and fps can be taken from the 
 * #GstCameraBin:video-source-caps property. 
 * #GstCameraBin::set-video-resolution-fps action signal can be used to set
 * frame rate and resolution for the video recording and view finder as well.
 * </para>
 * </refsect2>
 * <refsect2>
 * <title>Photography interface</title>
 * <para>
 * GstCameraBin implements #GstPhotography interface, which can be used to set
 * and get different settings related to digital imaging. Since currently many
 * of these settings require low-level support the photography interface support
 * is dependent on video src element. In practice photography interface settings
 * cannot be used successfully until in PAUSED state when the video src has
 * opened the video device. However it is possible to configure photography
 * settings in NULL state and camerabin will try applying them later.
 * </para>
 * </refsect2>
 * <refsect2>
 * <title>States</title>
 * <para>
 * Elements within GstCameraBin are created and destroyed when switching
 * between NULL and READY states. Therefore element properties should be set
 * in NULL state. User set elements are not unreffed until GstCameraBin is
 * unreffed or replaced by a new user set element. Initially only elements
 * needed for view finder mode are created to speed up startup. Image bin and
 * video bin elements are created when setting the mode or starting capture.
 * </para>
 * </refsect2>
 * <refsect2>
 * <title>Video and image previews</title>
 * <para>
 * GstCameraBin contains "preview-caps" property, which is used to determine
 * whether the application wants a preview image of the captured picture or
 * video. When set, a GstMessage named "preview-image" will be sent. This
 * message will contain a GstBuffer holding the preview image, converted
 * to a format defined by those preview caps. The ownership of the preview
 * image is kept in GstCameraBin, so application should ref the preview buffer
 * object if it needs to use it elsewhere than in message handler.
 * 
 * Defining preview caps is done by selecting the capturing mode first and
 * then setting the property. Camerabin remembers caps separately for both
 * modes, so it is not necessary to set the caps again after changing the mode.
 * </para>
 * </refsect2>
 * <refsect2>
 * <note>
 * <para>
 * Since the muxers tested so far have problems with discontinous buffers, QoS
 * has been disabled, and then in order to record video, you MUST ensure that
 * there is enough CPU to encode the video. Thus choose smart resolution and
 * frames per second values. It is also highly recommended to avoid color
 * conversions; make sure all the elements involved work with the same
 * colorspace (i.e. rgb or yuv i420 or whatelse).
 * </para>
 * </note>
 * </refsect2>
 */

/*
 * The pipeline in the camerabin is
 *
 * videosrc [ ! ffmpegcsp ] ! capsfilter ! crop ! scale ! capsfilter ! \
 *     [ video_filter ! ] out-sel name=osel ! queue name=img_q
 *
 * View finder:
 * osel. ! in-sel name=isel ! scale ! capsfilter [ ! ffmpegcsp ] ! vfsink
 *
 * Image bin:
 * img_q. [ ! ipp ] ! ffmpegcsp ! imageenc ! metadatamux ! filesink
 *
 * Video bin:
 * osel. ! tee name=t ! queue ! videoenc ! videomux name=mux ! filesink
 * t. ! queue ! isel.
 * audiosrc ! queue ! audioconvert ! volume ! audioenc ! mux.
 *
 * The properties of elements are:
 *
 *   vfsink - "sync", FALSE, "qos", FALSE, "async", FALSE
 *   output-selector - "resend-latest", FALSE
 *   input-selector - "select-all", TRUE
 */

/*
 * includes
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <stdlib.h>

#include <gst/gst.h>
#include <gst/camerasrc/gstbasecamerasrc.h>
#include "gstv4l2camerasrc.h"
/* FIXME: include #include <gst/gst-i18n-plugin.h> and use _(" ") */

#include "gstcamerabin.h"
#include "gstcamerabincolorbalance.h"
#include "gstcamerabinphotography.h"

#include "camerabindebug.h"
#include "camerabingeneral.h"
#include "camerabinpreview.h"

#include "gstcamerabin-marshal.h"

/*
 * enum and types
 */

enum
{
  /* action signals */
  CAPTURE_START_SIGNAL,
  CAPTURE_STOP_SIGNAL,
  CAPTURE_PAUSE_SIGNAL,
  SET_VIDEO_RESOLUTION_FPS_SIGNAL,
  SET_IMAGE_RESOLUTION_SIGNAL,
  /* emit signals */
  IMG_DONE_SIGNAL,
  LAST_SIGNAL
};


/*
 * defines and static global vars
 */

static guint camerabin_signals[LAST_SIGNAL];

/* default and range values for args */

#define DEFAULT_MODE MODE_PREVIEW

#define DEFAULT_FLAGS GST_CAMERABIN_FLAG_SOURCE_RESIZE | \
  GST_CAMERABIN_FLAG_VIEWFINDER_SCALE | \
  GST_CAMERABIN_FLAG_IMAGE_COLOR_CONVERSION

#define DEFAULT_BLOCK_VIEWFINDER FALSE

/* message names */
#define PREVIEW_MESSAGE_NAME "preview-image"
#define IMG_CAPTURED_MESSAGE_NAME "image-captured"

/*
 * static helper functions declaration
 */

static void camerabin_setup_src_elements (GstCameraBin * camera);

static void camerabin_setup_view_elements (GstCameraBin * camera);

static gboolean camerabin_create_view_elements (GstCameraBin * camera);

static gboolean camerabin_create_elements (GstCameraBin * camera);

static void camerabin_destroy_elements (GstCameraBin * camera);

static void camerabin_dispose_elements (GstCameraBin * camera);

static void gst_camerabin_change_mode (GstCameraBin * camera, gint mode);

static void
gst_camerabin_set_flags (GstCameraBin * camera, GstCameraBinFlags flags);

static void
gst_camerabin_change_filename (GstCameraBin * camera, const gchar * name);

static void gst_camerabin_rewrite_tags (GstCameraBin * camera);

static void gst_camerabin_start_image_capture (GstCameraBin * camera);

static void gst_camerabin_start_video_recording (GstCameraBin * camera);

static void
camerabin_pad_blocked (GstPad * pad, gboolean blocked, gpointer user_data);

static gboolean
gst_camerabin_have_img_buffer (GstPad * pad, GstBuffer * buffer,
    gpointer u_data);
static gboolean
gst_camerabin_have_vid_buffer (GstPad * pad, GstBuffer * buffer,
    gpointer u_data);
static gboolean
gst_camerabin_have_queue_data (GstPad * pad, GstMiniObject * mini_obj,
    gpointer u_data);
static gboolean
gst_camerabin_have_src_buffer (GstPad * pad, GstBuffer * buffer,
    gpointer u_data);

static void gst_camerabin_reset_to_view_finder (GstCameraBin * camera);

static void gst_camerabin_do_stop (GstCameraBin * camera);

static void gst_camerabin_proxy_notify_cb (GObject * video_source,
    GParamSpec * pspec, gpointer user_data);
static void gst_camerabin_monitor_video_source_properties (GstCameraBin *
    camera);

/*
 * GObject callback functions declaration
 */

static void gst_camerabin_dispose (GObject * object);

static void gst_camerabin_finalize (GObject * object);

static void gst_camerabin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);

static void gst_camerabin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static void gst_camerabin_override_photo_properties (GObjectClass *
    gobject_class);

/*
 * GstElement function declarations
 */

static GstStateChangeReturn
gst_camerabin_change_state (GstElement * element, GstStateChange transition);


/*
 * GstBin function declarations
 */
static void
gst_camerabin_handle_message_func (GstBin * bin, GstMessage * message);


/*
 * Action signal function declarations
 */

static void gst_camerabin_capture_start (GstCameraBin * camera);

static void gst_camerabin_capture_stop (GstCameraBin * camera);

static void gst_camerabin_capture_pause (GstCameraBin * camera);

static void
gst_camerabin_set_video_resolution_fps (GstCameraBin * camera, gint width,
    gint height, gint fps_n, gint fps_d);

static void
gst_camerabin_set_image_resolution (GstCameraBin * camera, gint width,
    gint height);


/*
 * GST BOILERPLATE and GObject types
 */

static gboolean
gst_camerabin_iface_supported (GstImplementsInterface * iface, GType iface_type)
{
  GstCameraBin *camera = GST_CAMERABIN (iface);

  if (iface_type == GST_TYPE_COLOR_BALANCE) {
    return
        gst_base_camera_src_get_color_balance (GST_BASE_CAMERA_SRC
        (camera->srcbin)) != NULL;
  } else if (iface_type == GST_TYPE_TAG_SETTER) {
    /* Note: Tag setter elements aren't
       present when image and video bin in NULL */
    GstElement *setter;
    setter = gst_bin_get_by_interface (GST_BIN (camera), iface_type);
    if (setter) {
      gst_object_unref (setter);
      return TRUE;
    } else {
      return FALSE;
    }
  } else if (iface_type == GST_TYPE_PHOTOGRAPHY) {
    /* Always support photography interface */
    return TRUE;
  }

  return FALSE;
}

static void
gst_camerabin_interface_init (GstImplementsInterfaceClass * klass)
{
  /*
   * default virtual functions
   */
  klass->supported = gst_camerabin_iface_supported;
}

static void
camerabin_init_interfaces (GType type)
{

  static const GInterfaceInfo camerabin_info = {
    (GInterfaceInitFunc) gst_camerabin_interface_init,
    NULL,
    NULL,
  };

  static const GInterfaceInfo camerabin_color_balance_info = {
    (GInterfaceInitFunc) gst_camerabin_color_balance_init,
    NULL,
    NULL,
  };

  static const GInterfaceInfo camerabin_tagsetter_info = {
    NULL,
    NULL,
    NULL,
  };
  static const GInterfaceInfo camerabin_photography_info = {
    (GInterfaceInitFunc) gst_camerabin_photography_init,
    NULL,
    NULL,
  };

  g_type_add_interface_static (type,
      GST_TYPE_IMPLEMENTS_INTERFACE, &camerabin_info);

  g_type_add_interface_static (type, GST_TYPE_COLOR_BALANCE,
      &camerabin_color_balance_info);

  g_type_add_interface_static (type, GST_TYPE_TAG_SETTER,
      &camerabin_tagsetter_info);

  g_type_add_interface_static (type, GST_TYPE_PHOTOGRAPHY,
      &camerabin_photography_info);
}

GST_BOILERPLATE_FULL (GstCameraBin, gst_camerabin, GstPipeline,
    GST_TYPE_PIPELINE, camerabin_init_interfaces);

/*
 * static helper functions implementation
 */

/*
 * camerabin_setup_src_elements:
 * @camera: camerabin object
 *
 * This function updates camerabin capsfilters according
 * to fps, resolution and zoom that have been configured
 * to camerabin.
 */
static void
camerabin_setup_src_elements (GstCameraBin * camera)
{
  GstBaseCameraSrc *srcbin = GST_BASE_CAMERA_SRC (camera->srcbin);
  GstPhotography *photography;

  gst_camerabin_monitor_video_source_properties (camera);

  /* Update photography interface settings */
  photography = gst_base_camera_src_get_photography (srcbin);
  if (photography) {
    gst_photography_set_config (photography, &camera->photo_settings);
  }
}

/*
 * camerabin_setup_view_elements:
 * @camera: camerabin object
 *
 * This function configures properties for view finder sink element.
 */
static void
camerabin_setup_view_elements (GstCameraBin * camera)
{
  GST_DEBUG_OBJECT (camera, "setting view finder properties");
  /* Set properties for view finder sink */
  /* Find the actual sink if using bin like autovideosink */
  if (GST_IS_BIN (camera->view_sink)) {
    GList *child = NULL, *children = GST_BIN_CHILDREN (camera->view_sink);
    for (child = children; child != NULL; child = g_list_next (child)) {
      GObject *ch = G_OBJECT (child->data);
      if (g_object_class_find_property (G_OBJECT_GET_CLASS (ch), "sync")) {
        g_object_set (G_OBJECT (ch), "sync", FALSE, "qos", FALSE, "async",
            FALSE, NULL);
      }
    }
  } else {
    g_object_set (G_OBJECT (camera->view_sink), "sync", FALSE, "qos", FALSE,
        "async", FALSE, NULL);
  }
}

/*
 * camerabin_create_view_elements:
 * @camera: camerabin object
 *
 * This function creates and links downstream side elements for camerabin.
 * ! scale ! cspconv ! view finder sink
 *
 * Returns: TRUE, if elements were successfully created, FALSE otherwise
 */
static gboolean
camerabin_create_view_elements (GstCameraBin * camera)
{
  GstBin *cbin = GST_BIN (camera);
  GstElement *queue = NULL;

  /* Add queue leading to view finder */
  if (!(queue = gst_camerabin_create_and_add_element (cbin, "queue"))) {
    goto error;
  }

  /* Set queue leaky, we don't want to block video encoder feed, but
   * prefer leaking view finder buffers instead, in case srcbin is pushing
   * down both paths in same thread.
   */
  g_object_set (G_OBJECT (queue), "leaky", 2, "max-size-buffers", 1, NULL);

  /* Add videoscale in case we need to downscale frame for view finder */
  if (camera->flags & GST_CAMERABIN_FLAG_VIEWFINDER_SCALE) {
    if (!(camera->view_scale =
            gst_camerabin_create_and_add_element (cbin, "videoscale"))) {
      goto error;
    }

    /* Add capsfilter to maintain aspect ratio while scaling */
    if (!(camera->aspect_filter =
            gst_camerabin_create_and_add_element (cbin, "capsfilter"))) {
      goto error;
    }
  }
  if (camera->flags & GST_CAMERABIN_FLAG_VIEWFINDER_COLOR_CONVERSION) {
    if (!gst_camerabin_create_and_add_element (cbin, "ffmpegcolorspace")) {
      goto error;
    }
  }

  if (camera->app_viewfinder_filter) {
    if (!gst_camerabin_add_element (GST_BIN (camera),
            camera->app_viewfinder_filter)) {
      goto error;
    }
  }

  /* Add application set or default video sink element */
  if (!(camera->view_sink = gst_camerabin_setup_default_element (cbin,
              camera->app_vf_sink, "autovideosink", DEFAULT_VIDEOSINK))) {
    camera->view_sink = NULL;
    goto error;
  } else {
    if (!gst_camerabin_add_element (cbin, camera->view_sink))
      goto error;
  }

  return TRUE;
error:
  return FALSE;
}

/*
 * camerabin_create_elements:
 * @camera: camerabin object
 *
 * This function creates and links all elements for camerabin,
 *
 * Returns: TRUE, if elements were successfully created, FALSE otherwise
 */
static gboolean
camerabin_create_elements (GstCameraBin * camera)
{
  gboolean ret = FALSE;

  GST_LOG_OBJECT (camera, "creating elements");

  /* Add camera src bin */
  if (!gst_camerabin_add_element (GST_BIN (camera), camera->srcbin)) {
    goto done;
  }

  camera->pad_src_img = gst_element_get_static_pad (camera->srcbin, "imgsrc");

  GST_DEBUG_OBJECT (camera, "pad_src_img: %" GST_PTR_FORMAT,
      camera->pad_src_img);

  gst_pad_add_buffer_probe (camera->pad_src_img,
      G_CALLBACK (gst_camerabin_have_img_buffer), camera);

  /* Add queue leading to image bin */
  camera->img_queue = gst_element_factory_make ("queue", "image-queue");
  if (!gst_camerabin_add_element_full (GST_BIN (camera), "imgsrc",
          camera->img_queue, NULL)) {
    goto done;
  }

  /* To avoid deadlock, we won't restrict the image queue size */
  /* FIXME: actually we would like to have some kind of restriction here (size),
     but deadlocks must be handled somehow... */
  g_object_set (G_OBJECT (camera->img_queue), "max-size-buffers", 0,
      "max-size-bytes", 0, "max-size-time", G_GUINT64_CONSTANT (0), NULL);

  camera->pad_src_queue = gst_element_get_static_pad (camera->img_queue, "src");

  gst_pad_add_data_probe (camera->pad_src_queue,
      G_CALLBACK (gst_camerabin_have_queue_data), camera);

  /* Add image bin */
  if (!gst_camerabin_add_element (GST_BIN (camera), camera->imgbin)) {
    goto done;
  }

  camera->pad_src_view = gst_element_get_static_pad (camera->srcbin, "vfsrc");

  /* Add video bin */
  camera->pad_src_vid = gst_element_get_static_pad (camera->srcbin, "vidsrc");
  if (!gst_camerabin_add_element_full (GST_BIN (camera), "vidsrc",
          camera->vidbin, NULL)) {
    goto done;
  }
  gst_pad_add_buffer_probe (camera->pad_src_vid,
      G_CALLBACK (gst_camerabin_have_vid_buffer), camera);

  /* Create view finder elements */
  if (!camerabin_create_view_elements (camera)) {
    GST_WARNING_OBJECT (camera, "creating view finder elements failed");
    goto done;
  }

  /* Set view finder active as default */
  gst_base_camera_src_set_mode (GST_BASE_CAMERA_SRC (camera->srcbin),
      MODE_PREVIEW);

  ret = TRUE;

done:

  if (FALSE == ret)
    camerabin_destroy_elements (camera);

  return ret;
}

/*
 * camerabin_destroy_elements:
 * @camera: camerabin object
 *
 * This function removes all elements from camerabin.
 */
static void
camerabin_destroy_elements (GstCameraBin * camera)
{
  GST_DEBUG_OBJECT (camera, "destroying elements");

  /* Release request pads */
  if (camera->pad_src_vid) {
    gst_object_unref (camera->pad_src_vid);
    camera->pad_src_vid = NULL;
  }
  if (camera->pad_src_img) {
    gst_object_unref (camera->pad_src_img);
    camera->pad_src_img = NULL;
  }
  if (camera->pad_src_view) {
    gst_object_unref (camera->pad_src_view);
    camera->pad_src_view = NULL;
  }

  if (camera->pad_src_queue) {
    gst_object_unref (camera->pad_src_queue);
    camera->pad_src_queue = NULL;
  }

  /* view finder elements */
  camera->view_scale = NULL;
  camera->aspect_filter = NULL;
  camera->view_sink = NULL;

  /* source elements */
  /* XXX do I need to unref, or does gst_camerabin_remove_elements_from_bin
   * do this for me? */
  camera->srcbin = NULL;

  camera->active_bin = NULL;

  /* Remove elements */
  gst_camerabin_remove_elements_from_bin (GST_BIN (camera));
}

/*
 * camerabin_dispose_elements:
 * @camera: camerabin object
 *
 * This function releases all allocated camerabin resources.
 */
static void
camerabin_dispose_elements (GstCameraBin * camera)
{
  GST_INFO ("cleaning");

  if (camera->capture_mutex) {
    g_mutex_free (camera->capture_mutex);
    camera->capture_mutex = NULL;
  }
  if (camera->cond) {
    g_cond_free (camera->cond);
    camera->cond = NULL;
  }
  if (camera->filename) {
    g_string_free (camera->filename, TRUE);
    camera->filename = NULL;
  }
  /* Unref application set elements */
  if (camera->app_vf_sink) {
    gst_object_unref (camera->app_vf_sink);
    camera->app_vf_sink = NULL;
  }
  if (camera->app_viewfinder_filter) {
    gst_object_unref (camera->app_viewfinder_filter);
    camera->app_viewfinder_filter = NULL;
  }

  /* Free caps */
  gst_caps_replace (&camera->preview_caps, NULL);
  gst_caps_replace (&camera->video_preview_caps, NULL);
  gst_buffer_replace (&camera->video_preview_buffer, NULL);

  if (camera->event_tags) {
    gst_tag_list_free (camera->event_tags);
    camera->event_tags = NULL;
  }
}

/*
 * gst_camerabin_image_capture_continue:
 * @camera: camerabin object
 * @filename: filename of the finished image
 *
 * Notify application that image has been saved with a signal.
 *
 * Returns TRUE if another image should be captured, FALSE otherwise.
 */
static gboolean
gst_camerabin_image_capture_continue (GstCameraBin * camera,
    const gchar * filename)
{
  gboolean cont = FALSE;

  GST_DEBUG_OBJECT (camera, "emitting img_done signal, filename: %s", filename);
  g_signal_emit (G_OBJECT (camera), camerabin_signals[IMG_DONE_SIGNAL], 0,
      filename, &cont);

  /* If the app wants to continue make sure new filename has been set */
  if (cont && g_str_equal (camera->filename->str, "")) {
    GST_ELEMENT_ERROR (camera, RESOURCE, NOT_FOUND,
        ("cannot continue capture, no filename has been set"), (NULL));
    cont = FALSE;
  }

  return cont;
}

/*
 * gst_camerabin_change_mode:
 * @camera: camerabin object
 * @mode: image or video mode
 *
 * Change camerabin mode between image and video capture.
 * Changing mode will stop ongoing capture.
 */
static void
gst_camerabin_change_mode (GstCameraBin * camera, gint mode)
{
  if (camera->mode != mode || !camera->active_bin) {
    GstState state;

    GST_DEBUG_OBJECT (camera, "setting mode: %d (old_mode=%d)",
        mode, camera->mode);
    /* Interrupt ongoing capture */
    gst_camerabin_do_stop (camera);
    // XXX need to re-think this a bit.. originally camerabin
    // had mode seperate from capturing state.. ie. either
    // img or vid mode, but not capturing, is preview mode..
    // but camsrcbin has img/vid/preview modes..  need to
    // align concept of modes..
    camera->mode = mode;
    gst_element_get_state (GST_ELEMENT (camera), &state, NULL, 0);
    if (state == GST_STATE_PAUSED || state == GST_STATE_PLAYING) {
      if (camera->active_bin) {
        GST_DEBUG_OBJECT (camera, "stopping active bin");
        gst_element_set_state (camera->active_bin, GST_STATE_READY);
      }
      if (camera->mode == MODE_IMAGE) {
        GstStateChangeReturn state_ret;

        camera->active_bin = camera->imgbin;
        state_ret =
            gst_element_set_state (camera->active_bin, GST_STATE_PAUSED);

        if (state_ret == GST_STATE_CHANGE_FAILURE) {
          GST_WARNING_OBJECT (camera, "state change failed");
          gst_element_set_state (camera->active_bin, GST_STATE_NULL);
          camera->active_bin = NULL;
        }
      } else if (camera->mode == MODE_VIDEO) {
        camera->active_bin = camera->vidbin;
      }
      gst_camerabin_reset_to_view_finder (camera);
    }
  }
}

/*
 * gst_camerabin_set_flags:
 * @camera: camerabin object
 * @flags: flags for camerabin, videobin and imagebin
 *
 * Change camerabin capture flags.
 */
static void
gst_camerabin_set_flags (GstCameraBin * camera, GstCameraBinFlags flags)
{
  g_return_if_fail (camera != NULL);

  GST_DEBUG_OBJECT (camera, "setting flags: %d", flags);

  GST_OBJECT_LOCK (camera);
  camera->flags = flags;
  GST_OBJECT_UNLOCK (camera);

  gst_camerabin_video_set_flags (GST_CAMERABIN_VIDEO (camera->vidbin), flags);
  gst_camerabin_image_set_flags (GST_CAMERABIN_IMAGE (camera->imgbin), flags);
}

/*
 * gst_camerabin_change_filename:
 * @camera: camerabin object
 * @name: new filename for capture
 *
 * Change filename for image or video capture.
 */
static void
gst_camerabin_change_filename (GstCameraBin * camera, const gchar * name)
{
  if (name == NULL)
    name = "";

  if (0 != strcmp (camera->filename->str, name)) {
    GST_DEBUG_OBJECT (camera, "changing filename from '%s' to '%s'",
        camera->filename->str, name);
    g_string_assign (camera->filename, name);
  }
}

/*
 * gst_camerabin_send_img_queue_event:
 * @camera: camerabin object
 * @event: event to be sent
 *
 * Send the given event to image queue.
 */
static void
gst_camerabin_send_img_queue_event (GstCameraBin * camera, GstEvent * event)
{
  GstPad *queue_sink;

  g_return_if_fail (camera != NULL);
  g_return_if_fail (event != NULL);

  queue_sink = gst_element_get_static_pad (camera->img_queue, "sink");
  gst_pad_send_event (queue_sink, event);
  gst_object_unref (queue_sink);
}

/*
 * gst_camerabin_send_img_queue_custom_event:
 * @camera: camerabin object
 * @ev_struct: event structure to be sent
 *
 * Generate and send a custom event to image queue.
 */
static void
gst_camerabin_send_img_queue_custom_event (GstCameraBin * camera,
    GstStructure * ev_struct)
{
  GstEvent *event;

  g_return_if_fail (camera != NULL);
  g_return_if_fail (ev_struct != NULL);

  event = gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM, ev_struct);
  gst_camerabin_send_img_queue_event (camera, event);
}

/*
 * gst_camerabin_rewrite_tags_to_bin:
 * @bin: bin holding tag setter elements
 * @list: tag list to be written
 *
 * This function looks for certain tag setters from given bin
 * and REPLACES ALL setter tags with given tag list
 *
 */
static void
gst_camerabin_rewrite_tags_to_bin (GstBin * bin, const GstTagList * list)
{
  GstElement *setter;
  GstElementFactory *setter_factory;
  const gchar *klass;
  GstIterator *iter;
  GstIteratorResult res = GST_ITERATOR_OK;
  gpointer data;

  iter = gst_bin_iterate_all_by_interface (bin, GST_TYPE_TAG_SETTER);

  while (res == GST_ITERATOR_OK || res == GST_ITERATOR_RESYNC) {
    res = gst_iterator_next (iter, &data);
    switch (res) {
      case GST_ITERATOR_DONE:
        break;
      case GST_ITERATOR_RESYNC:
        gst_iterator_resync (iter);
        break;
      case GST_ITERATOR_ERROR:
        GST_WARNING ("error iterating tag setters");
        break;
      case GST_ITERATOR_OK:
        setter = GST_ELEMENT (data);
        GST_LOG ("iterating tag setters: %" GST_PTR_FORMAT, setter);
        setter_factory = gst_element_get_factory (setter);
        klass = gst_element_factory_get_klass (setter_factory);
        /* FIXME: check if tags should be written to all tag setters,
           set tags only to Muxer elements for now */
        if (g_strrstr (klass, "Muxer")) {
          GST_DEBUG ("replacement tags %" GST_PTR_FORMAT, list);
          gst_tag_setter_merge_tags (GST_TAG_SETTER (setter), list,
              GST_TAG_MERGE_REPLACE_ALL);
        }
        gst_object_unref (setter);
        break;
      default:
        break;
    }
  }

  gst_iterator_free (iter);
}

/*
 * gst_camerabin_get_internal_tags:
 * @camera: the camera bin element
 *
 * Returns tag list containing metadata from camerabin
 * and it's elements
 */
static GstTagList *
gst_camerabin_get_internal_tags (GstCameraBin * camera)
{
  GstTagList *list = gst_tag_list_new ();
  GstColorBalance *balance = NULL;
  const GList *controls = NULL, *item;
  GstColorBalanceChannel *channel;
  gint min_value, max_value, mid_value, cur_value;
  gint width, height, zoom;

  if (camera->active_bin == camera->vidbin) {
    /* FIXME: check if internal video tag setting is needed */
    goto done;
  }

  g_object_get (camera->srcbin, "width", &width, "height", &height,
      "zoom", &zoom, NULL);

  gst_tag_list_add (list, GST_TAG_MERGE_REPLACE,
      "image-width", width, "image-height", height, NULL);

  gst_tag_list_add (list, GST_TAG_MERGE_REPLACE,
      "capture-digital-zoom", zoom, 100, NULL);

  if (gst_element_implements_interface (GST_ELEMENT (camera),
          GST_TYPE_COLOR_BALANCE)) {
    balance = GST_COLOR_BALANCE (camera);
  }

  if (balance) {
    controls = gst_color_balance_list_channels (balance);
  }
  for (item = controls; item; item = g_list_next (item)) {
    channel = item->data;
    min_value = channel->min_value;
    max_value = channel->max_value;
    /* the default value would probably better */
    mid_value = min_value + ((max_value - min_value) / 2);
    cur_value = gst_color_balance_get_value (balance, channel);

    if (!g_ascii_strcasecmp (channel->label, "brightness")) {
      /* The value of brightness. The unit is the APEX value (Additive System of Photographic Exposure).
       * Ordinarily it is given in the range of -99.99 to 99.99. Note that
       * if the numerator of the recorded value is 0xFFFFFFFF, Unknown shall be indicated.
       *
       * BrightnessValue (Bv) = log2 ( B/NK )
       * Note that: B:cd/cm² (candela per square centimeter), N,K: constant
       *
       * http://johnlind.tripod.com/science/scienceexposure.html
       *
       */
      gst_tag_list_add (list, GST_TAG_MERGE_REPLACE,
          "capture-brightness", cur_value, 1, NULL);
    } else if (!g_ascii_strcasecmp (channel->label, "contrast")) {
      /* 0 = Normal, 1 = Soft, 2 = Hard */

      gst_tag_list_add (list, GST_TAG_MERGE_REPLACE,
          "capture-contrast",
          (cur_value == mid_value) ? 0 : ((cur_value < mid_value) ? 1 : 2),
          NULL);
    } else if (!g_ascii_strcasecmp (channel->label, "gain")) {
      /* 0 = Normal, 1 = Low Up, 2 = High Up, 3 = Low Down, 4 = Hight Down */
      gst_tag_list_add (list, GST_TAG_MERGE_REPLACE,
          "capture-gain",
          (guint) (cur_value == mid_value) ? 0 : ((cur_value <
                  mid_value) ? 1 : 3), NULL);
    } else if (!g_ascii_strcasecmp (channel->label, "saturation")) {
      /* 0 = Normal, 1 = Low, 2 = High */
      gst_tag_list_add (list, GST_TAG_MERGE_REPLACE,
          "capture-saturation",
          (cur_value == mid_value) ? 0 : ((cur_value < mid_value) ? 1 : 2),
          NULL);
    }
  }

done:

  return list;
}

/*
 * gst_camerabin_rewrite_tags:
 * @camera: the camera bin element
 *
 * Merges application set tags to camerabin internal tags,
 * and writes them using image or video bin tag setters.
 */
static void
gst_camerabin_rewrite_tags (GstCameraBin * camera)
{
  const GstTagList *app_tag_list = NULL;
  GstTagList *list = NULL;

  /* Get application set tags */
  app_tag_list = gst_tag_setter_get_tag_list (GST_TAG_SETTER (camera));

  /* Get tags from camerabin and it's elements */
  list = gst_camerabin_get_internal_tags (camera);

  if (app_tag_list) {
    gst_tag_list_insert (list, app_tag_list, GST_TAG_MERGE_REPLACE);
  }

  /* Write tags */
  if (camera->active_bin == camera->vidbin) {
    gst_camerabin_rewrite_tags_to_bin (GST_BIN (camera->active_bin), list);
  } else {
    /* Image tags need to be sent as a serialized event into image queue */
    GstEvent *tagevent = gst_event_new_tag (gst_tag_list_copy (list));
    gst_camerabin_send_img_queue_event (camera, tagevent);
  }

  gst_tag_list_free (list);
}

/*
 * gst_camerabin_start_image_capture:
 * @camera: camerabin object
 *
 * Initiates image capture.
 */
static void
gst_camerabin_start_image_capture (GstCameraBin * camera)
{
  gboolean ret = FALSE;

  GST_INFO_OBJECT (camera, "starting image capture");

  g_mutex_lock (camera->capture_mutex);

  /* Enable still image capture mode in v4l2camsrc */
  ret = gst_base_camera_src_set_mode (GST_BASE_CAMERA_SRC (camera->srcbin),
      MODE_IMAGE);

  camera->capturing = TRUE;

  g_mutex_unlock (camera->capture_mutex);

  if (!ret) {
    GST_WARNING_OBJECT (camera, "starting image capture failed");
  }
}

 /*
  * FIXME ideally a caps renegotiation is better here
  */
static void
reset_video_capture_caps (GstCameraBin * camera)
{
  GstState state, pending;

//XXX  GST_INFO_OBJECT (camera, "switching resolution to %dx%d and fps to %d/%d",
//XXX      camera->width, camera->height, camera->fps_n, camera->fps_d);

  /* Interrupt ongoing capture */
  gst_camerabin_do_stop (camera);

  gst_element_get_state (GST_ELEMENT (camera), &state, &pending, 0);
  if (state == GST_STATE_PAUSED || state == GST_STATE_PLAYING) {
    GST_INFO_OBJECT (camera,
        "changing to READY to initialize videosrc with new format");
    gst_element_set_state (GST_ELEMENT (camera), GST_STATE_READY);
  }
  if (pending != GST_STATE_VOID_PENDING) {
    GST_LOG_OBJECT (camera, "restoring pending state: %s",
        gst_element_state_get_name (pending));
    state = pending;
  }

  gst_element_set_state (GST_ELEMENT (camera), state);
}

/*
 * gst_camerabin_start_video_recording:
 * @camera: camerabin object
 *
 * Initiates video recording.
 */
static void
gst_camerabin_start_video_recording (GstCameraBin * camera)
{
  GstStateChangeReturn state_ret;
  /* FIXME: how to ensure resolution and fps is supported by CPU?
   * use a queue overrun signal?
   */
  GST_INFO_OBJECT (camera, "starting video capture");

  /* check if need to update video capture caps */
  if (camera->video_capture_caps_update) {
    reset_video_capture_caps (camera);
  }

  gst_camerabin_rewrite_tags (camera);

  /* Pause the pipeline in order to distribute new clock in paused_to_playing */
  state_ret = gst_element_set_state (GST_ELEMENT (camera), GST_STATE_PAUSED);

  if (state_ret != GST_STATE_CHANGE_FAILURE) {
    g_mutex_lock (camera->capture_mutex);
    camera->capturing = TRUE;
    g_mutex_unlock (camera->capture_mutex);
    gst_element_set_locked_state (camera->vidbin, FALSE);
    /* ensure elements activated before feeding data into it */
    state_ret = gst_element_set_state (GST_ELEMENT (camera), GST_STATE_PAUSED);
    gst_base_camera_src_set_mode (GST_BASE_CAMERA_SRC (camera->srcbin),
        MODE_VIDEO);
    gst_element_set_state (GST_ELEMENT (camera), GST_STATE_PLAYING);
    gst_element_set_locked_state (camera->vidbin, TRUE);
  } else {
    GST_WARNING_OBJECT (camera, "videobin state change failed");
    gst_element_set_state (camera->vidbin, GST_STATE_NULL);
    gst_camerabin_reset_to_view_finder (camera);
  }
}

/*
 * gst_camerabin_send_video_eos:
 * @camera: camerabin object
 *
 * Generate and send eos event to video bin in order to
 * finish recording properly.
 */
static void
gst_camerabin_send_video_eos (GstCameraBin * camera)
{
  GstPad *videopad;

  g_return_if_fail (camera != NULL);

  if (!camera->eos_handled) {
    /* Send eos event to video bin */
    GST_INFO_OBJECT (camera, "sending eos to videobin");
    videopad = gst_element_get_static_pad (camera->vidbin, "sink");
    gst_pad_send_event (videopad, gst_event_new_eos ());
    gst_object_unref (videopad);
    /* Block viewfinder after capturing if requested by application */
    if (camera->block_viewfinder) {
      gst_pad_set_blocked_async (camera->pad_src_view, TRUE,
          (GstPadBlockCallback) camerabin_pad_blocked, camera);
    }
    camera->eos_handled = TRUE;
  } else {
    GST_INFO_OBJECT (camera, "dropping duplicate EOS");
  }
}

/*
 * camerabin_pad_blocked:
 * @pad: pad to block/unblock
 * @blocked: TRUE to block, FALSE to unblock
 * @u_data: camera bin object
 *
 * Callback function for blocking a pad.
 */
static void
camerabin_pad_blocked (GstPad * pad, gboolean blocked, gpointer user_data)
{
  GstCameraBin *camera;

  camera = (GstCameraBin *) user_data;

  GST_DEBUG_OBJECT (camera, "%s %s:%s",
      blocked ? "blocking" : "unblocking", GST_DEBUG_PAD_NAME (pad));
}

/*
 * gst_camerabin_send_preview:
 * @camera: camerabin object
 * @buffer: received buffer
 *
 * Convert given buffer to desired preview format and send is as a #GstMessage
 * to application.
 *
 * Returns: TRUE always
 */
static gboolean
gst_camerabin_send_preview (GstCameraBin * camera, GstBuffer * buffer)
{
  GstElement *pipeline;
  GstBuffer *prev = NULL;
  GstStructure *s;
  GstMessage *msg;
  gboolean ret = FALSE;

  GST_DEBUG_OBJECT (camera, "creating preview");

  pipeline = (camera->mode == MODE_IMAGE) ?
      camera->preview_pipeline : camera->video_preview_pipeline;
  prev = gst_camerabin_preview_convert (camera, pipeline, buffer);

  GST_DEBUG_OBJECT (camera, "preview created: %p", prev);

  if (prev) {
    s = gst_structure_new (PREVIEW_MESSAGE_NAME,
        "buffer", GST_TYPE_BUFFER, prev, NULL);
    gst_buffer_unref (prev);

    msg = gst_message_new_element (GST_OBJECT (camera), s);

    GST_DEBUG_OBJECT (camera, "sending message with preview image");

    if (gst_element_post_message (GST_ELEMENT (camera), msg) == FALSE) {
      GST_WARNING_OBJECT (camera,
          "This element has no bus, therefore no message sent!");
    }
    ret = TRUE;
  }

  return ret;
}

/*
 * gst_camerabin_have_img_buffer:
 * @pad: output-selector src pad leading to image bin
 * @buffer: still image frame
 * @u_data: camera bin object
 *
 * Buffer probe called before sending each buffer to image queue.
 * Generates and sends preview image as gst message if requested.
 */
static gboolean
gst_camerabin_have_img_buffer (GstPad * pad, GstBuffer * buffer,
    gpointer u_data)
{
  GstCameraBin *camera = (GstCameraBin *) u_data;
  GstStructure *fn_ev_struct = NULL;
  gboolean ret = TRUE;
  GstPad *os_sink = NULL;

  GST_LOG ("got buffer %p with size %d", buffer, GST_BUFFER_SIZE (buffer));

  /* Image filename should be set by now */
  if (g_str_equal (camera->filename->str, "")) {
    GST_DEBUG_OBJECT (camera, "filename not set, dropping buffer");
    ret = FALSE;
    goto done;
  }

  if (camera->preview_caps) {
    gst_camerabin_send_preview (camera, buffer);
  }

  gst_camerabin_rewrite_tags (camera);

  /* Send a custom event which tells the filename to image queue */
  /* NOTE: This needs to be THE FIRST event to be sent to queue for
     every image. It triggers imgbin state change to PLAYING. */
  fn_ev_struct = gst_structure_new ("img-filename",
      "filename", G_TYPE_STRING, camera->filename->str, NULL);
  GST_DEBUG_OBJECT (camera, "sending filename event to image queue");
  gst_camerabin_send_img_queue_custom_event (camera, fn_ev_struct);

  /* Add buffer probe to outputselector's sink pad. It sends
     EOS event to image queue. */
  os_sink = gst_element_get_static_pad (camera->srcbin, "imgsrc");      // XXX ok to have on srcpad instead of sinkpad??
  camera->image_captured_id = gst_pad_add_buffer_probe (os_sink,
      G_CALLBACK (gst_camerabin_have_src_buffer), camera);
  gst_object_unref (os_sink);

done:

  /* HACK: v4l2camsrc changes to view finder resolution automatically
     after one captured still image */
  gst_base_camera_src_finish_image_capture (GST_BASE_CAMERA_SRC
      (camera->srcbin));

  GST_DEBUG_OBJECT (camera, "image captured, switching to viewfinder");

  gst_camerabin_reset_to_view_finder (camera);

  GST_DEBUG_OBJECT (camera, "switched back to viewfinder");

  return TRUE;
}

/*
 * gst_camerabin_have_vid_buffer:
 * @pad: output-selector src pad leading to video bin
 * @buffer: buffer pushed to the pad
 * @u_data: camerabin object
 *
 * Buffer probe for src pad leading to video bin.
 * Sends eos event to video bin if stop requested and drops
 * all buffers after this.
 */
static gboolean
gst_camerabin_have_vid_buffer (GstPad * pad, GstBuffer * buffer,
    gpointer u_data)
{
  GstCameraBin *camera = (GstCameraBin *) u_data;
  gboolean ret = TRUE;
  GST_LOG ("got video buffer %p with size %d",
      buffer, GST_BUFFER_SIZE (buffer));

  if (camera->video_preview_caps &&
      !camera->video_preview_buffer && !camera->stop_requested) {
    GST_DEBUG ("storing video preview %p", buffer);
    camera->video_preview_buffer = gst_buffer_copy (buffer);
  }

  if (G_UNLIKELY (camera->stop_requested)) {
    gst_camerabin_send_video_eos (camera);
    ret = FALSE;                /* Drop buffer */
  }

  return ret;
}

/*
 * gst_camerabin_have_src_buffer:
 * @pad: output-selector sink pad which receives frames from video source
 * @buffer: buffer pushed to the pad
 * @u_data: camerabin object
 *
 * Buffer probe for sink pad. It sends custom eos event to image queue and
 * notifies application by sending a "image-captured" message to GstBus.
 * This probe is installed after image has been captured and it disconnects
 * itself after EOS has been sent.
 */
static gboolean
gst_camerabin_have_src_buffer (GstPad * pad, GstBuffer * buffer,
    gpointer u_data)
{
  GstCameraBin *camera = (GstCameraBin *) u_data;
  GstMessage *msg;

  GST_LOG_OBJECT (camera, "got image buffer %p with size %d",
      buffer, GST_BUFFER_SIZE (buffer));

  g_mutex_lock (camera->capture_mutex);
  camera->capturing = FALSE;
  g_cond_signal (camera->cond);
  g_mutex_unlock (camera->capture_mutex);

  msg = gst_message_new_element (GST_OBJECT (camera),
      gst_structure_new (IMG_CAPTURED_MESSAGE_NAME, NULL));

  GST_DEBUG_OBJECT (camera, "sending 'image captured' message");

  if (gst_element_post_message (GST_ELEMENT (camera), msg) == FALSE) {
    GST_WARNING_OBJECT (camera,
        "This element has no bus, therefore no message sent!");
  }

  /* We can't send real EOS event, since it would switch the image queue
     into "draining mode". Therefore we send our own custom eos and
     catch & drop it later in queue's srcpad data probe */
  GST_DEBUG_OBJECT (camera, "sending img-eos to image queue");
  gst_camerabin_send_img_queue_custom_event (camera,
      gst_structure_new ("img-eos", NULL));

  /* Prevent video source from pushing frames until we want them */
  if (camera->block_viewfinder) {
    gst_pad_set_blocked_async (camera->pad_src_view, TRUE,
        (GstPadBlockCallback) camerabin_pad_blocked, camera);
  }

  /* our work is done, disconnect */
  gst_pad_remove_buffer_probe (pad, camera->image_captured_id);

  return TRUE;
}

/*
 * gst_camerabin_have_queue_data:
 * @pad: image queue src pad leading to image bin
 * @mini_obj: buffer or event pushed to the pad
 * @u_data: camerabin object
 *
 * Buffer probe for image queue src pad leading to image bin. It sets imgbin
 * into PLAYING mode when image buffer is passed to it. This probe also
 * monitors our internal custom events and handles them accordingly.
 */
static gboolean
gst_camerabin_have_queue_data (GstPad * pad, GstMiniObject * mini_obj,
    gpointer u_data)
{
  GstCameraBin *camera = (GstCameraBin *) u_data;
  gboolean ret = TRUE;

  if (GST_IS_BUFFER (mini_obj)) {
    GstEvent *tagevent;

    GST_LOG_OBJECT (camera, "queue sending image buffer to imgbin");

    tagevent = gst_event_new_tag (gst_tag_list_copy (camera->event_tags));
    gst_element_send_event (camera->imgbin, tagevent);
    gst_tag_list_free (camera->event_tags);
    camera->event_tags = gst_tag_list_new ();
  } else if (GST_IS_EVENT (mini_obj)) {
    const GstStructure *evs;
    GstEvent *event;

    event = GST_EVENT_CAST (mini_obj);
    evs = gst_event_get_structure (event);

    GST_LOG_OBJECT (camera, "got event %s", GST_EVENT_TYPE_NAME (event));

    if (GST_EVENT_TYPE (event) == GST_EVENT_TAG) {
      GstTagList *tlist;

      GST_DEBUG_OBJECT (camera, "queue sending taglist to image pipeline");
      gst_event_parse_tag (event, &tlist);
      gst_tag_list_insert (camera->event_tags, tlist, GST_TAG_MERGE_REPLACE);
      ret = FALSE;
    } else if (evs && gst_structure_has_name (evs, "img-filename")) {
      const gchar *fname;

      GST_DEBUG_OBJECT (camera, "queue setting image filename to imagebin");
      fname = gst_structure_get_string (evs, "filename");
      g_object_set (G_OBJECT (camera->imgbin), "filename", fname, NULL);

      /* imgbin fails to start unless the filename is set */
      gst_element_set_state (camera->imgbin, GST_STATE_PLAYING);
      GST_LOG_OBJECT (camera, "Set imgbin to PLAYING");

      ret = FALSE;
    } else if (evs && gst_structure_has_name (evs, "img-eos")) {
      GST_DEBUG_OBJECT (camera, "queue sending EOS to image pipeline");
      gst_pad_set_blocked_async (camera->pad_src_queue, TRUE,
          (GstPadBlockCallback) camerabin_pad_blocked, camera);
      gst_element_send_event (camera->imgbin, gst_event_new_eos ());
      ret = FALSE;
    }
  }

  return ret;
}

/*
 * gst_camerabin_reset_to_view_finder:
 * @camera: camerabin object
 *
 * Stop capturing and set camerabin to view finder mode.
 * Reset capture counters and flags.
 */
static void
gst_camerabin_reset_to_view_finder (GstCameraBin * camera)
{
  GstStateChangeReturn state_ret;
  GST_DEBUG_OBJECT (camera, "resetting");

  /* Set video bin to READY state */
  if (camera->active_bin == camera->vidbin) {
    state_ret = gst_element_set_state (camera->active_bin, GST_STATE_READY);
    if (state_ret == GST_STATE_CHANGE_FAILURE) {
      GST_WARNING_OBJECT (camera, "state change failed");
      gst_element_set_state (camera->active_bin, GST_STATE_NULL);
      camera->active_bin = NULL;
    }
  }

  /* Reset counters and flags */
  camera->stop_requested = FALSE;
  camera->paused = FALSE;
  camera->eos_handled = FALSE;

  /* Enable view finder mode in v4l2camsrc */
  gst_base_camera_src_set_mode (GST_BASE_CAMERA_SRC (camera->srcbin),
      MODE_PREVIEW);

  GST_DEBUG_OBJECT (camera, "reset done");
}

/*
 * gst_camerabin_do_stop:
 * @camera: camerabin object
 *
 * Raise flag to indicate to image and video bin capture stop.
 * Stopping paused video recording handled as a special case.
 * Wait for ongoing capturing to finish.
 */
static void
gst_camerabin_do_stop (GstCameraBin * camera)
{
  g_mutex_lock (camera->capture_mutex);
  if (camera->capturing) {
    GST_DEBUG_OBJECT (camera, "mark stop");
    camera->stop_requested = TRUE;

    if (camera->video_preview_caps && camera->video_preview_buffer) {
      gst_camerabin_send_preview (camera, camera->video_preview_buffer);
      gst_buffer_unref (camera->video_preview_buffer);
      camera->video_preview_buffer = NULL;
    }

    /* Take special care when stopping paused video capture */
    if ((camera->active_bin == camera->vidbin) && camera->paused) {
      /* Send eos event to video bin before setting it to playing */
      gst_camerabin_send_video_eos (camera);
      /* We must change to playing now in order to get video bin eos events
         and buffered data through and finish recording properly */
      gst_element_set_state (GST_ELEMENT (camera->vidbin), GST_STATE_PLAYING);
      camera->paused = FALSE;
    }

    GST_DEBUG_OBJECT (camera, "waiting for capturing to finish");
    g_cond_wait (camera->cond, camera->capture_mutex);
    GST_DEBUG_OBJECT (camera, "capturing finished");
  }
  g_mutex_unlock (camera->capture_mutex);
}

/*
 * gst_camerabin_default_signal_img_done:
 * @camera: camerabin object
 * @fname: filename of the recently saved image
 *
 * Default handler for #GstCameraBin::image-done signal,
 * stops always capture.
 *
 * Returns: FALSE always
 */
static gboolean
gst_camerabin_default_signal_img_done (GstCameraBin * camera,
    const gchar * fname)
{
  return FALSE;
}

static void
gst_camerabin_proxy_notify_cb (GObject * video_source, GParamSpec * pspec,
    gpointer user_data)
{
  const gchar *name = g_param_spec_get_name (pspec);
  GstElement *camerabin = GST_ELEMENT (user_data);

  GST_DEBUG_OBJECT (camerabin, "proxying %s notify from %" GST_PTR_FORMAT, name,
      GST_ELEMENT (video_source));
  g_object_notify (G_OBJECT (camerabin), name);

}

/*
 * gst_camerabin_monitor_video_source_properties:
 * @camera: camerabin object
 *
 * Monitor notify signals from video source photography interface
 * properties, and proxy the notifications to application.
 *
 */
static void
gst_camerabin_monitor_video_source_properties (GstCameraBin * camera)
{
  GstPhotography *photography;
  GParamSpec **properties;
  gchar *notify_string;
  gpointer photo_iface;
  guint i, n_properties = 0;

  GST_DEBUG_OBJECT (camera, "checking for photography interface support");
  photography =
      gst_base_camera_src_get_photography (GST_BASE_CAMERA_SRC
      (camera->srcbin));
  if (photography) {
    GST_DEBUG_OBJECT (camera,
        "start monitoring property changes in %" GST_PTR_FORMAT, photography);
    photo_iface = g_type_default_interface_ref (GST_TYPE_PHOTOGRAPHY);
    properties =
        g_object_interface_list_properties (photo_iface, &n_properties);
    if (properties) {
      for (i = 0; i < n_properties; i++) {
        notify_string =
            g_strconcat ("notify::", g_param_spec_get_name (properties[i]),
            NULL);
        GST_DEBUG_OBJECT (camera, "connecting to %" GST_PTR_FORMAT " - %s",
            photography, notify_string);
        g_signal_connect (G_OBJECT (photography), notify_string,
            (GCallback) gst_camerabin_proxy_notify_cb, camera);
        g_free (notify_string);
      }
    }
    g_type_default_interface_unref (photo_iface);
  }
}

/*
* gst_camerabin_change_viewfinder_blocking:
* @camera: camerabin object
* @blocked: new viewfinder blocking state
*
* Handle viewfinder blocking parameter change.
*/
static void
gst_camerabin_change_viewfinder_blocking (GstCameraBin * camera,
    gboolean blocked)
{
  gboolean old_value;

  GST_OBJECT_LOCK (camera);
  old_value = camera->block_viewfinder;
  camera->block_viewfinder = blocked;
  GST_OBJECT_UNLOCK (camera);

  /* "block_viewfinder" is now set and will be checked after capture */
  GST_DEBUG_OBJECT (camera, "viewfinder blocking set to %d, was %d",
      camera->block_viewfinder, old_value);

  if (old_value == blocked)
    return;

  if (!blocked && camera->pad_src_view
      && gst_pad_is_blocked (camera->pad_src_view)) {
    /* Unblock viewfinder: the pad is blocked and we need to unblock it */
    gst_pad_set_blocked_async (camera->pad_src_view, FALSE,
        (GstPadBlockCallback) camerabin_pad_blocked, camera);
  }
}

/*
 * GObject callback functions implementation
 */

static void
gst_camerabin_base_init (gpointer gclass)
{
  GstElementClass *element_class = GST_ELEMENT_CLASS (gclass);

  gst_element_class_set_details_simple (element_class, "Camera Bin",
      "Generic/Bin/Camera",
      "Handle lot of features present in DSC",
      "Nokia Corporation <multimedia@maemo.org>, "
      "Edgard Lima <edgard.lima@indt.org.br>");
}

static void
gst_camerabin_class_init (GstCameraBinClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBinClass *gstbin_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);
  gstbin_class = GST_BIN_CLASS (klass);

  /* gobject */

  gobject_class->dispose = GST_DEBUG_FUNCPTR (gst_camerabin_dispose);
  gobject_class->finalize = GST_DEBUG_FUNCPTR (gst_camerabin_finalize);

  gobject_class->set_property = gst_camerabin_set_property;
  gobject_class->get_property = gst_camerabin_get_property;

  /**
   * GstCameraBin:filename:
   *
   * Set filename for the still image capturing or video capturing.
   */

  g_object_class_install_property (gobject_class, ARG_FILENAME,
      g_param_spec_string ("filename", "Filename",
          "Filename of the image or video to save", "",
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstCameraBin:mode:
   *
   * Set the mode of operation: still image capturing or video recording.
   * Setting the mode will create and destroy image bin or video bin elements
   * according to the mode. You can set this property at any time, changing
   * the mode will stop ongoing capture.
   */

  g_object_class_install_property (gobject_class, ARG_MODE,
      g_param_spec_enum ("mode", "Mode",
          "The capture mode (still image capture or video recording)",
          GST_TYPE_CAMERABIN_MODE, DEFAULT_MODE,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstCameraBin:flags
   *
   * Control the behaviour of camerabin.
   */
  g_object_class_install_property (gobject_class, ARG_FLAGS,
      g_param_spec_flags ("flags", "Flags", "Flags to control behaviour",
          GST_TYPE_CAMERABIN_FLAGS, DEFAULT_FLAGS,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

 /**
   * GstCameraBin:mute:
   *
   * Mute audio in video recording mode.
   * Set this property only when #GstCameraBin is in READY, PAUSED or PLAYING.
   */

  g_object_class_install_property (gobject_class, ARG_MUTE,
      g_param_spec_boolean ("mute", "Mute",
          "True to mute the recording. False to record with audio",
          ARG_DEFAULT_MUTE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstCameraBin:zoom:
   *
   * Set up the zoom applied to the frames.
   * Set this property only when #GstCameraBin is in READY, PAUSED or PLAYING.
   */

  g_object_class_install_property (gobject_class, ARG_ZOOM,
      g_param_spec_int ("zoom", "Zoom",
          "The zoom. 100 for 1x, 200 for 2x and so on",
          MIN_ZOOM, MAX_ZOOM, DEFAULT_ZOOM,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstCameraBin:image-post-processing:
   *
   * Set up an element to do image post processing.
   * This property can only be set while #GstCameraBin is in NULL state.
   * The ownership of the element will be taken by #GstCameraBin.
   */
  g_object_class_install_property (gobject_class, ARG_IMAGE_POST,
      g_param_spec_object ("image-post-processing",
          "Image post processing element",
          "Image Post-Processing GStreamer element (default is NULL)",
          GST_TYPE_ELEMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   *  GstCameraBin:image-encoder:
   *
   * Set up an image encoder (for example, jpegenc or pngenc) element.
   * This property can only be set while #GstCameraBin is in NULL state.
   * The ownership of the element will be taken by #GstCameraBin.
   */

  g_object_class_install_property (gobject_class, ARG_IMAGE_ENC,
      g_param_spec_object ("image-encoder", "Image encoder",
          "Image encoder GStreamer element (default is jpegenc)",
          GST_TYPE_ELEMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   *  GstCameraBin:video-post-processing:
   *
   * Set up an element to do video post processing.
   * This property can only be set while #GstCameraBin is in NULL state.
   * The ownership of the element will be taken by #GstCameraBin.
   */

  g_object_class_install_property (gobject_class, ARG_VIDEO_POST,
      g_param_spec_object ("video-post-processing",
          "Video post processing element",
          "Video post processing GStreamer element (default is NULL)",
          GST_TYPE_ELEMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   *  GstCameraBin:video-encoder:
   *
   * Set up a video encoder element.
   * This property can only be set while #GstCameraBin is in NULL state.
   * The ownership of the element will be taken by #GstCameraBin.
   */

  g_object_class_install_property (gobject_class, ARG_VIDEO_ENC,
      g_param_spec_object ("video-encoder", "Video encoder",
          "Video encoder GStreamer element (default is theoraenc)",
          GST_TYPE_ELEMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   *  GstCameraBin:audio-encoder:
   *
   * Set up an audio encoder element.
   * This property can only be set while #GstCameraBin is in NULL state.
   * The ownership of the element will be taken by #GstCameraBin.
   */

  g_object_class_install_property (gobject_class, ARG_AUDIO_ENC,
      g_param_spec_object ("audio-encoder", "Audio encoder",
          "Audio encoder GStreamer element (default is vorbisenc)",
          GST_TYPE_ELEMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   *  GstCameraBin:video-muxer:
   *
   * Set up a video muxer element.
   * This property can only be set while #GstCameraBin is in NULL state.
   * The ownership of the element will be taken by #GstCameraBin.
   */

  g_object_class_install_property (gobject_class, ARG_VIDEO_MUX,
      g_param_spec_object ("video-muxer", "Video muxer",
          "Video muxer GStreamer element (default is oggmux)",
          GST_TYPE_ELEMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   *  GstCameraBin:viewfinder-sink:
   *
   * Set up a sink element to render frames in view finder.
   * By default "autovideosink" or DEFAULT_VIDEOSINK will be used.
   * This property can only be set while #GstCameraBin is in NULL state.
   * The ownership of the element will be taken by #GstCameraBin.
   */

  g_object_class_install_property (gobject_class, ARG_VF_SINK,
      g_param_spec_object ("viewfinder-sink", "Viewfinder sink",
          "Viewfinder sink GStreamer element (NULL = default video sink)",
          GST_TYPE_ELEMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   *  GstCameraBin:video-source:
   *
   * Set up a video source element.
   * By default "autovideosrc" or DEFAULT_VIDEOSRC will be used.
   * This property can only be set while #GstCameraBin is in NULL state.
   * The ownership of the element will be taken by #GstCameraBin.
   */

  g_object_class_install_property (gobject_class, ARG_VIDEO_SRC,
      g_param_spec_object ("video-source", "Video source element",
          "Video source GStreamer element (NULL = default video src)",
          GST_TYPE_ELEMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
  /**
   *  GstCameraBin:audio-source:
   *
   * Set up an audio source element.
   * By default "autoaudiosrc" or DEFAULT_AUDIOSRC will be used.
   * This property can only be set while #GstCameraBin is in NULL state.
   * The ownership of the element will be taken by #GstCameraBin.
   */

  g_object_class_install_property (gobject_class, ARG_AUDIO_SRC,
      g_param_spec_object ("audio-source", "Audio source element",
          "Audio source GStreamer element (NULL = default audio src)",
          GST_TYPE_ELEMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   *  GstCameraBin:video-source-filter:
   *
   * Set up optional video filter element, all frames from video source
   * will be processed by this element. e.g. An application might add
   * image enhancers/parameter adjustment filters here to improve captured
   * image/video results, or add analyzers to give feedback on capture
   * the application.
   * This property can only be set while #GstCameraBin is in NULL state.
   * The ownership of the element will be taken by #GstCameraBin.
   */

  g_object_class_install_property (gobject_class, ARG_VIDEO_SOURCE_FILTER,
      g_param_spec_object ("video-source-filter", "video source filter element",
          "Optional video filter GStreamer element, filters all frames from"
          "the video source", GST_TYPE_ELEMENT,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstCameraBin:video-source-caps:
   *
   * The allowed modes of operation of the video source. Have in mind that it
   * doesn't mean #GstCameraBin can operate in all those modes,
   * it depends also on the other elements in the pipeline. Remember to
   * gst_caps_unref after using it.
   */

  g_object_class_install_property (gobject_class, ARG_INPUT_CAPS,
      g_param_spec_boxed ("video-source-caps", "Video source caps",
          "The allowed modes of the video source operation",
          GST_TYPE_CAPS, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS));

  /**
   * GstCameraBin:filter-caps:
   *
   * Caps applied to capsfilter element after videosrc [ ! ffmpegcsp ].
   * You can use this e.g. to make sure video color format matches with
   * encoders and other elements configured to camerabin and/or change
   * resolution and frame rate.
   */

  g_object_class_install_property (gobject_class, ARG_FILTER_CAPS,
      g_param_spec_boxed ("filter-caps", "Filter caps",
          "Filter video data coming from videosrc element",
          GST_TYPE_CAPS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstCameraBin:preview-caps:
   *
   * If application wants to receive a preview image, it needs to 
   * set this property to depict the desired image format caps. When
   * this property is not set (NULL), message containing the preview
   * image is not sent.
   */

  g_object_class_install_property (gobject_class, ARG_PREVIEW_CAPS,
      g_param_spec_boxed ("preview-caps", "Preview caps",
          "Caps defining the preview image format",
          GST_TYPE_CAPS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstCameraBin:viewfinder-filter:
   * Set up viewfinder filter element, all frames going to viewfinder sink
   * element will be processed by this element.
   * Applications can use this to overlay text/images in the screen, or
   * plug facetracking algorithms, for example.
   * This property can only be set while #GstCameraBin is in NULL state.
   * The ownership of the element will be taken by #GstCameraBin.
   */

  g_object_class_install_property (gobject_class, ARG_VIEWFINDER_FILTER,
      g_param_spec_object ("viewfinder-filter", "viewfinder filter element",
          "viewfinder filter GStreamer element",
          GST_TYPE_ELEMENT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstCameraBin:block-after-capture:
   *
   * Block viewfinder after capture.
   * When set to TRUE, camerabin will freeze the viewfinder after capturing.
   * This is useful if application wants to display the preview image
   * and running the viewfinder at the same time would be just a waste of
   * CPU cycles. Viewfinder can be enabled again by setting this property to
   * FALSE.
   */

  g_object_class_install_property (gobject_class, ARG_BLOCK_VIEWFINDER,
      g_param_spec_boolean ("block-after-capture",
          "Block viewfinder after capture",
          "Block viewfinder after capturing an image or video",
          DEFAULT_BLOCK_VIEWFINDER,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

   /**
    * GstCameraBin:image-capture-width:
    *
    * The width to be used when capturing still images. If 0, the
    * viewfinder's width will be used.
    */
  g_object_class_install_property (gobject_class, ARG_IMAGE_CAPTURE_WIDTH,
      g_param_spec_int ("image-capture-width",
          "The width used for image capture",
          "The width used for image capture", 0, G_MAXINT16,
          DEFAULT_CAPTURE_WIDTH, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

   /**
    * GstCameraBin:image-capture-height:
    *
    * The height to be used when capturing still images. If 0, the
    * viewfinder's height will be used.
    */
  g_object_class_install_property (gobject_class, ARG_IMAGE_CAPTURE_HEIGHT,
      g_param_spec_int ("image-capture-height",
          "The height used for image capture",
          "The height used for image capture", 0, G_MAXINT16,
          DEFAULT_CAPTURE_HEIGHT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

   /**
    * GstCameraBin:video-capture-width:
    *
    * The width to be used when capturing video.
    */
  g_object_class_install_property (gobject_class, ARG_VIDEO_CAPTURE_WIDTH,
      g_param_spec_int ("video-capture-width",
          "The width used for video capture",
          "The width used for video capture", 0, G_MAXINT16,
          DEFAULT_CAPTURE_WIDTH, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstCameraBin:video-capture-height:
   *
   * The height to be used when capturing video.
   */
  g_object_class_install_property (gobject_class, ARG_VIDEO_CAPTURE_HEIGHT,
      g_param_spec_int ("video-capture-height",
          "The height used for video capture",
          "The height used for video capture", 0, G_MAXINT16,
          DEFAULT_CAPTURE_HEIGHT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstCameraBin:video-capture-framerate:
   *
   * The framerate to be used when capturing video.
   */
  g_object_class_install_property (gobject_class, ARG_VIDEO_CAPTURE_FRAMERATE,
      gst_param_spec_fraction ("video-capture-framerate",
          "The framerate used for video capture",
          "The framerate used for video capture", 0, 1, G_MAXINT32, 1,
          DEFAULT_FPS_N, DEFAULT_FPS_D,
          G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * GstCameraBin::capture-start:
   * @camera: the camera bin element
   *
   * Starts image capture or video recording depending on the Mode.
   * If there is a capture already going on, does nothing.
   * Resumes video recording if it has been paused.
   */

  camerabin_signals[CAPTURE_START_SIGNAL] =
      g_signal_new ("capture-start",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_STRUCT_OFFSET (GstCameraBinClass, capture_start),
      NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  /**
   * GstCameraBin::capture-stop:
   * @camera: the camera bin element
   *
   * Stops still image preview, continuous image capture and video
   * recording and returns to the view finder mode.
   */

  camerabin_signals[CAPTURE_STOP_SIGNAL] =
      g_signal_new ("capture-stop",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_STRUCT_OFFSET (GstCameraBinClass, capture_stop),
      NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  /**
   * GstCameraBin::capture-pause:
   * @camera: the camera bin element
   *
   * Pauses video recording or resumes paused video recording.
   * If in image mode or not recording, does nothing.
   */

  camerabin_signals[CAPTURE_PAUSE_SIGNAL] =
      g_signal_new ("capture-pause",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_STRUCT_OFFSET (GstCameraBinClass, capture_pause),
      NULL, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  /**
   * GstCameraBin::set-video-resolution-fps:
   * @camera: the camera bin element
   * @width: number of horizontal pixels
   * @height: number of vertical pixels
   * @fps_n: frames per second numerator
   * @fps_d: frames per second denominator
   *
   * Changes the frame resolution and frames per second of the video source.
   * The application must be aware of the resolutions supported by the camera.
   * Supported resolutions and frame rates can be get using input-caps property.
   *
   * Setting @fps_n or @fps_d to 0 configures maximum framerate for the
   * given resolution, unless in night mode when minimum is configured.
   *
   * This is the same as setting the 'video-capture-width',
   * 'video-capture-height' and 'video-capture-framerate' properties, but it
   * already updates the caps to force use this resolution and framerate.
   */

  camerabin_signals[SET_VIDEO_RESOLUTION_FPS_SIGNAL] =
      g_signal_new ("set-video-resolution-fps",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_STRUCT_OFFSET (GstCameraBinClass, set_video_resolution_fps),
      NULL, NULL, __gst_camerabin_marshal_VOID__INT_INT_INT_INT, G_TYPE_NONE, 4,
      G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT);

  /**
   * GstCameraBin::set-image-resolution:
   * @camera: the camera bin element
   * @width: number of horizontal pixels
   * @height: number of vertical pixels
   *
   * Changes the resolution used for still image capture.
   * Does not affect view finder mode and video recording.
   *
   * This actually sets the 'image-capture-width' and 'image-capture-height'
   * properties.
   */

  camerabin_signals[SET_IMAGE_RESOLUTION_SIGNAL] =
      g_signal_new ("set-image-resolution",
      G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
      G_STRUCT_OFFSET (GstCameraBinClass, set_image_resolution),
      NULL, NULL, __gst_camerabin_marshal_VOID__INT_INT, G_TYPE_NONE, 2,
      G_TYPE_INT, G_TYPE_INT);

  /**
   * GstCameraBin::image-done:
   * @camera: the camera bin element
   * @filename: the name of the file just saved
   *
   * Signal emitted when the file has just been saved.
   *
   * Don't call any #GstCameraBin method from this signal, if you do so there
   * will be a deadlock.
   */

  camerabin_signals[IMG_DONE_SIGNAL] =
      g_signal_new ("image-done", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST, G_STRUCT_OFFSET (GstCameraBinClass, img_done),
      g_signal_accumulator_true_handled, NULL,
      __gst_camerabin_marshal_BOOLEAN__STRING, G_TYPE_BOOLEAN, 1,
      G_TYPE_STRING);

  gst_camerabin_override_photo_properties (gobject_class);

  klass->capture_start = gst_camerabin_capture_start;
  klass->capture_stop = gst_camerabin_capture_stop;
  klass->capture_pause = gst_camerabin_capture_pause;
  klass->set_video_resolution_fps = gst_camerabin_set_video_resolution_fps;
  klass->set_image_resolution = gst_camerabin_set_image_resolution;

  klass->img_done = gst_camerabin_default_signal_img_done;

  /* gstelement */

  gstelement_class->change_state =
      GST_DEBUG_FUNCPTR (gst_camerabin_change_state);

  /* gstbin */
  /* override handle_message to peek when video or image bin reaches eos */
  gstbin_class->handle_message =
      GST_DEBUG_FUNCPTR (gst_camerabin_handle_message_func);

}

/* initialize the new element
 * instantiate pads and add them to element
 * set functions
 * initialize structure
 */
static void
gst_camerabin_init (GstCameraBin * camera, GstCameraBinClass * gclass)
{
  /* GstElementClass *klass = GST_ELEMENT_GET_CLASS (camera); */

  camera->filename = g_string_new ("");
  camera->mode = DEFAULT_MODE;
  camera->flags = DEFAULT_FLAGS;
  camera->stop_requested = FALSE;
  camera->paused = FALSE;
  camera->capturing = FALSE;
  camera->eos_handled = FALSE;

  camera->event_tags = gst_tag_list_new ();

  /* concurrency control */
  camera->capture_mutex = g_mutex_new ();
  camera->cond = g_cond_new ();

  /* pad names for output and input selectors */
  camera->pad_src_view = NULL;
  camera->pad_src_img = NULL;
  camera->pad_src_vid = NULL;

  camera->video_preview_buffer = NULL;

  /* camera src bin */
  camera->srcbin = g_object_new (GST_TYPE_V4L2_CAMERA_SRC, NULL);
  gst_object_ref (camera->srcbin);      /* XXX why is this? */

  /* image capture bin */
  camera->imgbin = g_object_new (GST_TYPE_CAMERABIN_IMAGE, NULL);
  gst_object_ref (camera->imgbin);

  /* video capture bin */
  camera->vidbin = g_object_new (GST_TYPE_CAMERABIN_VIDEO, NULL);
  gst_object_ref (camera->vidbin);

  /* view finder elements */
  camera->view_scale = NULL;
  camera->aspect_filter = NULL;
  camera->view_sink = NULL;

  camera->app_vf_sink = NULL;
  camera->app_viewfinder_filter = NULL;

  camera->active_bin = NULL;

  memset (&camera->photo_settings, 0, sizeof (GstPhotoSettings));
}

static void
gst_camerabin_dispose (GObject * object)
{
  GstCameraBin *camera;

  camera = GST_CAMERABIN (object);

  GST_DEBUG_OBJECT (camera, "disposing");

  gst_element_set_state (camera->imgbin, GST_STATE_NULL);
  gst_object_unref (camera->imgbin);

  gst_element_set_state (camera->vidbin, GST_STATE_NULL);
  gst_object_unref (camera->vidbin);

  if (camera->preview_pipeline) {
    gst_camerabin_preview_destroy_pipeline (camera, camera->preview_pipeline);
    camera->preview_pipeline = NULL;
  }
  if (camera->video_preview_pipeline) {
    gst_camerabin_preview_destroy_pipeline (camera,
        camera->video_preview_pipeline);
    camera->video_preview_pipeline = NULL;
  }

  camerabin_destroy_elements (camera);
  camerabin_dispose_elements (camera);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gst_camerabin_finalize (GObject * object)
{
  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gst_camerabin_override_photo_properties (GObjectClass * gobject_class)
{
  g_object_class_override_property (gobject_class, ARG_WB_MODE,
      GST_PHOTOGRAPHY_PROP_WB_MODE);

  g_object_class_override_property (gobject_class, ARG_COLOUR_TONE,
      GST_PHOTOGRAPHY_PROP_COLOUR_TONE);

  g_object_class_override_property (gobject_class, ARG_SCENE_MODE,
      GST_PHOTOGRAPHY_PROP_SCENE_MODE);

  g_object_class_override_property (gobject_class, ARG_FLASH_MODE,
      GST_PHOTOGRAPHY_PROP_FLASH_MODE);

  g_object_class_override_property (gobject_class, ARG_CAPABILITIES,
      GST_PHOTOGRAPHY_PROP_CAPABILITIES);

  g_object_class_override_property (gobject_class, ARG_EV_COMP,
      GST_PHOTOGRAPHY_PROP_EV_COMP);

  g_object_class_override_property (gobject_class, ARG_ISO_SPEED,
      GST_PHOTOGRAPHY_PROP_ISO_SPEED);

  g_object_class_override_property (gobject_class, ARG_APERTURE,
      GST_PHOTOGRAPHY_PROP_APERTURE);

  g_object_class_override_property (gobject_class, ARG_EXPOSURE,
      GST_PHOTOGRAPHY_PROP_EXPOSURE);

  g_object_class_override_property (gobject_class,
      ARG_IMAGE_CAPTURE_SUPPORTED_CAPS,
      GST_PHOTOGRAPHY_PROP_IMAGE_CAPTURE_SUPPORTED_CAPS);

  g_object_class_override_property (gobject_class, ARG_FLICKER_MODE,
      GST_PHOTOGRAPHY_PROP_FLICKER_MODE);

  g_object_class_override_property (gobject_class, ARG_FOCUS_MODE,
      GST_PHOTOGRAPHY_PROP_FOCUS_MODE);
}

static void
gst_camerabin_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstCameraBin *camera = GST_CAMERABIN (object);

  if (gst_camerabin_photography_set_property (camera, prop_id, value))
    return;

  switch (prop_id) {
    case ARG_MUTE:
      gst_camerabin_video_set_mute (GST_CAMERABIN_VIDEO (camera->vidbin),
          g_value_get_boolean (value));
      break;
    case ARG_MODE:
      gst_camerabin_change_mode (camera, g_value_get_enum (value));
      break;
    case ARG_FLAGS:
      gst_camerabin_set_flags (camera, g_value_get_flags (value));
      break;
    case ARG_FILENAME:
      gst_camerabin_change_filename (camera, g_value_get_string (value));
      break;
    case ARG_VIDEO_POST:
      if (GST_STATE (camera->vidbin) != GST_STATE_NULL) {
        GST_WARNING_OBJECT (camera,
            "can't use set element until next video bin NULL to READY state change");
      }
      gst_camerabin_video_set_post (GST_CAMERABIN_VIDEO (camera->vidbin),
          g_value_get_object (value));
      break;
    case ARG_VIDEO_ENC:
      if (GST_STATE (camera->vidbin) != GST_STATE_NULL) {
        GST_WARNING_OBJECT (camera,
            "can't use set element until next video bin NULL to READY state change");
      }
      gst_camerabin_video_set_video_enc (GST_CAMERABIN_VIDEO (camera->vidbin),
          g_value_get_object (value));
      break;
    case ARG_AUDIO_ENC:
      if (GST_STATE (camera->vidbin) != GST_STATE_NULL) {
        GST_WARNING_OBJECT (camera,
            "can't use set element until next video bin NULL to READY state change");
      }
      gst_camerabin_video_set_audio_enc (GST_CAMERABIN_VIDEO (camera->vidbin),
          g_value_get_object (value));
      break;
    case ARG_VIDEO_MUX:
      if (GST_STATE (camera->vidbin) != GST_STATE_NULL) {
        GST_WARNING_OBJECT (camera,
            "can't use set element until next video bin NULL to READY state change");
      }
      gst_camerabin_video_set_muxer (GST_CAMERABIN_VIDEO (camera->vidbin),
          g_value_get_object (value));
      break;
    case ARG_IMAGE_POST:
      if (GST_STATE (camera->imgbin) != GST_STATE_NULL) {
        GST_WARNING_OBJECT (camera,
            "can't use set element until next image bin NULL to READY state change");
      }
      gst_camerabin_image_set_postproc (GST_CAMERABIN_IMAGE (camera->imgbin),
          g_value_get_object (value));
      break;
    case ARG_IMAGE_ENC:
      if (GST_STATE (camera->imgbin) != GST_STATE_NULL) {
        GST_WARNING_OBJECT (camera,
            "can't use set element until next image bin NULL to READY state change");
      }
      gst_camerabin_image_set_encoder (GST_CAMERABIN_IMAGE (camera->imgbin),
          g_value_get_object (value));
      break;
    case ARG_VF_SINK:
      if (GST_STATE (camera) != GST_STATE_NULL) {
        GST_ELEMENT_ERROR (camera, CORE, FAILED,
            ("camerabin must be in NULL state when setting the view finder element"),
            (NULL));
      } else {
        if (camera->app_vf_sink)
          gst_object_unref (camera->app_vf_sink);
        camera->app_vf_sink = g_value_get_object (value);
        if (camera->app_vf_sink)
          gst_object_ref (camera->app_vf_sink);
      }
      break;
    case ARG_AUDIO_SRC:
      if (GST_STATE (camera->vidbin) != GST_STATE_NULL) {
        GST_WARNING_OBJECT (camera,
            "can't use set element until next video bin NULL to READY state change");
      }
      gst_camerabin_video_set_audio_src (GST_CAMERABIN_VIDEO (camera->vidbin),
          g_value_get_object (value));
      break;
    case ARG_PREVIEW_CAPS:
    {
      GstElement **prev_pipe = NULL;
      GstCaps **prev_caps = NULL;
      GstCaps *new_caps = NULL;

      if (camera->mode == MODE_IMAGE) {
        prev_pipe = &camera->preview_pipeline;
        prev_caps = &camera->preview_caps;
      } else if (camera->mode == MODE_VIDEO) {
        prev_pipe = &camera->video_preview_pipeline;
        prev_caps = &camera->video_preview_caps;
      }

      new_caps = (GstCaps *) gst_value_get_caps (value);

      if (prev_caps && !gst_caps_is_equal (*prev_caps, new_caps)) {
        GST_DEBUG_OBJECT (camera,
            "setting preview caps: %" GST_PTR_FORMAT, new_caps);
        if (*prev_pipe) {
          gst_camerabin_preview_destroy_pipeline (camera, *prev_pipe);
          *prev_pipe = NULL;
        }
        GST_OBJECT_LOCK (camera);
        gst_caps_replace (prev_caps, new_caps);
        GST_OBJECT_UNLOCK (camera);

        if (new_caps && !gst_caps_is_any (new_caps) &&
            !gst_caps_is_empty (new_caps)) {
          *prev_pipe = gst_camerabin_preview_create_pipeline (camera, new_caps);
        }
      }
      break;
    }
    case ARG_VIEWFINDER_FILTER:
      if (GST_STATE (camera) != GST_STATE_NULL) {
        GST_ELEMENT_ERROR (camera, CORE, FAILED,
            ("camerabin must be in NULL state when setting the viewfinder filter element"),
            (NULL));
      } else {
        if (camera->app_viewfinder_filter)
          gst_object_unref (camera->app_viewfinder_filter);
        camera->app_viewfinder_filter = g_value_dup_object (value);
      }
      break;
    case ARG_BLOCK_VIEWFINDER:
      gst_camerabin_change_viewfinder_blocking (camera,
          g_value_get_boolean (value));
      break;
    case ARG_ZOOM:
    case ARG_FILTER_CAPS:
    case ARG_VIDEO_SOURCE_FILTER:
    case ARG_VIDEO_SRC:
    case ARG_IMAGE_CAPTURE_WIDTH:
    case ARG_IMAGE_CAPTURE_HEIGHT:
    case ARG_VIDEO_CAPTURE_WIDTH:
    case ARG_VIDEO_CAPTURE_HEIGHT:
    case ARG_VIDEO_CAPTURE_FRAMERATE:
      /* delegate these to camera srcbin */
      g_object_set_property (G_OBJECT (camera->srcbin),
          g_param_spec_get_name (pspec), value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_camerabin_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstCameraBin *camera = GST_CAMERABIN (object);

  if (gst_camerabin_photography_get_property (camera, prop_id, value))
    return;

  switch (prop_id) {
    case ARG_FILENAME:
      g_value_set_string (value, camera->filename->str);
      break;
    case ARG_MODE:
      g_value_set_enum (value, camera->mode);
      break;
    case ARG_FLAGS:
      g_value_set_flags (value, camera->flags);
      break;
    case ARG_MUTE:
      g_value_set_boolean (value,
          gst_camerabin_video_get_mute (GST_CAMERABIN_VIDEO (camera->vidbin)));
      break;
    case ARG_IMAGE_POST:
      g_value_set_object (value,
          gst_camerabin_image_get_postproc (GST_CAMERABIN_IMAGE
              (camera->imgbin)));
      break;
    case ARG_IMAGE_ENC:
      g_value_set_object (value,
          gst_camerabin_image_get_encoder (GST_CAMERABIN_IMAGE
              (camera->imgbin)));
      break;
    case ARG_VIDEO_POST:
      g_value_set_object (value,
          gst_camerabin_video_get_post (GST_CAMERABIN_VIDEO (camera->vidbin)));
      break;
    case ARG_VIDEO_ENC:
      g_value_set_object (value,
          gst_camerabin_video_get_video_enc (GST_CAMERABIN_VIDEO
              (camera->vidbin)));
      break;
    case ARG_AUDIO_ENC:
      g_value_set_object (value,
          gst_camerabin_video_get_audio_enc (GST_CAMERABIN_VIDEO
              (camera->vidbin)));
      break;
    case ARG_VIDEO_MUX:
      g_value_set_object (value,
          gst_camerabin_video_get_muxer (GST_CAMERABIN_VIDEO (camera->vidbin)));
      break;
    case ARG_VF_SINK:
      if (camera->view_sink)
        g_value_set_object (value, camera->view_sink);
      else
        g_value_set_object (value, camera->app_vf_sink);
      break;
    case ARG_AUDIO_SRC:
      g_value_set_object (value,
          gst_camerabin_video_get_audio_src (GST_CAMERABIN_VIDEO
              (camera->vidbin)));
      break;
    case ARG_INPUT_CAPS:
      gst_value_set_caps (value,
          gst_base_camera_src_get_allowed_input_caps (GST_BASE_CAMERA_SRC
              (camera->srcbin)));
      break;
    case ARG_PREVIEW_CAPS:
      if (camera->mode == MODE_IMAGE)
        gst_value_set_caps (value, camera->preview_caps);
      else if (camera->mode == MODE_VIDEO)
        gst_value_set_caps (value, camera->video_preview_caps);
      break;
    case ARG_VIEWFINDER_FILTER:
      g_value_set_object (value, camera->app_viewfinder_filter);
      break;
    case ARG_BLOCK_VIEWFINDER:
      g_value_set_boolean (value, camera->block_viewfinder);
      break;
    case ARG_ZOOM:
    case ARG_FILTER_CAPS:
    case ARG_VIDEO_SOURCE_FILTER:
    case ARG_VIDEO_SRC:
    case ARG_IMAGE_CAPTURE_WIDTH:
    case ARG_IMAGE_CAPTURE_HEIGHT:
    case ARG_VIDEO_CAPTURE_WIDTH:
    case ARG_VIDEO_CAPTURE_HEIGHT:
    case ARG_VIDEO_CAPTURE_FRAMERATE:
      /* delegate these to camera srcbin */
      g_object_get_property (G_OBJECT (camera->srcbin),
          g_param_spec_get_name (pspec), value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/*
 * GstElement functions implementation
 */

static GstStateChangeReturn
gst_camerabin_change_state (GstElement * element, GstStateChange transition)
{
  GstCameraBin *camera = GST_CAMERABIN (element);
  GstStateChangeReturn ret;

  GST_DEBUG_OBJECT (element, "changing state: %s -> %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)));

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!camerabin_create_elements (camera)) {
        ret = GST_STATE_CHANGE_FAILURE;
        goto done;
      }
      /* Lock to control image and video bin state separately
         from view finder */
      gst_element_set_locked_state (camera->imgbin, TRUE);
      gst_element_set_locked_state (camera->vidbin, TRUE);
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      camerabin_setup_src_elements (camera);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      /* If using autovideosink, set view finder sink properties
         now that actual sink has been created. */
      camerabin_setup_view_elements (camera);
      break;
    case GST_STATE_CHANGE_READY_TO_NULL:
      gst_element_set_locked_state (camera->imgbin, FALSE);
      gst_element_set_locked_state (camera->vidbin, FALSE);
      break;
    default:
      break;
  }

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);

  GST_DEBUG_OBJECT (element, "after chaining up: %s -> %s = %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)),
      gst_element_state_change_return_get_name (ret));

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_READY:{
      GstPhotography *photography;
      g_mutex_lock (camera->capture_mutex);
      if (camera->capturing) {
        GST_WARNING_OBJECT (camera, "was capturing when changing to READY");
        camera->capturing = FALSE;
        /* Reset capture and don't wait for capturing to finish properly.
           Proper capturing should have been finished before going to READY. */
        gst_camerabin_reset_to_view_finder (camera);
        g_cond_signal (camera->cond);
      }
      g_mutex_unlock (camera->capture_mutex);
      photography =
          gst_base_camera_src_get_photography (GST_BASE_CAMERA_SRC
          (camera->srcbin));
      if (photography) {
        g_signal_handlers_disconnect_by_func (photography,
            gst_camerabin_proxy_notify_cb, camera);
      }
      break;
    }
    case GST_STATE_CHANGE_READY_TO_NULL:
      camerabin_destroy_elements (camera);
      break;
      /* In some error situation camerabin may end up being still in NULL
         state so we must take care of destroying elements. */
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (ret == GST_STATE_CHANGE_FAILURE)
        camerabin_destroy_elements (camera);
      break;
    default:
      break;
  }

done:
  GST_DEBUG_OBJECT (element, "changed state: %s -> %s = %s",
      gst_element_state_get_name (GST_STATE_TRANSITION_CURRENT (transition)),
      gst_element_state_get_name (GST_STATE_TRANSITION_NEXT (transition)),
      gst_element_state_change_return_get_name (ret));

  return ret;
}

static gboolean
gst_camerabin_imgbin_finished (gpointer u_data)
{
  GstCameraBin *camera = GST_CAMERABIN (u_data);
  gchar *filename = NULL;

  /* Get the filename of the finished image */
  g_object_get (G_OBJECT (camera->imgbin), "filename", &filename, NULL);

  GST_DEBUG_OBJECT (camera, "Image encoding finished");

  /* Close the file of saved image */
  gst_element_set_state (camera->imgbin, GST_STATE_READY);
  GST_DEBUG_OBJECT (camera, "Image pipeline set to READY");

  /* Send image-done signal */
  gst_camerabin_image_capture_continue (camera, filename);
  g_free (filename);

  /* Set image bin back to PAUSED so that buffer-allocs don't fail */
  gst_element_set_state (camera->imgbin, GST_STATE_PAUSED);

  /* Unblock image queue pad to process next buffer */
  gst_pad_set_blocked_async (camera->pad_src_queue, FALSE,
      (GstPadBlockCallback) camerabin_pad_blocked, camera);
  GST_DEBUG_OBJECT (camera, "Queue srcpad unblocked");

  /* disconnect automatically */
  return FALSE;
}

/*
 * GstBin functions implementation
 */

/* Peek eos messages but don't interfere with bin msg handling */
static void
gst_camerabin_handle_message_func (GstBin * bin, GstMessage * msg)
{
  GstCameraBin *camera = GST_CAMERABIN (bin);

  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_EOS:
      if (GST_MESSAGE_SRC (msg) == GST_OBJECT (camera->vidbin)) {
        /* Video eos */
        GST_DEBUG_OBJECT (camera,
            "got video eos message, stopping video capture");
        g_mutex_lock (camera->capture_mutex);
        camera->capturing = FALSE;
        g_cond_signal (camera->cond);
        g_mutex_unlock (camera->capture_mutex);
      } else if (GST_MESSAGE_SRC (msg) == GST_OBJECT (camera->imgbin)) {
        /* Image eos */
        GST_DEBUG_OBJECT (camera, "got image eos message");
        /* Calling callback directly will deadlock in
           imagebin state change functions */
        g_idle_add_full (G_PRIORITY_HIGH, gst_camerabin_imgbin_finished, camera,
            NULL);
      }
      break;
    case GST_MESSAGE_ERROR:
      GST_DEBUG_OBJECT (camera, "error from child %" GST_PTR_FORMAT,
          GST_MESSAGE_SRC (msg));
      g_mutex_lock (camera->capture_mutex);
      if (camera->capturing) {
        camera->capturing = FALSE;
        g_cond_signal (camera->cond);
      }
      g_mutex_unlock (camera->capture_mutex);
      break;
    default:
      break;
  }
  GST_BIN_CLASS (parent_class)->handle_message (bin, msg);
}

/*
 * Action signal function implementation
 */

static void
gst_camerabin_capture_start (GstCameraBin * camera)
{

  GST_INFO_OBJECT (camera, "starting capture");
  if (camera->paused) {
    gst_camerabin_capture_pause (camera);
    return;
  }

  if (!camera->active_bin) {
    GST_INFO_OBJECT (camera, "mode not explicitly set by application");
    gst_camerabin_change_mode (camera, camera->mode);
    if (!camera->active_bin) {
      GST_ELEMENT_ERROR (camera, CORE, FAILED,
          ("starting capture failed"), (NULL));
    }
  }

  if (g_str_equal (camera->filename->str, "")) {
    GST_ELEMENT_ERROR (camera, CORE, FAILED,
        ("set filename before starting capture"), (NULL));
    return;
  }

  g_mutex_lock (camera->capture_mutex);
  if (camera->capturing) {
    GST_WARNING_OBJECT (camera, "capturing \"%s\" ongoing, set new filename",
        camera->filename->str);
    /* FIXME: we need to send something more to the app, so that it does not for
     * for image-done */
    g_mutex_unlock (camera->capture_mutex);
    return;
  }
  g_mutex_unlock (camera->capture_mutex);

  if (camera->active_bin) {
    if (camera->active_bin == camera->imgbin) {
      GST_INFO_OBJECT (camera, "starting image capture");
      gst_camerabin_start_image_capture (camera);
    } else if (camera->active_bin == camera->vidbin) {
      GST_INFO_OBJECT (camera,
          "setting video filename and starting video capture");
      g_object_set (G_OBJECT (camera->active_bin), "filename",
          camera->filename->str, NULL);
      gst_camerabin_start_video_recording (camera);
    }
  }
}

static void
gst_camerabin_capture_stop (GstCameraBin * camera)
{
  if (camera->active_bin == camera->vidbin) {
    GST_INFO_OBJECT (camera, "stopping video capture");
    gst_camerabin_do_stop (camera);
    gst_camerabin_reset_to_view_finder (camera);
  } else {
    GST_INFO_OBJECT (camera, "stopping image capture isn't needed");
  }
}

static void
gst_camerabin_capture_pause (GstCameraBin * camera)
{
  if (camera->active_bin == camera->vidbin) {
    if (!camera->paused) {
      GST_INFO_OBJECT (camera, "pausing capture");

      /* Bring all camerabin elements to PAUSED */
      gst_element_set_locked_state (camera->vidbin, FALSE);
      gst_element_set_state (GST_ELEMENT (camera), GST_STATE_PAUSED);

      /* Switch to view finder mode */
      gst_base_camera_src_set_mode (GST_BASE_CAMERA_SRC (camera->srcbin),
          MODE_PREVIEW);

      /* Set view finder to PLAYING and leave videobin PAUSED */
      gst_element_set_locked_state (camera->vidbin, TRUE);
      gst_element_set_state (GST_ELEMENT (camera), GST_STATE_PLAYING);

      camera->paused = TRUE;
    } else {
      GST_INFO_OBJECT (camera, "unpausing capture");

      /* Bring all camerabin elements to PAUSED */
      gst_element_set_state (GST_ELEMENT (camera), GST_STATE_PAUSED);

      /* Switch to video recording mode */
      gst_base_camera_src_set_mode (GST_BASE_CAMERA_SRC (camera->srcbin),
          MODE_VIDEO);

      /* Bring all camerabin elements to PLAYING */
      gst_element_set_locked_state (camera->vidbin, FALSE);
      gst_element_set_state (GST_ELEMENT (camera), GST_STATE_PLAYING);
      gst_element_set_locked_state (camera->vidbin, TRUE);

      camera->paused = FALSE;
    }
    GST_DEBUG_OBJECT (camera, "pause done");
  } else {
    GST_WARNING ("pausing in image capture mode disabled");
  }
}

static void
gst_camerabin_set_video_resolution_fps (GstCameraBin * camera, gint width,
    gint height, gint fps_n, gint fps_d)
{
  g_object_set (camera, "video-capture-width", width, "video-capture-height",
      height, "video-capture-framerate", fps_n, fps_d, NULL);

  reset_video_capture_caps (camera);
}

static void
gst_camerabin_set_image_resolution (GstCameraBin * camera, gint width,
    gint height)
{
  g_object_set (camera, "image-capture-width", (guint16) width,
      "image-capture-height", (guint16) height, NULL);
}

/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and pad templates
 * register the features
 */
static gboolean
plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (gst_camerabin_debug, "camerabin", 0, "CameraBin");

  return gst_element_register (plugin, "camerabin",
      GST_RANK_NONE, GST_TYPE_CAMERABIN) &&
      gst_element_register (plugin, "v4l2camerasrc",
      GST_RANK_NONE, GST_TYPE_V4L2_CAMERA_SRC);
}

/* this is the structure that gstreamer looks for to register plugins
 */
GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    "camerabin",
    "High level api for DC (Digital Camera) application",
    plugin_init, VERSION, "LGPL", GST_PACKAGE_NAME, GST_PACKAGE_ORIGIN)
