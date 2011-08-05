/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2011 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#ifndef __GUSB_DEVICE_H__
#define __GUSB_DEVICE_H__

#include <glib-object.h>

#include <libusb-1.0/libusb.h>

G_BEGIN_DECLS

#define GUSB_TYPE_DEVICE		(g_usb_device_get_type ())
#define GUSB_DEVICE(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), GUSB_TYPE_DEVICE, GUsbDevice))
#define GUSB_IS_DEVICE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GUSB_TYPE_DEVICE))

typedef struct _GUsbDevicePrivate	GUsbDevicePrivate;
typedef struct _GUsbDevice		GUsbDevice;
typedef struct _GUsbDeviceClass	GUsbDeviceClass;

/* dummy */
#define GUSB_DEVICE_ERROR	1

/* only libusb > 1.0.8 has libusb_strerror */
#ifndef HAVE_NEW_USB
#define	libusb_strerror(f1)		"unknown"
#endif

/**
 * CdSensorError:
 *
 * The error code.
 **/
typedef enum {
	GUSB_DEVICE_ERROR_INTERNAL
} GUsbDeviceError;

struct _GUsbDevice
{
	 GObject			 parent;
	 GUsbDevicePrivate		*priv;
};

struct _GUsbDeviceClass
{
	GObjectClass			 parent_class;
};

GType			 g_usb_device_get_type		(void);

gboolean		 g_usb_device_connect		(GUsbDevice	*usb,
							 guint		 vendor_id,
							 guint		 product_id,
							 guint		 configuration,
							 guint		 interface,
							 GError		**error);
gboolean		 g_usb_device_disconnect	(GUsbDevice	*usb,
							 GError		**error);

libusb_device_handle	*g_usb_device_get_handle	(GUsbDevice	*usb);
GUsbDevice		*g_usb_device_new		(void);

G_END_DECLS

#endif /* __GUSB_DEVICE_H__ */

