/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2020 Emmanuel Pacaud <emmanuel@gnome.org>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include <glib-object.h>
#include <gio/gio.h>
#include <gusb/gusb-device.h>

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
