/*
 * GStreamer
 * Copyright (C) 2008 Nokia Corporation <multimedia@maemo.org>
 *
 * Photography interface implementation for camerabin.
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
#include <config.h>
#endif

#include <string.h>
#include <gst/camerasrc/gstbasecamerasrc.h>
#include "gstcamerabinphotography.h"
#include "gstcamerabin.h"
#include <gst/camerasrc/gstcamerabin-enum.h>

GST_DEBUG_CATEGORY_STATIC (camerabinphoto_debug);
#define GST_CAT_DEFAULT camerabinphoto_debug

#define GET_PHOTOGRAPHY(cam) \
		gst_base_camera_src_get_photography (GST_BASE_CAMERA_SRC ((cam)->srcbin));

static void
gst_camerabin_handle_scene_mode (GstCameraBin * camera,
    GstSceneMode scene_mode);

static gboolean
gst_camerabin_set_ev_compensation (GstPhotography * photo,
    gfloat ev_compensation)
{
  GstCameraBin *camera;
  GstPhotography *photography;
  gboolean ret = TRUE;

  g_return_val_if_fail (photo != NULL, FALSE);

  camera = GST_CAMERABIN (photo);

  /* Cache the setting */
  camera->photo_settings.ev_compensation = ev_compensation;

  photography = GET_PHOTOGRAPHY (camera);

  if (photography) {
    ret = gst_photography_set_ev_compensation (photography, ev_compensation);
  }
  return ret;
}

static gboolean
gst_camerabin_get_ev_compensation (GstPhotography * photo,
    gfloat * ev_compensation)
{
  GstCameraBin *camera;
  GstPhotography *photography;
  gboolean ret = FALSE;

  g_return_val_if_fail (photo != NULL, FALSE);

  camera = GST_CAMERABIN (photo);

  photography = GET_PHOTOGRAPHY (camera);

  if (photography) {
    ret = gst_photography_get_ev_compensation (photography, ev_compensation);
  }
  return ret;
}

static gboolean
gst_camerabin_set_iso_speed (GstPhotography * photo, guint iso_speed)
{
  GstCameraBin *camera;
  GstPhotography *photography;
  gboolean ret = TRUE;

  g_return_val_if_fail (photo != NULL, FALSE);

  camera = GST_CAMERABIN (photo);

  /* Cache the setting */
  camera->photo_settings.iso_speed = iso_speed;

  photography = GET_PHOTOGRAPHY (camera);

  if (photography) {
    ret = gst_photography_set_iso_speed (photography, iso_speed);
  }
  return ret;
}

static gboolean
gst_camerabin_get_iso_speed (GstPhotography * photo, guint * iso_speed)
{
  GstCameraBin *camera;
  GstPhotography *photography;
  gboolean ret = FALSE;

  g_return_val_if_fail (photo != NULL, FALSE);

  camera = GST_CAMERABIN (photo);

  photography = GET_PHOTOGRAPHY (camera);

  if (photography) {
    ret = gst_photography_get_iso_speed (photography, iso_speed);
  }
  return ret;
}

static gboolean
gst_camerabin_set_white_balance_mode (GstPhotography * photo,
    GstWhiteBalanceMode white_balance_mode)
{
  GstCameraBin *camera;
  GstPhotography *photography;
  gboolean ret = TRUE;

  g_return_val_if_fail (photo != NULL, FALSE);

  camera = GST_CAMERABIN (photo);

  /* Cache the setting */
  camera->photo_settings.wb_mode = white_balance_mode;

  photography = GET_PHOTOGRAPHY (camera);

  if (photography) {
    ret = gst_photography_set_white_balance_mode (photography,
        white_balance_mode);
  }
  return ret;
}

static gboolean
gst_camerabin_get_white_balance_mode (GstPhotography * photo,
    GstWhiteBalanceMode * white_balance_mode)
{
  GstCameraBin *camera;
  GstPhotography *photography;
  gboolean ret = FALSE;

  g_return_val_if_fail (photo != NULL, FALSE);

  camera = GST_CAMERABIN (photo);

  photography = GET_PHOTOGRAPHY (camera);

  if (photography) {
    ret = gst_photography_get_white_balance_mode (photography,
        white_balance_mode);
  }
  return ret;
}

