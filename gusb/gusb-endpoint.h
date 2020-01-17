/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Richard Hughes <richard@hughsie.com>
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

#ifndef __GUSB_ENDPOINT_H__
#define __GUSB_ENDPOINT_H__

#include <glib-object.h>
#include <gio/gio.h>
#include <gusb-device.h>

G_BEGIN_DECLS

#define G_USB_TYPE_ENDPOINT (g_usb_endpoint_get_type ())
G_DECLARE_FINAL_TYPE (GUsbEndpoint, g_usb_endpoint, G_USB, ENDPOINT, GObject)

guint8			g_usb_endpoint_get_kind			(GUsbEndpoint *endpoint);
guint16			g_usb_endpoint_get_maximum_packet_size 	(GUsbEndpoint *endpoint);
guint8 			g_usb_endpoint_get_polling_interval	(GUsbEndpoint *endpoint);
guint8 			g_usb_endpoint_get_refresh		(GUsbEndpoint *endpoint);
guint8 			g_usb_endpoint_get_synch_address	(GUsbEndpoint *endpoint);
guint8		 	g_usb_endpoint_get_address		(GUsbEndpoint *endpoint);
guint8		 	g_usb_endpoint_get_number		(GUsbEndpoint *endpoint);
GUsbDeviceDirection	g_usb_endpoint_get_direction		(GUsbEndpoint *endpoint);
GBytes *		g_usb_endpoint_get_extra		(GUsbEndpoint *endpoint);

G_END_DECLS

#endif /* __GUSB_ENDPOINT_H__ */
