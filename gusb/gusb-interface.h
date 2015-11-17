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

#ifndef __GUSB_INTERFACE_H__
#define __GUSB_INTERFACE_H__

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define G_USB_TYPE_INTERFACE (g_usb_interface_get_type ())
G_DECLARE_FINAL_TYPE (GUsbInterface, g_usb_interface, G_USB, INTERFACE, GObject)

guint8		 g_usb_interface_get_length	(GUsbInterface	*interface);
guint8		 g_usb_interface_get_kind	(GUsbInterface	*interface);
guint8		 g_usb_interface_get_number	(GUsbInterface	*interface);
guint8		 g_usb_interface_get_alternate	(GUsbInterface	*interface);
guint8		 g_usb_interface_get_class	(GUsbInterface	*interface);
guint8		 g_usb_interface_get_subclass	(GUsbInterface	*interface);
guint8		 g_usb_interface_get_protocol	(GUsbInterface	*interface);
guint8		 g_usb_interface_get_index	(GUsbInterface	*interface);
GBytes		*g_usb_interface_get_extra	(GUsbInterface	*interface);

G_END_DECLS

#endif /* __GUSB_INTERFACE_H__ */