static gboolean
gst_camerabin_set_colour_tone_mode (GstPhotography * photo,
    GstColourToneMode colour_tone_mode)
{
  GstCameraBin *camera;
  GstPhotography *photography;
  gboolean ret = TRUE;

  g_return_val_if_fail (photo != NULL, FALSE);

  camera = GST_CAMERABIN (photo);

  /* Cache the setting */
  camera->photo_settings.tone_mode = colour_tone_mode;

  photography = GET_PHOTOGRAPHY (camera);

  if (photography) {
    ret = gst_photography_set_colour_tone_mode (photography, colour_tone_mode);
  }
  return ret;
}

static gboolean
gst_camerabin_get_colour_tone_mode (GstPhotography * photo,
    GstColourToneMode * colour_tone_mode)
{
  GstCameraBin *camera;
  GstPhotography *photography;
  gboolean ret = FALSE;

  g_return_val_if_fail (photo != NULL, FALSE);

  camera = GST_CAMERABIN (photo);

  photography = GET_PHOTOGRAPHY (camera);

  if (photography) {
    ret = gst_photography_get_colour_tone_mode (photography, colour_tone_mode);
  }
  return ret;
}

static gboolean
gst_camerabin_set_flash_mode (GstPhotography * photo, GstFlashMode flash_mode)
{
  GstCameraBin *camera;
  GstPhotography *photography;
  gboolean ret = TRUE;

  g_return_val_if_fail (photo != NULL, FALSE);

  camera = GST_CAMERABIN (photo);

  /* Cache the setting */
  camera->photo_settings.flash_mode = flash_mode;

  photography = GET_PHOTOGRAPHY (camera);

  if (photography) {
    ret = gst_photography_set_flash_mode (photography, flash_mode);
  }
  return ret;
}

static gboolean
gst_camerabin_get_flash_mode (GstPhotography * photo, GstFlashMode * flash_mode)
{
  GstCameraBin *camera;
  GstPhotography *photography;
  gboolean ret = FALSE;

  g_return_val_if_fail (photo != NULL, FALSE);

  camera = GST_CAMERABIN (photo);

  photography = GET_PHOTOGRAPHY (camera);

  if (photography) {
    ret = gst_photography_get_flash_mode (photography, flash_mode);
  }
  return ret;
}

static gboolean
gst_camerabin_set_scene_mode (GstPhotography * photo, GstSceneMode scene_mode)
{
  GstCameraBin *camera;
  GstPhotography *photography;
  gboolean ret = TRUE;

  g_return_val_if_fail (photo != NULL, FALSE);

  camera = GST_CAMERABIN (photo);

  /* Cache the setting */
  camera->photo_settings.scene_mode = scene_mode;

  gst_camerabin_handle_scene_mode (camera, scene_mode);

  photography = GET_PHOTOGRAPHY (camera);

  if (photography) {
    ret = gst_photography_set_scene_mode (photography, scene_mode);
    if (ret) {
      gst_photography_get_config (photography, &camera->photo_settings);
    }
  }
  return ret;
}

static gboolean
gst_camerabin_get_scene_mode (GstPhotography * photo, GstSceneMode * scene_mode)
{
  GstCameraBin *camera;
  GstPhotography *photography;
  gboolean ret = FALSE;

  g_return_val_if_fail (photo != NULL, FALSE);

  camera = GST_CAMERABIN (photo);

  photography = GET_PHOTOGRAPHY (camera);

  if (photography) {
    ret = gst_photography_get_scene_mode (photography, scene_mode);
  }
  return ret;
}

static GstPhotoCaps
gst_camerabin_get_capabilities (GstPhotography * photo)
{
  GstCameraBin *camera;
  GstPhotography *photography;
  /* camerabin can zoom by itself */
  GstPhotoCaps pcaps = GST_PHOTOGRAPHY_CAPS_ZOOM;

  g_return_val_if_fail (photo != NULL, FALSE);

  camera = GST_CAMERABIN (photo);

  photography = GET_PHOTOGRAPHY (camera);

  if (photography) {
    pcaps |= gst_photography_get_capabilities (photography);
  }

  return pcaps;
}

