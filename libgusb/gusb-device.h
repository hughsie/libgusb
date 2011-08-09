/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2011 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2011 Hans de Goede <hdegoede@redhat.com>
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __GUSB_DEVICE_H__
#define __GUSB_DEVICE_H__

#include <glib-object.h>

G_BEGIN_DECLS

#define G_USB_TYPE_DEVICE		(g_usb_device_get_type ())
#define G_USB_DEVICE(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), G_USB_TYPE_DEVICE, GUsbDevice))
#define G_USB_IS_DEVICE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), G_USB_TYPE_DEVICE))
#define G_USB_DEVICE_ERROR		(g_usb_device_error_quark ())

typedef struct _GUsbDevicePrivate	GUsbDevicePrivate;
typedef struct _GUsbDevice		GUsbDevice;
typedef struct _GUsbDeviceClass		GUsbDeviceClass;

/**
 * GUsbDeviceError:
 *
 * The error code.
 **/
typedef enum {
	G_USB_DEVICE_ERROR_INTERNAL
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
GQuark			 g_usb_device_error_quark	(void);

guint8			 g_usb_device_get_bus		(GUsbDevice      *device);
guint8			 g_usb_device_get_address	(GUsbDevice      *device);

#if 0 /* TODO */
GUsbDeviceHandle 	*g_usb_device_get_device_handle	(GUsbDevice	 *device,
							 GError		**err);
#endif

G_END_DECLS

#endif /* __GUSB_DEVICE_H__ */
