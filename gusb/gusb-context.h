/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011-2016 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2011 Hans de Goede <hdegoede@redhat.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>

#include <gusb/gusb-device.h>
#include <gusb/gusb-source.h>

G_BEGIN_DECLS

#define G_USB_TYPE_CONTEXT		(g_usb_context_get_type ())
#define G_USB_CONTEXT(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), G_USB_TYPE_CONTEXT, GUsbContext))
#define G_USB_IS_CONTEXT(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), G_USB_TYPE_CONTEXT))
#define G_USB_CONTEXT_ERROR		(g_usb_context_error_quark ())

typedef struct _GUsbContextPrivate	GUsbContextPrivate;
typedef struct _GUsbContext		GUsbContext;
typedef struct _GUsbContextClass	GUsbContextClass;

struct _GUsbContext
{
	 GObject			 parent;
	 GUsbContextPrivate		*priv;
};

struct _GUsbContextClass
{
	GObjectClass			 parent_class;
	void (*device_added)		(GUsbContext		*context,
					 GUsbDevice		*device);
	void (*device_removed)		(GUsbContext		*context,
					 GUsbDevice		*device);
	/*< private >*/
	/*
	 * If adding fields to this struct, remove corresponding
	 * amount of padding to avoid changing overall struct size
	 */
	gchar _gusb_reserved[62];
};

typedef enum {
	G_USB_CONTEXT_ERROR_INTERNAL
} GUsbContextError;

/**
 * GUsbContextFlags:
 *
 * The flags to use for the context.
 **/
typedef enum {
	G_USB_CONTEXT_FLAGS_NONE		= 0,
	G_USB_CONTEXT_FLAGS_AUTO_OPEN_DEVICES	= 1 << 0,
	/*< private >*/
	G_USB_CONTEXT_FLAGS_LAST
} GUsbContextFlags;

GType		 g_usb_context_get_type			(void);
GQuark		 g_usb_context_error_quark		(void);

GUsbContext	*g_usb_context_new			(GError		**error);

void		 g_usb_context_set_flags		(GUsbContext	*context,
							 GUsbContextFlags flags);
GUsbContextFlags g_usb_context_get_flags		(GUsbContext	*context);

G_DEPRECATED
GUsbSource	*g_usb_context_get_source		(GUsbContext	*context,
							 GMainContext	*main_ctx);
GMainContext	*g_usb_context_get_main_context		(GUsbContext	*context);
void		 g_usb_context_set_main_context		(GUsbContext	*context,
							 GMainContext	*main_ctx);

void		 g_usb_context_enumerate		(GUsbContext	*context);

void		 g_usb_context_set_debug		(GUsbContext	*context,
							 GLogLevelFlags	 flags);
GPtrArray	*g_usb_context_get_devices		(GUsbContext	*context);

GUsbDevice	*g_usb_context_find_by_bus_address	(GUsbContext	*context,
							 guint8		 bus,
							 guint8		 address,
							 GError		**error);

GUsbDevice	*g_usb_context_find_by_vid_pid		(GUsbContext	*context,
							 guint16	 vid,
							 guint16	 pid,
							 GError		**error);
GUsbDevice	*g_usb_context_find_by_platform_id	(GUsbContext	*context,
							 const gchar	*platform_id,
							 GError		**error);

GUsbDevice	*g_usb_context_wait_for_replug		(GUsbContext	*context,
							 GUsbDevice	*device,
							 guint		 timeout_ms,
							 GError		**error);

G_END_DECLS