static void
gst_camerabin_set_autofocus (GstPhotography * photo, gboolean on)
{
  GstCameraBin *camera;
  GstPhotography *photography;

  g_return_if_fail (photo != NULL);

  camera = GST_CAMERABIN (photo);

  GST_DEBUG_OBJECT (camera, "setting autofocus %s", on ? "ON" : "OFF");

  photography = GET_PHOTOGRAPHY (camera);

  if (photography) {
    gst_photography_set_autofocus (photography, on);
  }
}

static gboolean
gst_camerabin_get_aperture (GstPhotography * photo, guint * aperture)
{
  GstCameraBin *camera;
  GstPhotography *photography;
  gboolean ret = FALSE;

  g_return_val_if_fail (photo != NULL, FALSE);

  camera = GST_CAMERABIN (photo);

  photography = GET_PHOTOGRAPHY (camera);

  if (photography) {
    ret = gst_photography_get_aperture (photography, aperture);
  }
  return ret;
}

static void
gst_camerabin_set_aperture (GstPhotography * photo, guint aperture)
{
  GstCameraBin *camera;
  GstPhotography *photography;

  g_return_if_fail (photo != NULL);

  camera = GST_CAMERABIN (photo);

  photography = GET_PHOTOGRAPHY (camera);

  if (photography) {
    gst_photography_set_aperture (photography, aperture);
  }
}

static gboolean
gst_camerabin_get_exposure (GstPhotography * photo, guint32 * exposure)
{
  GstCameraBin *camera;
  GstPhotography *photography;
  gboolean ret = FALSE;

  g_return_val_if_fail (photo != NULL, FALSE);

  camera = GST_CAMERABIN (photo);

  photography = GET_PHOTOGRAPHY (camera);

  if (photography) {
    ret = gst_photography_get_exposure (photography, exposure);
  }
  return ret;
}

static void
gst_camerabin_set_exposure (GstPhotography * photo, guint32 exposure)
{
  GstCameraBin *camera;
  GstPhotography *photography;

  g_return_if_fail (photo != NULL);

  camera = GST_CAMERABIN (photo);

  photography = GET_PHOTOGRAPHY (camera);

  if (photography) {
    gst_photography_set_exposure (photography, exposure);
  }
}

static gboolean
gst_camerabin_set_config (GstPhotography * photo, GstPhotoSettings * config)
{
  GstCameraBin *camera;
  GstPhotography *photography;
  gboolean ret = TRUE;
  g_return_val_if_fail (photo != NULL, FALSE);
  camera = GST_CAMERABIN (photo);

  /* Cache the settings */
  memcpy (&camera->photo_settings, config, sizeof (GstPhotoSettings));

  /* Handle night mode */
  gst_camerabin_handle_scene_mode (camera, config->scene_mode);

  photography = GET_PHOTOGRAPHY (camera);

  if (photography) {
    ret = gst_photography_set_config (photography, config);
  }
  return ret;
}

static gboolean
gst_camerabin_get_config (GstPhotography * photo, GstPhotoSettings * config)
{
  GstCameraBin *camera;
  GstPhotography *photography;
  gboolean ret = FALSE;
  g_return_val_if_fail (photo != NULL, FALSE);
  camera = GST_CAMERABIN (photo);
  photography = GET_PHOTOGRAPHY (camera);
  if (photography) {
    ret = gst_photography_get_config (photography, config);
  }
  return ret;
}

static void
gst_camerabin_handle_scene_mode (GstCameraBin * camera, GstSceneMode scene_mode)
{
  // XXX maybe handle this inside night_mode propery in srcbin?  But
  // what about the set-video-resolution-fps?  Where is that handled?
#if 0
  if (scene_mode == GST_PHOTOGRAPHY_SCENE_MODE_NIGHT) {
    if (!camera->night_mode) {
      GST_DEBUG ("enabling night mode, lowering fps");
      /* Make camerabin select the lowest allowed frame rate */
      camera->night_mode = TRUE;
      /* Remember frame rate before setting night mode */
      camera->pre_night_fps_n = camera->fps_n;
      camera->pre_night_fps_d = camera->fps_d;
      g_signal_emit_by_name (camera, "set-video-resolution-fps", camera->width,
          camera->height, 0, 1, NULL);
    } else {
      GST_DEBUG ("night mode already enabled");
    }
  } else {
    if (camera->night_mode) {
      GST_DEBUG ("disabling night mode, restoring fps to %d/%d",
          camera->pre_night_fps_n, camera->pre_night_fps_d);
      camera->night_mode = FALSE;
      g_signal_emit_by_name (camera, "set-video-resolution-fps", camera->width,
          camera->height, camera->pre_night_fps_n, camera->pre_night_fps_d, 0);
    }
  }
#endif
}

