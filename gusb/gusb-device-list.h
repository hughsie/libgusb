/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Hans de Goede <hdegoede@redhat.com>
 * Copyright (C) 2011-2014 Richard Hughes <richard@hughsie.com>
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

#ifndef __GUSB_DEVICE_LIST_H__
#define __GUSB_DEVICE_LIST_H__

#include <glib-object.h>

#include <gusb/gusb-context.h>
#include <gusb/gusb-device.h>

G_BEGIN_DECLS

#define G_USB_TYPE_DEVICE_LIST		(g_usb_device_list_get_type ())
#define G_USB_DEVICE_LIST(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), G_USB_TYPE_DEVICE_LIST, GUsbDeviceList))
#define G_USB_IS_DEVICE_LIST(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), G_USB_TYPE_DEVICE_LIST))

typedef struct _GUsbDeviceListPrivate	GUsbDeviceListPrivate;
typedef struct _GUsbDeviceList		GUsbDeviceList;
typedef struct _GUsbDeviceListClass	GUsbDeviceListClass;

struct _GUsbDeviceList
{
	 GObject			 parent;
	 GUsbDeviceListPrivate		*priv;
};

struct _GUsbDeviceListClass
{
	GObjectClass			 parent_class;
	/* Signals */
	void (*device_added)		(GUsbDeviceList		*list,
					 GUsbDevice		*device);
	void (*device_removed)		(GUsbDeviceList		*list,
					 GUsbDevice		*device);
	/*< private >*/
	/*
	 * If adding fields to this struct, remove corresponding
	 * amount of padding to avoid changing overall struct size
	 */
	gchar _gusb_reserved[64];
};

GType			 g_usb_device_list_get_type (void);

G_DEPRECATED_FOR(g_usb_context_new)
GUsbDeviceList		*g_usb_device_list_new			(GUsbContext	*context);

G_DEPRECATED
void			 g_usb_device_list_coldplug		(GUsbDeviceList	*list);

G_DEPRECATED_FOR(g_usb_context_get_devices)
GPtrArray		*g_usb_device_list_get_devices		(GUsbDeviceList	*list);

G_DEPRECATED_FOR(g_usb_context_find_by_bus_address)
GUsbDevice		*g_usb_device_list_find_by_bus_address	(GUsbDeviceList	*list,
								 guint8		 bus,
								 guint8		 address,
								 GError		**error);

G_DEPRECATED_FOR(g_usb_context_find_by_vid_pid)
GUsbDevice		*g_usb_device_list_find_by_vid_pid	(GUsbDeviceList	*list,
								 guint16	 vid,
								 guint16	 pid,
								 GError		**error);

G_END_DECLS

#endif /* __GUSB_DEVICE_LIST_H__ */
