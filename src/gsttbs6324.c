/*
 * gsttbs6324.c - GStreamer plugin for TBS6324 Quad 3G-SDI Encoder
 *
 * Copyright (C) 2025 Robert Hensel VK3DG Australia
 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/types.h>

#include "gsttbs6324.h"

GST_DEBUG_CATEGORY_STATIC (gst_tbs6324_src_debug);
#define GST_CAT_DEFAULT gst_tbs6324_src_debug

/* Custom TBS ioctl definitions */
#define FE_24CXX_READ    _IOR('o', 92, struct mcu24cxx_info)
#define FE_24CXX_WRITE   _IOW('o', 93, struct mcu24cxx_info)

struct mcu24cxx_info {
    __u32 bassaddr;
    __u8 reg;
    __u32 data;
};

/* Register addresses from TBS6324SDITool */
#define BASE_ADDRESS_UDPCMD  0x2000
#define COMMAND              0x00
#define STATUS               0x00
#define RW_SIZEADDR          0x08
#define RW_TYPEADDR          0x0c
#define RW_LENADDR           0x10
#define RW_ADDRESS           0x0c
#define RW_DATA              0x18
#define DATA_BLKCTRL         0x14
#define SERVICE_ID           0x34

/* Channel-specific offsets */
#define TBS6324_1_OFFSET     0x1000
#define TBS6324_2_OFFSET     0x800
#define TBS6324_3_OFFSET     0x1800
#define TBS6324_4_OFFSET     0x400

/* Default values */
#define DEFAULT_ENCODER           0
#define DEFAULT_FRONTEND          0
#define DEFAULT_ENCODER_TYPE      "H264"
#define DEFAULT_WIDTH             1920
#define DEFAULT_HEIGHT            1080
#define DEFAULT_FPS               60
#define DEFAULT_BITRATE           6000
#define DEFAULT_BITRATE_MODE      "cbr"
#define DEFAULT_GOP_INTERVAL      30
#define DEFAULT_PROFILE           "main"
#define DEFAULT_PMT_PID           1024
#define DEFAULT_VIDEO_PID         512
#define DEFAULT_AUDIO_PID         768
#define DEFAULT_SERVICE_ID        256
#define DEFAULT_PROGRAM_NAME      "TBS-SDI"
#define DEFAULT_AUDIO_CODEC       "AAC"
#define DEFAULT_AUDIO_BITRATE     128000
#define DEFAULT_AUDIO_TYPE        "LC-AAC"
#define DEFAULT_AUDIO_CHANNEL     0
#define DEFAULT_AUDIO_RESAMPLE    0
#define DEFAULT_BUFFER_SIZE       (188 * 100)  /* 100 TS packets */

/* Plugin version and package */
#ifndef VERSION
#define VERSION "0.1.0"
#endif

#ifndef PACKAGE
#define PACKAGE "gst-tbs6324"
#endif

/* Filter signals and args */
enum
{
  PROP_0,
  PROP_ENCODER,
  PROP_ENCODER_TYPE,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_FPS,
  PROP_BITRATE,
  PROP_BITRATE_MODE,
  PROP_GOP_INTERVAL,
  PROP_PROFILE,
  PROP_PMT_PID,
  PROP_VIDEO_PID,
  PROP_AUDIO_PID,
  PROP_SERVICE_ID,
  PROP_PROGRAM_NAME,
  PROP_AUDIO_CODEC,
  PROP_AUDIO_BITRATE,
  PROP_AUDIO_TYPE,
  PROP_AUDIO_CHANNEL,
  PROP_AUDIO_RESAMPLE,
};

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("video/mpegts, "
        "systemstream = (boolean) true, "
        "packetsize = (int) 188"));

#define _do_init \
    GST_DEBUG_CATEGORY_INIT (gst_tbs6324_src_debug, "tbs6324src", 0, "TBS6324 SDI Encoder Source");

G_DEFINE_TYPE_WITH_CODE (GstTBS6324Src, gst_tbs6324_src, GST_TYPE_PUSH_SRC, _do_init);

static void gst_tbs6324_src_finalize (GObject * object);
static void gst_tbs6324_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_tbs6324_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gboolean gst_tbs6324_src_start (GstBaseSrc * basesrc);
static gboolean gst_tbs6324_src_stop (GstBaseSrc * basesrc);
static GstFlowReturn gst_tbs6324_src_create (GstPushSrc * pushsrc,
    GstBuffer ** buf);

  static gboolean gst_tbs6324_detect_card (GstTBS6324Src * src);
/* Helper functions */
static gboolean gst_tbs6324_open_devices (GstTBS6324Src * src);
static void gst_tbs6324_close_devices (GstTBS6324Src * src);
static gboolean gst_tbs6324_configure_encoder (GstTBS6324Src * src);
static gchar *gst_tbs6324_get_firmware_version (GstTBS6324Src * src);

/* Command type for reading extended info (firmware version) */
#define GET_EXT         0x10203034