static gboolean
gst_camerabin_set_flicker_mode (GstPhotography * photo,
    GstFlickerReductionMode flicker_mode)
{
  GstCameraBin *camera;
  GstPhotography *photography;
  gboolean ret = TRUE;

  g_return_val_if_fail (photo != NULL, FALSE);

  camera = GST_CAMERABIN (photo);

  /* Cache the setting */
  camera->photo_settings.flicker_mode = flicker_mode;

  photography = GET_PHOTOGRAPHY (camera);

  if (photography) {
    ret = gst_photography_set_flicker_mode (photography, flicker_mode);
  }
  return ret;
}

static gboolean
gst_camerabin_get_flicker_mode (GstPhotography * photo,
    GstFlickerReductionMode * flicker_mode)
{
  GstCameraBin *camera;
  GstPhotography *photography;
  gboolean ret = FALSE;

  g_return_val_if_fail (photo != NULL, FALSE);

  camera = GST_CAMERABIN (photo);

  photography = GET_PHOTOGRAPHY (camera);

  if (photography) {
    ret = gst_photography_get_flicker_mode (photography, flicker_mode);
  }
  return ret;
}

static gboolean
gst_camerabin_set_focus_mode (GstPhotography * photo, GstFocusMode focus_mode)
{
  GstCameraBin *camera;
  GstPhotography *photography;
  gboolean ret = TRUE;

  g_return_val_if_fail (photo != NULL, FALSE);

  camera = GST_CAMERABIN (photo);

  /* Cache the setting */
  camera->photo_settings.focus_mode = focus_mode;

  photography = GET_PHOTOGRAPHY (camera);

  if (photography) {
    ret = gst_photography_set_focus_mode (photography, focus_mode);
  }
  return ret;
}

static gboolean
gst_camerabin_get_focus_mode (GstPhotography * photo, GstFocusMode * focus_mode)
{
  GstCameraBin *camera;
  GstPhotography *photography;
  gboolean ret = FALSE;

  g_return_val_if_fail (photo != NULL, FALSE);

  camera = GST_CAMERABIN (photo);

  photography = GET_PHOTOGRAPHY (camera);

  if (photography) {
    ret = gst_photography_get_focus_mode (photography, focus_mode);
  }
  return ret;
}

