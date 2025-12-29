/*
 * gsttbs6324.h - GStreamer plugin for TBS6324 Quad 3G-SDI Encoder
 *
 * Copyright (C) 2025 Robert Hensel VK3DG Australia
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#ifndef __GST_TBS6324_SRC_H__
#define __GST_TBS6324_SRC_H__

#include <gst/gst.h>
#include <gst/base/gstpushsrc.h>

G_BEGIN_DECLS

#define GST_TYPE_TBS6324_SRC \
  (gst_tbs6324_src_get_type())
#define GST_TBS6324_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_TBS6324_SRC,GstTBS6324Src))
#define GST_TBS6324_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_TBS6324_SRC,GstTBS6324SrcClass))
#define GST_IS_TBS6324_SRC(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_TBS6324_SRC))
#define GST_IS_TBS6324_SRC_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_TBS6324_SRC))

typedef struct _GstTBS6324Src GstTBS6324Src;
typedef struct _GstTBS6324SrcClass GstTBS6324SrcClass;

struct _GstTBS6324Src
{
  GstPushSrc parent;

  /* Device handles */
  int encoder_number;        /* Encoder channel (0-3) */
  int frontend_number;       /* Frontend number (usually 0) */
  gchar *frontend_device;    /* Frontend device path */
  gchar *dvr_device;         /* DVR device path */
  int fd_frontend;           /* Frontend device fd for ioctl */
  int fd_dvr;                /* DVR device fd for reading data */

  /* Encoder properties */
  gchar *encoder_type;       /* H264 or H265 */
  int width;                 /* Video width */
  int height;                /* Video height */
  int fps;                   /* Frame rate */
  int bitrate;               /* Video bitrate (kbps) */
  gchar *bitrate_mode;       /* CBR, VBR, or EVBR */
  int gop_interval;          /* GOP interval */
  gchar *profile;            /* H.264 profile */

  /* MPEG-TS properties */
  int pmt_pid;               /* PMT PID */
  int video_pid;             /* Video PID */
  int audio_pid;             /* Audio PID */
  int service_id;            /* Service ID */
  gchar *program_name;       /* Program name */

  /* Audio properties */
  gchar *audio_codec;        /* AAC or MP2 */
  int audio_bitrate;         /* Audio bitrate (bps) */
  gchar *audio_type;         /* AAC type */
  int audio_channel;         /* Audio channel */
  int audio_resample;        /* Resample rate */

  /* Internal state */
  gboolean configured;       /* Encoder configured flag */
  gboolean streaming;        /* Streaming active flag */
  guint buffer_size;         /* Buffer size for reading */
  GstClockTime last_ts;      /* Last timestamp */
  gchar *firmware_version;   /* Firmware version string */
};

struct _GstTBS6324SrcClass
{
  GstPushSrcClass parent_class;
};

GType gst_tbs6324_src_get_type (void);

G_END_DECLS

#endif /* __GST_TBS6324_SRC_H__ */