static gint gst_tbs6324_get_channel_offset (GstTBS6324Src * src);
static gboolean gst_tbs6324_write_register (GstTBS6324Src * src,
    __u32 reg, __u32 data);
static __u32 gst_tbs6324_read_register (GstTBS6324Src * src, __u32 reg);

static void
gst_tbs6324_src_class_init (GstTBS6324SrcClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;
  GstBaseSrcClass *gstbasesrc_class;
  GstPushSrcClass *gstpushsrc_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gstelement_class = GST_ELEMENT_CLASS (klass);
  gstbasesrc_class = GST_BASE_SRC_CLASS (klass);
  gstpushsrc_class = GST_PUSH_SRC_CLASS (klass);

  gobject_class->finalize = gst_tbs6324_src_finalize;
  gobject_class->set_property = gst_tbs6324_src_set_property;
  gobject_class->get_property = gst_tbs6324_src_get_property;

  gst_element_class_set_metadata (gstelement_class,
      "TBS6324 SDI Encoder Source",
      "Source/Video/Hardware",
      "Captures video from TBS6324 Quad 3G-SDI PCIe Encoder"
#ifdef HAVE_CONFIG_H
      "\nBuilt: " BUILD_DATE " " BUILD_TIME
#endif
      ,
      "Plugin Author");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_template));

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_tbs6324_src_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_tbs6324_src_stop);

  gstpushsrc_class->create = GST_DEBUG_FUNCPTR (gst_tbs6324_src_create);

  /* Properties */
  g_object_class_install_property (gobject_class, PROP_ENCODER,
      g_param_spec_int ("encoder", "Encoder number",
          "The encoder channel number (0-3 for TBS6324 channels)",
          0, 3, DEFAULT_ENCODER, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_ENCODER_TYPE,
      g_param_spec_string ("encoder-type", "Encoder type",
          "Video encoder type (H264 or H265)",
          DEFAULT_ENCODER_TYPE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_WIDTH,
      g_param_spec_int ("width", "Output width",
          "Output video width (0 for auto, 640-1920)",
          0, 1920, DEFAULT_WIDTH, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_HEIGHT,
      g_param_spec_int ("height", "Output height",
          "Output video height (0 for auto, 360-1080)",
          0, 1080, DEFAULT_HEIGHT, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_FPS,
      g_param_spec_int ("fps", "Frame rate",
          "Output frame rate (5-60)",
          5, 60, DEFAULT_FPS, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BITRATE,
      g_param_spec_int ("bitrate", "Video bitrate",
          "Video bitrate in kbps (160-20000)",
          160, 20000, DEFAULT_BITRATE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_BITRATE_MODE,
      g_param_spec_string ("bitrate-mode", "Bitrate control mode",
          "Bitrate control mode (cbr, vbr, or evbr)",
          DEFAULT_BITRATE_MODE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_GOP_INTERVAL,
      g_param_spec_int ("gop-interval", "GOP interval",
          "GOP key interval (1-180)",
          1, 180, DEFAULT_GOP_INTERVAL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PROFILE,
      g_param_spec_string ("profile", "H.264 profile",
          "H.264 profile (baseline, main, or high)",
          DEFAULT_PROFILE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PMT_PID,
      g_param_spec_int ("pmt-pid", "PMT PID",
          "PMT PID (16-7936)",
          16, 7936, DEFAULT_PMT_PID, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_VIDEO_PID,
      g_param_spec_int ("video-pid", "Video PID",
          "Video PID (16-3840)",
          16, 3840, DEFAULT_VIDEO_PID, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_AUDIO_PID,
      g_param_spec_int ("audio-pid", "Audio PID",
          "Audio PID (16-3840)",
          16, 3840, DEFAULT_AUDIO_PID, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_SERVICE_ID,
      g_param_spec_int ("service-id", "Service ID",
          "Service ID (1-65535)",
          1, 65535, DEFAULT_SERVICE_ID, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_PROGRAM_NAME,
      g_param_spec_string ("program-name", "Program name",
          "Program name (max 16 characters)",
          DEFAULT_PROGRAM_NAME, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_AUDIO_CODEC,
      g_param_spec_string ("audio-codec", "Audio codec",
          "Audio codec (AAC or MP2)",
          DEFAULT_AUDIO_CODEC, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_AUDIO_BITRATE,
      g_param_spec_int ("audio-bitrate", "Audio bitrate",
          "Audio bitrate in bps (24000-256000)",
          24000, 256000, DEFAULT_AUDIO_BITRATE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_AUDIO_TYPE,
      g_param_spec_string ("audio-type", "AAC type",
          "AAC type (LC-AAC or HE-AAC)",
          DEFAULT_AUDIO_TYPE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_AUDIO_CHANNEL,
      g_param_spec_int ("audio-channel", "Audio channel",
          "SDI audio channel pair (0-3, where 0=ch1-2, 1=ch3-4, etc.)",
          0, 3, DEFAULT_AUDIO_CHANNEL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (gobject_class, PROP_AUDIO_RESAMPLE,
      g_param_spec_int ("audio-resample", "Audio resample rate",
          "Audio resample rate (0=off, 32000, or 44100)",
          0, 44100, DEFAULT_AUDIO_RESAMPLE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

/* Helper function to get encoder-specific default values */
static void
gst_tbs6324_get_encoder_defaults (GstTBS6324Src * src,
    gint *pmt_pid, gint *video_pid, gint *audio_pid,
    gint *service_id, gchar **program_name)
{
  /* Base values for encoder 0 */
  gint base_pmt_pid = DEFAULT_PMT_PID;
  gint base_video_pid = DEFAULT_VIDEO_PID;
  gint base_audio_pid = DEFAULT_AUDIO_PID;
  gint base_service_id = DEFAULT_SERVICE_ID;

  /* Add offset based on encoder number */
  *pmt_pid = base_pmt_pid + (src->encoder_number * 10);
  *video_pid = base_video_pid + src->encoder_number;
  *audio_pid = base_audio_pid + src->encoder_number;
  *service_id = base_service_id + src->encoder_number;

  /* Generate unique program name */
  *program_name = g_strdup_printf ("TBS-SDI-%d", src->encoder_number + 1);
}

static void
gst_tbs6324_src_init (GstTBS6324Src * src)
{
  gint pmt_pid, video_pid, audio_pid, service_id;
  gchar *program_name;

  src->encoder_number = DEFAULT_ENCODER;
  src->frontend_number = DEFAULT_FRONTEND;
  src->frontend_device = NULL;
  src->dvr_device = NULL;
  src->fd_frontend = -1;
  src->fd_dvr = -1;

  src->encoder_type = g_strdup (DEFAULT_ENCODER_TYPE);
  src->width = DEFAULT_WIDTH;
  src->height = DEFAULT_HEIGHT;
  src->fps = DEFAULT_FPS;
  src->bitrate = DEFAULT_BITRATE;
  src->bitrate_mode = g_strdup (DEFAULT_BITRATE_MODE);
  src->gop_interval = DEFAULT_GOP_INTERVAL;
  src->profile = g_strdup (DEFAULT_PROFILE);

  /* Get encoder-specific defaults */
  gst_tbs6324_get_encoder_defaults (src, &pmt_pid, &video_pid, &audio_pid,
      &service_id, &program_name);
  src->pmt_pid = pmt_pid;
  src->video_pid = video_pid;
  src->audio_pid = audio_pid;
  src->service_id = service_id;
  src->program_name = program_name;

  src->audio_codec = g_strdup (DEFAULT_AUDIO_CODEC);
  src->audio_bitrate = DEFAULT_AUDIO_BITRATE;
  src->audio_type = g_strdup (DEFAULT_AUDIO_TYPE);
  src->audio_channel = DEFAULT_AUDIO_CHANNEL;
  src->audio_resample = DEFAULT_AUDIO_RESAMPLE;

  src->configured = FALSE;
  src->streaming = FALSE;
  src->firmware_version = NULL;
  src->buffer_size = DEFAULT_BUFFER_SIZE;
  src->last_ts = GST_CLOCK_TIME_NONE;

  /* Live source */
  gst_base_src_set_live (GST_BASE_SRC (src), TRUE);
  gst_base_src_set_format (GST_BASE_SRC (src), GST_FORMAT_TIME);
}

static void
gst_tbs6324_src_finalize (GObject * object)
{
  GstTBS6324Src *src = GST_TBS6324_SRC (object);

  g_free (src->frontend_device);
  g_free (src->dvr_device);
  g_free (src->encoder_type);
  g_free (src->bitrate_mode);
  g_free (src->profile);
  g_free (src->program_name);
  g_free (src->audio_codec);
  g_free (src->audio_type);
  g_free (src->firmware_version);

  G_OBJECT_CLASS (gst_tbs6324_src_parent_class)->finalize (object);
}

static void
gst_tbs6324_src_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstTBS6324Src *src = GST_TBS6324_SRC (object);

  switch (prop_id) {
    case PROP_ENCODER:
      src->encoder_number = g_value_get_int (value);
      break;
    case PROP_ENCODER_TYPE:
      g_free (src->encoder_type);
      src->encoder_type = g_value_dup_string (value);
      break;
    case PROP_WIDTH:
      src->width = g_value_get_int (value);
      break;
    case PROP_HEIGHT:
      src->height = g_value_get_int (value);
      break;
    case PROP_FPS:
      src->fps = g_value_get_int (value);
      break;
    case PROP_BITRATE:
      src->bitrate = g_value_get_int (value);
      break;
    case PROP_BITRATE_MODE:
      g_free (src->bitrate_mode);
      src->bitrate_mode = g_value_dup_string (value);
      break;
    case PROP_GOP_INTERVAL:
      src->gop_interval = g_value_get_int (value);
      break;
    case PROP_PROFILE:
      g_free (src->profile);
      src->profile = g_value_dup_string (value);
      break;
    case PROP_PMT_PID:
      src->pmt_pid = g_value_get_int (value);
      break;
    case PROP_VIDEO_PID:
      src->video_pid = g_value_get_int (value);
      break;
    case PROP_AUDIO_PID:
      src->audio_pid = g_value_get_int (value);
      break;
    case PROP_SERVICE_ID:
      src->service_id = g_value_get_int (value);
      break;
    case PROP_PROGRAM_NAME:
      g_free (src->program_name);
      src->program_name = g_value_dup_string (value);
      break;
    case PROP_AUDIO_CODEC:
      g_free (src->audio_codec);
      src->audio_codec = g_value_dup_string (value);
      break;
    case PROP_AUDIO_BITRATE:
      src->audio_bitrate = g_value_get_int (value);
      break;
    case PROP_AUDIO_TYPE:
      g_free (src->audio_type);
      src->audio_type = g_value_dup_string (value);
      break;
    case PROP_AUDIO_CHANNEL:
      src->audio_channel = g_value_get_int (value);
      break;
    case PROP_AUDIO_RESAMPLE:
      src->audio_resample = g_value_get_int (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_tbs6324_src_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstTBS6324Src *src = GST_TBS6324_SRC (object);

  switch (prop_id) {
    case PROP_ENCODER:
      g_value_set_int (value, src->encoder_number);
      break;
    case PROP_ENCODER_TYPE:
      g_value_set_string (value, src->encoder_type);
      break;
    case PROP_WIDTH:
      g_value_set_int (value, src->width);
      break;
    case PROP_HEIGHT:
      g_value_set_int (value, src->height);
      break;
    case PROP_FPS:
      g_value_set_int (value, src->fps);
      break;
    case PROP_BITRATE:
      g_value_set_int (value, src->bitrate);
      break;
    case PROP_BITRATE_MODE:
      g_value_set_string (value, src->bitrate_mode);
      break;
    case PROP_GOP_INTERVAL:
      g_value_set_int (value, src->gop_interval);
      break;
    case PROP_PROFILE:
      g_value_set_string (value, src->profile);
      break;
    case PROP_PMT_PID:
      g_value_set_int (value, src->pmt_pid);
      break;
    case PROP_VIDEO_PID:
      g_value_set_int (value, src->video_pid);
      break;
    case PROP_AUDIO_PID:
      g_value_set_int (value, src->audio_pid);
      break;
    case PROP_SERVICE_ID:
      g_value_set_int (value, src->service_id);
      break;
    case PROP_PROGRAM_NAME:
      g_value_set_string (value, src->program_name);
      break;
    case PROP_AUDIO_CODEC:
      g_value_set_string (value, src->audio_codec);
      break;
    case PROP_AUDIO_BITRATE:
      g_value_set_int (value, src->audio_bitrate);
      break;
    case PROP_AUDIO_TYPE:
      g_value_set_string (value, src->audio_type);
      break;
    case PROP_AUDIO_CHANNEL:
      g_value_set_int (value, src->audio_channel);
      break;
    case PROP_AUDIO_RESAMPLE:
      g_value_set_int (value, src->audio_resample);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* Get channel-specific address offset */
static gint
gst_tbs6324_get_channel_offset (GstTBS6324Src * src)
{
  switch (src->encoder_number) {
    case 0: return TBS6324_1_OFFSET;
    case 1: return TBS6324_2_OFFSET;
    case 2: return TBS6324_3_OFFSET;
    case 3: return TBS6324_4_OFFSET;
    default: return TBS6324_1_OFFSET;
  }
}

/* Write register using FE_24CXX ioctl */
static gboolean
gst_tbs6324_write_register (GstTBS6324Src * src, __u32 reg, __u32 data)
{
  struct mcu24cxx_info info;
  __u32 base_addr;

  if (src->fd_frontend < 0) {
    GST_ERROR_OBJECT (src, "Frontend device not open");
    return FALSE;
  }

  /* Get base address for this channel */
  base_addr = BASE_ADDRESS_UDPCMD + gst_tbs6324_get_channel_offset (src);

  info.bassaddr = base_addr;
  info.reg = reg;  /* Register offset */
  info.data = data;

  GST_TRACE_OBJECT (src, "Write: base=0x%04x, reg=0x%02x, data=0x%08x",
      base_addr, reg, data);

  if (ioctl (src->fd_frontend, FE_24CXX_WRITE, &info) < 0) {
    GST_ERROR_OBJECT (src, "FE_24CXX_WRITE failed (reg=0x%02x): %s", reg, g_strerror (errno));
    return FALSE;
  }

  return TRUE;
}
/* Read register using FE_24CXX ioctl */
static __u32
gst_tbs6324_read_register (GstTBS6324Src * src, __u32 reg)
{
  struct mcu24cxx_info info;
  __u32 base_addr;
  
  if (src->fd_frontend < 0) {
    GST_ERROR_OBJECT (src, "Frontend device not open");
    return 0;
  }
  
  base_addr = BASE_ADDRESS_UDPCMD + gst_tbs6324_get_channel_offset (src);
  
  info.bassaddr = base_addr;
  info.reg = reg;
  
  GST_TRACE_OBJECT (src, "Read: base=0x%04x, reg=0x%02x", base_addr, reg);
  
  if (ioctl (src->fd_frontend, FE_24CXX_READ, &info) < 0) {
    GST_ERROR_OBJECT (src, "FE_24CXX_READ failed (reg=0x%02x): %s", reg, g_strerror (errno));
    return 0;
  }
  
  return info.data;
}


/* Configure encoder with current settings */
static gboolean
gst_tbs6324_configure_encoder (GstTBS6324Src * src)
{
  gchar cmd_string[512];
  __u32 cmd_type = 0x10203040;  /* SET_STREAM1 */
  __u32 udplen, datalen;
  __u32 data_value;
  gsize str_len;
  guint i;

  GST_INFO_OBJECT (src, "Configuring encoder for channel %d", src->encoder_number);

  /* Build configuration command string */
  /* Format: &param1=value1&param2=value2... */
  g_snprintf (cmd_string, sizeof (cmd_string),
      "&codec_type=%s&cbr_rate=%d&vbr_rate=%d&evbr_rate=%d&venc_gop=%d&enc_fps=%d&enc_width=%d&enc_height=%d&video_pid=%d&audio_pid=%d&pmt_pid=%d&sid=%d&service_name=%s&audio_bitrate=%d&audio_resample=%d&audio_channel=%d",
      src->bitrate_mode,
      src->bitrate,
      src->bitrate,
      src->bitrate,
      src->gop_interval,
      src->fps,
      src->width,
      src->height,
      src->video_pid,
      src->audio_pid,
      src->pmt_pid,
      src->service_id,
      src->program_name,
      src->audio_bitrate,
      src->audio_resample,
      src->audio_channel
  );

  str_len = strlen (cmd_string);
  datalen = str_len;
  udplen = datalen + 8;  /* Add 8 bytes header */

  GST_INFO_OBJECT (src, "Command: %s", cmd_string);
  GST_DEBUG_OBJECT (src, "Data length: %u, UDP length: %u", datalen, udplen);

  /* Step 1: Set DATA_BLKCTRL to 0x80 (bit31 = 1 for write operation) */
  data_value = 0x00000080;
  if (!gst_tbs6324_write_register (src, DATA_BLKCTRL, data_value)) {
    GST_ERROR_OBJECT (src, "Failed to set DATA_BLKCTRL");
    return FALSE;
  }

  /* Step 2: Write command type to RW_TYPEADDR */
  data_value = cmd_type;
  if (!gst_tbs6324_write_register (src, RW_TYPEADDR, data_value)) {
    GST_ERROR_OBJECT (src, "Failed to write command type");
    return FALSE;
  }

  /* Step 3: Write UDP length to RW_LENADDR */
  data_value = udplen;
  if (!gst_tbs6324_write_register (src, RW_LENADDR, data_value)) {
    GST_ERROR_OBJECT (src, "Failed to write UDP length");
    return FALSE;
  }

  /* Step 4: Write data size to RW_SIZEADDR */
  /* Format: [0x00, (datalen >> 8), (datalen & 0xff), 0x00] */
  data_value = ((datalen & 0xff00) << 8) | (datalen & 0xff);
  if (!gst_tbs6324_write_register (src, RW_SIZEADDR, data_value)) {
    GST_ERROR_OBJECT (src, "Failed to write data size");
    return FALSE;
  }

  /* Step 5: Write command data to RW_DATA in 4-byte chunks */
  for (i = 0; i < (str_len / 4); i++) {
    data_value = (cmd_string[i*4] |
                  (cmd_string[i*4 + 1] << 8) |
                  (cmd_string[i*4 + 2] << 16) |
                  (cmd_string[i*4 + 3] << 24));
    if (!gst_tbs6324_write_register (src, RW_DATA, data_value)) {
      GST_ERROR_OBJECT (src, "Failed to write command data at chunk %u", i);
      return FALSE;
    }
  }
  /* Write remaining bytes if not a multiple of 4 */
  if (str_len % 4) {
    guchar remaining[4] = {0};
    for (i = 0; i < (str_len % 4); i++) {
      remaining[i] = cmd_string[(str_len / 4) * 4 + i];
    }
    data_value = (remaining[0] |
                  (remaining[1] << 8) |
                  (remaining[2] << 16) |
                  (remaining[3] << 24));
    if (!gst_tbs6324_write_register (src, RW_DATA, data_value)) {
      GST_ERROR_OBJECT (src, "Failed to write remaining command data");
      return FALSE;
    }
  }

  /* Step 6: Send command (write 0 to COMMAND register) */
  data_value = 0x00000000;
  if (!gst_tbs6324_write_register (src, COMMAND, data_value)) {
    GST_ERROR_OBJECT (src, "Failed to send command");
    return FALSE;
  }

  /* Step 7: Clear DATA_BLKCTRL to 0x00 (when finished, clear bit31) */
  data_value = 0x00000000;
  if (!gst_tbs6324_write_register (src, DATA_BLKCTRL, data_value)) {
    GST_ERROR_OBJECT (src, "Failed to clear DATA_BLKCTRL");
    return FALSE;
  }

  GST_INFO_OBJECT (src, "Encoder configuration sent successfully");
  src->configured = TRUE;

  return TRUE;
}
/* Get firmware version from encoder */
static gchar *
gst_tbs6324_get_firmware_version (GstTBS6324Src * src)
{
  __u32 data_value;
  guchar response[1024];
  guint i;
  __u32 type, len, actual_len;
  gchar *version = NULL;
  
  GST_INFO_OBJECT (src, "Reading firmware version from encoder");
  
  /* Step 1: Write GET_EXT command type */
  data_value = GET_EXT;
  if (!gst_tbs6324_write_register (src, RW_TYPEADDR, data_value)) {
    GST_ERROR_OBJECT (src, "Failed to write GET_EXT command type");
    return NULL;
  }
  
  /* Step 2: Write UDP length (8 bytes for read request) */
  data_value = 8;
  if (!gst_tbs6324_write_register (src, RW_LENADDR, data_value)) {
    GST_ERROR_OBJECT (src, "Failed to write UDP length");
    return NULL;
  }
  
  /* Step 3: Send command (read mode - bit7 set) */
  data_value = 0x00000080;
  if (!gst_tbs6324_write_register (src, COMMAND, data_value)) {
    GST_ERROR_OBJECT (src, "Failed to send read command");
    return NULL;
  }
  
  /* Step 4: Wait for status ACK (simplified - skip for now) */
  /* TODO: Implement proper status checking like check_status_ack() */
  
  /* Step 5: Set DATA_BLKCTRL to enable reading */
  data_value = 0x00000080;
  if (!gst_tbs6324_write_register (src, DATA_BLKCTRL, data_value)) {
    GST_ERROR_OBJECT (src, "Failed to set DATA_BLKCTRL");
    return NULL;
  }
  
  /* Step 6: Read response type */
  type = gst_tbs6324_read_register (src, RW_TYPEADDR);
  GST_DEBUG_OBJECT (src, "Response type: 0x%08x", type);
  
  /* Step 7: Read response length */
  len = gst_tbs6324_read_register (src, RW_LENADDR);
  actual_len = ((len & 0xff00) >> 8) + (len & 0xff) - 8;
  if (actual_len > 1024) {
    actual_len = 1024;
  }
  GST_DEBUG_OBJECT (src, "Response length: %u bytes", actual_len);
  
  /* Step 8: Read response data */
  for (i = 0; i < (actual_len / 4); i++) {
    data_value = gst_tbs6324_read_register (src, RW_DATA);
    response[i*4] = data_value & 0xff;
    response[i*4 + 1] = (data_value >> 8) & 0xff;
    response[i*4 + 2] = (data_value >> 16) & 0xff;
    response[i*4 + 3] = (data_value >> 24) & 0xff;
  }
  if (actual_len % 4) {
    data_value = gst_tbs6324_read_register (src, RW_DATA);
    guint remaining = actual_len % 4;
    for (i = 0; i < remaining; i++) {
      response[(actual_len / 4) * 4 + i] = (data_value >> (i * 8)) & 0xff;
    }
  }
  
  /* Null-terminate the response */
  if (actual_len < 10) {
    response[actual_len] = '\0';
  } else {
    response[actual_len - 1] = '\0';
  }
  
  /* Step 9: Clear DATA_BLKCTRL */
  data_value = 0x00000000;
  if (!gst_tbs6324_write_register (src, DATA_BLKCTRL, data_value)) {
    GST_ERROR_OBJECT (src, "Failed to clear DATA_BLKCTRL");
    return NULL;
  }
  
  GST_DEBUG_OBJECT (src, "Response data: %s", (gchar *)response);
  
  /* Parse version from response */
  /* Response format: &version=v4_20240929_NSDI_OEMF_release&other=values */
  gchar *version_ptr = g_strstr_len ((gchar *)response, actual_len, "version=");
  if (version_ptr) {
    version_ptr += 8; /* Skip "version=" */
    gchar *end_ptr = g_strstr_len (version_ptr, actual_len - (version_ptr - (gchar *)response), "&");
    if (end_ptr) {
      version = g_strndup (version_ptr, end_ptr - version_ptr);
    } else {
      version = g_strdup (version_ptr);
    }
    GST_INFO_OBJECT (src, "Firmware version: %s", version);
  } else {
    GST_WARNING_OBJECT (src, "Could not find version in response");
  }
  
  return version;
}

/* Detect TBS6324 card by checking lspci output */
static gboolean
gst_tbs6324_detect_card (GstTBS6324Src * src)
{
  FILE *fp;
  char line[256];
  gboolean found_tbs6324 = FALSE;
  
  GST_INFO_OBJECT (src, "Detecting TBS6324 card using lspci");
  
  /* Run lspci and grep for 6324 device ID */
  fp = popen ("lspci -nn | grep -i 6324", "r");
  if (fp == NULL) {
    GST_ERROR_OBJECT (src, "Failed to execute lspci command: %s", g_strerror (errno));
    GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND,
        ("TBS6324 card not found"),
        ("Could not execute lspci command to detect TBS6324 card. "
         "This typically means the pciutils package is not installed. "
         "Please install: sudo apt-get install pciutils"));
    return FALSE;
  }

  /* Read lspci output line by line */
  while (fgets (line, sizeof (line), fp) != NULL) {
    GST_TRACE_OBJECT (src, "lspci output: %s", line);
    
    /* Look for TBS6324 device ID */
    /* lspci output format: "XXXX:YY:ZZ.T Multimedia video controller: VendorName DeviceName [XXXX:6324]" */
    if (strstr (line, "6324")) {
      /* Check if it's the right vendor (0x0010 = TurboSight) */
      /* lspci shows device IDs in hex format [vendor:device] */
      if (strstr (line, "[10e6:6324]") || strstr (line, "[10e6:") || strstr (line, "6324")) {
        GST_INFO_OBJECT (src, "Found TBS6324 card in lspci output");
        found_tbs6324 = TRUE;
        break;
      }
    }
  }
  
  pclose (fp);

  if (!found_tbs6324) {
    GST_ELEMENT_ERROR (src, RESOURCE, NOT_FOUND,
        ("TBS6324 card not found"),
        ("Could not find TBS6324 card using lspci command. "
         "This means the TBS6324 hardware is not installed or not detected by the system. "
         "Please verify: 1) TBS6324 card is properly installed in PCIe slot, 2) lspci lists the device, "
         "3) Try: lspci -vvv | grep 6324 to verify hardware detection"));
    return FALSE;
  }

  GST_INFO_OBJECT (src, "TBS6324 card detected successfully");
  return TRUE;
}


/* Open both frontend and DVR devices */
static gboolean
gst_tbs6324_open_devices (GstTBS6324Src * src)
{
  gchar *frontend_path;
  gchar *dvr_path;

  frontend_path = g_strdup_printf ("/dev/dvb/adapter%d/frontend%d",
      src->encoder_number, src->frontend_number);
  dvr_path = g_strdup_printf ("/dev/dvb/adapter%d/dvr%d",
      src->encoder_number, src->frontend_number);

  GST_INFO_OBJECT (src, "Opening devices: frontend=%s, dvr=%s",
      frontend_path, dvr_path);

    /* Detect if TBS6324 card is present before opening devices */
    if (!gst_tbs6324_detect_card (src)) {
      GST_ERROR_OBJECT (src, "Card detection failed");
      g_free (frontend_path);
      g_free (dvr_path);
      return FALSE;
    }

  /* Open frontend device for ioctl configuration */
  src->fd_frontend = open (frontend_path, O_RDWR | O_NONBLOCK);
  if (src->fd_frontend < 0) {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ_WRITE,
        ("Could not open frontend device"),
        ("open %s failed: %s", frontend_path, g_strerror (errno)));
    g_free (frontend_path);
    g_free (dvr_path);
    return FALSE;
  }

  /* Open DVR device for reading MPEG-TS stream */
  src->fd_dvr = open (dvr_path, O_RDONLY | O_NONBLOCK);
  if (src->fd_dvr < 0) {
    GST_ELEMENT_ERROR (src, RESOURCE, OPEN_READ,
        ("Could not open DVR device"),
        ("open %s failed: %s", dvr_path, g_strerror (errno)));
    close (src->fd_frontend);
    src->fd_frontend = -1;
    g_free (frontend_path);
    g_free (dvr_path);
    return FALSE;
  }

  src->frontend_device = frontend_path;
  src->dvr_device = dvr_path;

  GST_INFO_OBJECT (src, "Devices opened successfully (frontend fd=%d, dvr fd=%d)",
      src->fd_frontend, src->fd_dvr);
  
  /* Read firmware version to verify encoder communication */
  gchar *firmware_version = gst_tbs6324_get_firmware_version (src);
  if (firmware_version) {
    GST_INFO_OBJECT (src, "TBS6324 firmware version: %s", firmware_version);
    src->firmware_version = firmware_version;
  } else {
    GST_WARNING_OBJECT (src, "Could not read firmware version, but continuing anyway");
    src->firmware_version = g_strdup ("unknown");
  }


  return TRUE;
}

/* Close both frontend and DVR devices */
static void
gst_tbs6324_close_devices (GstTBS6324Src * src)
{
  if (src->fd_frontend >= 0) {
    GST_INFO_OBJECT (src, "Closing frontend device");
    close (src->fd_frontend);
    src->fd_frontend = -1;
  }

  if (src->fd_dvr >= 0) {
    GST_INFO_OBJECT (src, "Closing DVR device");
    close (src->fd_dvr);
    src->fd_dvr = -1;
  }

  g_free (src->frontend_device);
  src->frontend_device = NULL;

  g_free (src->dvr_device);
  src->dvr_device = NULL;
}

/* Start streaming */
static gboolean
gst_tbs6324_src_start (GstBaseSrc * basesrc)
{
  GstTBS6324Src *src = GST_TBS6324_SRC (basesrc);

  GST_INFO_OBJECT (src, "Starting TBS6324 source on encoder %d", src->encoder_number);

  /* Open both frontend and DVR devices */
  if (!gst_tbs6324_open_devices (src)) {
    return FALSE;
  }

  /* Configure encoder via frontend device */
  if (!gst_tbs6324_configure_encoder (src)) {
    GST_ERROR_OBJECT (src, "Failed to configure encoder");
    gst_tbs6324_close_devices (src);
    return FALSE;
  }

  src->streaming = TRUE;
  src->last_ts = GST_CLOCK_TIME_NONE;

  GST_INFO_OBJECT (src, "TBS6324 source started successfully");

  return TRUE;
}

/* Stop streaming */
static gboolean
gst_tbs6324_src_stop (GstBaseSrc * basesrc)
{
  GstTBS6324Src *src = GST_TBS6324_SRC (basesrc);

  GST_INFO_OBJECT (src, "Stopping TBS6324 source");

  src->streaming = FALSE;
  gst_tbs6324_close_devices (src);

  GST_INFO_OBJECT (src, "TBS6324 source stopped");

  return TRUE;
}

/* Create buffer with data from DVR device */
static GstFlowReturn
gst_tbs6324_src_create (GstPushSrc * pushsrc, GstBuffer ** buf)
{
  GstTBS6324Src *src = GST_TBS6324_SRC (pushsrc);
  GstBuffer *buffer;
  GstMapInfo map;
  ssize_t bytes_read;
  gssize ret;

  if (src->fd_dvr < 0) {
    GST_ERROR_OBJECT (src, "DVR device not open");
    return GST_FLOW_ERROR;
  }

  /* Allocate buffer */
  buffer = gst_buffer_new_and_alloc (src->buffer_size);

  if (!gst_buffer_map (buffer, &map, GST_MAP_WRITE)) {
    GST_ERROR_OBJECT (src, "Failed to map buffer");
    gst_buffer_unref (buffer);
    return GST_FLOW_ERROR;
  }

  /* Read from DVR device (not frontend!) */
  ret = read (src->fd_dvr, map.data, map.size);

  if (ret < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      /* No data available yet, try again */
      gst_buffer_unmap (buffer, &map);
      gst_buffer_unref (buffer);
      return GST_FLOW_OK;
    }
    GST_ERROR_OBJECT (src, "Read error from DVR: %s", g_strerror (errno));
    gst_buffer_unmap (buffer, &map);
    gst_buffer_unref (buffer);
    return GST_FLOW_ERROR;
  }

  bytes_read = (ssize_t) ret;

  if (bytes_read == 0) {
    /* End of stream */
    GST_DEBUG_OBJECT (src, "End of stream");
    gst_buffer_unmap (buffer, &map);
    gst_buffer_unref (buffer);
    return GST_FLOW_EOS;
  }

  /* Resize buffer to actual read size */
  gst_buffer_resize (buffer, 0, bytes_read);
  gst_buffer_unmap (buffer, &map);

  /* Set timestamp */
  if (src->last_ts == GST_CLOCK_TIME_NONE) {
    src->last_ts = gst_clock_get_time (GST_ELEMENT_CLOCK (src));
  }
  GST_BUFFER_PTS (buffer) = src->last_ts;
  GST_BUFFER_DTS (buffer) = GST_CLOCK_TIME_NONE;

  *buf = buffer;

  GST_LOG_OBJECT (src, "Read %zd bytes from DVR device", bytes_read);

  return GST_FLOW_OK;
}

static gboolean
plugin_init (GstPlugin * plugin)
{
#ifdef HAVE_CONFIG_H
  GST_INFO ("TBS6324 Plugin - Built on %s at %s", BUILD_DATE, BUILD_TIME);
#endif
  return gst_element_register (plugin, "tbs6324src", GST_RANK_NONE,
      GST_TYPE_TBS6324_SRC);
}

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    tbs6324,
    "TBS6324 SDI Encoder Source Plugin",
    plugin_init, VERSION, "LGPL", PACKAGE, "http://github.com/TVforME/GStreamer/gsttbs6324")