gboolean
gst_camerabin_photography_get_property (GstCameraBin * camera, guint prop_id,
    GValue * value)
{
  gboolean ret = FALSE;

  GST_DEBUG_OBJECT (camera, "Photointerface property: %d", prop_id);

  switch (prop_id) {
    case ARG_WB_MODE:
    {
      GstWhiteBalanceMode wb_mode;
      GST_DEBUG_OBJECT (camera, "==== GETTING PROP_WB_MODE ====");
      if (gst_camerabin_get_white_balance_mode ((GstPhotography *) camera,
              &wb_mode)) {
        g_value_set_enum (value, wb_mode);
      }
      ret = TRUE;
      break;
    }
    case ARG_COLOUR_TONE:
    {
      GstColourToneMode tone;
      GST_DEBUG_OBJECT (camera, "==== GETTING PROP_COLOUR_TONE ====");
      if (gst_camerabin_get_colour_tone_mode ((GstPhotography *) camera, &tone)) {
        g_value_set_enum (value, tone);
      }
      ret = TRUE;
      break;
    }
    case ARG_SCENE_MODE:
    {
      GstSceneMode scene;
      GST_DEBUG_OBJECT (camera, "==== GETTING PROP_SCENE_MODE ====");
      if (gst_camerabin_get_scene_mode ((GstPhotography *) camera, &scene)) {
        g_value_set_enum (value, scene);
      }
      ret = TRUE;
      break;
    }
    case ARG_FLASH_MODE:
    {
      GstFlashMode flash;
      GST_DEBUG_OBJECT (camera, "==== GETTING PROP_FLASH_MODE ====");
      if (gst_camerabin_get_flash_mode ((GstPhotography *) camera, &flash)) {
        g_value_set_enum (value, flash);
      }
      ret = TRUE;
      break;
    }
    case ARG_CAPABILITIES:
    {
      gulong capabilities;
      GST_DEBUG_OBJECT (camera, "==== GETTING PROP_CAPABILITIES ====");
      capabilities =
          (gulong) gst_camerabin_get_capabilities ((GstPhotography *) camera);
      g_value_set_ulong (value, capabilities);
      ret = TRUE;
      break;
    }
    case ARG_EV_COMP:
    {
      gfloat ev_comp;
      GST_DEBUG_OBJECT (camera, "==== GETTING PROP_EV_COMP ====");
      if (gst_camerabin_get_ev_compensation ((GstPhotography *) camera,
              &ev_comp)) {
        g_value_set_float (value, ev_comp);
      }
      ret = TRUE;
      break;
    }
    case ARG_ISO_SPEED:
    {
      guint iso_speed;
      GST_DEBUG_OBJECT (camera, "==== GETTING PROP_ISO_SPEED ====");
      if (gst_camerabin_get_iso_speed ((GstPhotography *) camera, &iso_speed)) {
        g_value_set_uint (value, iso_speed);
      }
      ret = TRUE;
      break;
    }
    case ARG_APERTURE:
    {
      guint aperture;
      GST_DEBUG_OBJECT (camera, "==== GETTING PROP_APERTURE ====");
      if (gst_camerabin_get_aperture ((GstPhotography *) camera, &aperture)) {
        g_value_set_uint (value, aperture);
      }
      ret = TRUE;
      break;
    }
    case ARG_EXPOSURE:
    {
      guint32 exposure;
      GST_DEBUG_OBJECT (camera, "==== GETTING PROP_EXPOSURE ====");
      if (gst_camerabin_get_exposure ((GstPhotography *) camera, &exposure)) {
        g_value_set_uint (value, exposure);
      }
      ret = TRUE;
      break;
    }
    case ARG_IMAGE_CAPTURE_SUPPORTED_CAPS:
    {
      GstPhotography *photography = photography = GET_PHOTOGRAPHY (camera);

      GST_DEBUG_OBJECT (camera, "==== GETTING PROP_IMAGE_CAPTURE_CAPS ====");
      if (photography) {
        g_object_get_property (G_OBJECT (camera->srcbin),
            "image-capture-supported-caps", value);
      } else {
        g_object_get_property (G_OBJECT (camera), "video-source-caps", value);
      }
      ret = TRUE;
      break;
    }
    case ARG_FLICKER_MODE:
    {
      GstFlickerReductionMode mode;
      GST_DEBUG_OBJECT (camera, "==== GETTING PROP_FLICKER_MODE ====");
      if (gst_camerabin_get_flicker_mode ((GstPhotography *) camera, &mode)) {
        g_value_set_enum (value, mode);
      }
      ret = TRUE;
      break;
    }
    case ARG_FOCUS_MODE:
    {
      GstFocusMode mode;
      GST_DEBUG_OBJECT (camera, "==== GETTING PROP_FOCUS_MODE ====");
      if (gst_camerabin_get_focus_mode ((GstPhotography *) camera, &mode)) {
        g_value_set_enum (value, mode);
      }
      ret = TRUE;
      break;
    }
    default:
      break;
  }

  return ret;
}


/*
 *
 */
