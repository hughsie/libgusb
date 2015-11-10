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

#define G_USB_TYPE_INTERFACE		(g_usb_interface_get_type ())
#define G_USB_INTERFACE(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), G_USB_TYPE_INTERFACE, GUsbInterface))
#define G_USB_IS_INTERFACE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), G_USB_TYPE_INTERFACE))

typedef struct _GUsbInterfacePrivate	GUsbInterfacePrivate;
typedef struct _GUsbInterface		GUsbInterface;
typedef struct _GUsbInterfaceClass	GUsbInterfaceClass;

struct _GUsbInterface
{
	 GObject			 parent;
	 GUsbInterfacePrivate		*priv;
};

struct _GUsbInterfaceClass
{
	GObjectClass			 parent_class;
	/*< private >*/
	/*
	 * If adding fields to this struct, remove corresponding
	 * amount of padding to avoid changing overall struct size
	 */
	gpointer _gusb_reserved[31];
};

GType		 g_usb_interface_get_type	(void);

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