gboolean
gst_camerabin_photography_set_property (GstCameraBin * camera, guint prop_id,
    const GValue * value)
{
  gboolean ret = FALSE;

  switch (prop_id) {
    case ARG_WB_MODE:
      GST_DEBUG_OBJECT (camera, "==== SETTING PROP_WB_MODE ====");
      gst_camerabin_set_white_balance_mode ((GstPhotography *) camera,
          g_value_get_enum (value));
      ret = TRUE;
      break;
    case ARG_COLOUR_TONE:
      GST_DEBUG_OBJECT (camera, "==== SETTING PROP_COLOUR_TONE ====");
      gst_camerabin_set_colour_tone_mode ((GstPhotography *) camera,
          g_value_get_enum (value));
      ret = TRUE;
      break;
    case ARG_SCENE_MODE:
      GST_DEBUG_OBJECT (camera, "==== SETTING PROP_SCENE_MODE ====");
      gst_camerabin_set_scene_mode ((GstPhotography *) camera,
          g_value_get_enum (value));
      ret = TRUE;
      break;
    case ARG_FLASH_MODE:
      GST_DEBUG_OBJECT (camera, "==== SETTING PROP_FLASH_MODE ====");
      gst_camerabin_set_flash_mode ((GstPhotography *) camera,
          g_value_get_enum (value));
      ret = TRUE;
      break;
    case ARG_EV_COMP:
      GST_DEBUG_OBJECT (camera, "==== SETTING PROP_EV_COMP ====");
      gst_camerabin_set_ev_compensation ((GstPhotography *) camera,
          g_value_get_float (value));
      ret = TRUE;
      break;
    case ARG_ISO_SPEED:
      GST_DEBUG_OBJECT (camera, "==== SETTING PROP_ISO_SPEED ====");
      gst_camerabin_set_iso_speed ((GstPhotography *) camera,
          g_value_get_uint (value));
      ret = TRUE;
      break;
    case ARG_APERTURE:
      GST_DEBUG_OBJECT (camera, "==== SETTING PROP_APERTURE ====");
      gst_camerabin_set_aperture ((GstPhotography *) camera,
          g_value_get_uint (value));
      ret = TRUE;
      break;
    case ARG_EXPOSURE:
      GST_DEBUG_OBJECT (camera, "==== SETTING PROP_EXPOSURE ====");
      gst_camerabin_set_exposure ((GstPhotography *) camera,
          g_value_get_uint (value));
      ret = TRUE;
      break;
    case ARG_FLICKER_MODE:
      GST_DEBUG_OBJECT (camera, "==== SETTING PROP_FLICKER_MODE ====");
      gst_camerabin_set_flicker_mode ((GstPhotography *) camera,
          g_value_get_enum (value));
      ret = TRUE;
      break;
    case ARG_FOCUS_MODE:
      GST_DEBUG_OBJECT (camera, "==== SETTING PROP_FOCUS_MODE ====");
      gst_camerabin_set_focus_mode ((GstPhotography *) camera,
          g_value_get_enum (value));
      ret = TRUE;
      break;
    default:
      break;
  }

  return ret;
}


void
gst_camerabin_photography_init (GstPhotographyInterface * iface)
{
  GST_DEBUG_CATEGORY_INIT (camerabinphoto_debug, "camerabinphoto", 0,
      "Camerabin photography interface debugging");

  GST_INFO ("initing");

  iface->set_ev_compensation = gst_camerabin_set_ev_compensation;
  iface->get_ev_compensation = gst_camerabin_get_ev_compensation;

  iface->set_iso_speed = gst_camerabin_set_iso_speed;
  iface->get_iso_speed = gst_camerabin_get_iso_speed;

  iface->set_white_balance_mode = gst_camerabin_set_white_balance_mode;
  iface->get_white_balance_mode = gst_camerabin_get_white_balance_mode;

  iface->set_colour_tone_mode = gst_camerabin_set_colour_tone_mode;
  iface->get_colour_tone_mode = gst_camerabin_get_colour_tone_mode;

  iface->set_scene_mode = gst_camerabin_set_scene_mode;
  iface->get_scene_mode = gst_camerabin_get_scene_mode;

  iface->set_flash_mode = gst_camerabin_set_flash_mode;
  iface->get_flash_mode = gst_camerabin_get_flash_mode;

  iface->get_capabilities = gst_camerabin_get_capabilities;

  iface->set_autofocus = gst_camerabin_set_autofocus;

  iface->set_config = gst_camerabin_set_config;
  iface->get_config = gst_camerabin_get_config;
}
