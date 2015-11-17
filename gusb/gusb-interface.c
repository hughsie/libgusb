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

/**
 * SECTION:gusb-interface
 * @short_description: GLib wrapper around a USB interface.
 *
 * This object is a thin glib wrapper around a libusb_interface_descriptor.
 *
 * All the data is copied when the object is created and the original
 * descriptor can be destoyed any at point.
 */

#include "config.h"

#include <string.h>

#include "gusb-interface.h"
#include "gusb-interface-private.h"

struct _GUsbInterface
{
	GObject parent_instance;

	struct libusb_interface_descriptor iface;
	GBytes *extra;
};

G_DEFINE_TYPE (GUsbInterface, g_usb_interface, G_TYPE_OBJECT)

static void
g_usb_interface_finalize (GObject *object)
{
	GUsbInterface *interface = G_USB_INTERFACE (object);

	g_bytes_unref (interface->extra);

	G_OBJECT_CLASS (g_usb_interface_parent_class)->finalize (object);
}

static void
g_usb_interface_class_init (GUsbInterfaceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = g_usb_interface_finalize;
}

static void
g_usb_interface_init (GUsbInterface *interface)
{
}

/**
 * _g_usb_interface_new:
 *
 * Return value: a new #GUsbInterface object.
 *
 * Since: 0.2.8
 **/
GUsbInterface *
_g_usb_interface_new (const struct libusb_interface_descriptor *iface)
{
	GUsbInterface *interface;
	interface = g_object_new (G_USB_TYPE_INTERFACE, NULL);

	/* copy the data */
	memcpy (&interface->iface,
		iface,
		sizeof (struct libusb_interface_descriptor));
	interface->extra = g_bytes_new (iface->extra, iface->extra_length);

	return G_USB_INTERFACE (interface);
}

/**
 * g_usb_interface_get_length:
 * @interface: a #GUsbInterface
 *
 * Gets the USB bus number for the interface.
 *
 * Return value: The 8-bit bus number
 *
 * Since: 0.2.8
 **/
guint8
g_usb_interface_get_length (GUsbInterface *interface)
{
	g_return_val_if_fail (G_USB_IS_INTERFACE (interface), 0);
	return interface->iface.bLength;
}

/**
 * g_usb_interface_get_kind:
 * @interface: a #GUsbInterface
 *
 * Gets the type of interface.
 *
 * Return value: The 8-bit address
 *
 * Since: 0.2.8
 **/
guint8
g_usb_interface_get_kind (GUsbInterface *interface)
{
	g_return_val_if_fail (G_USB_IS_INTERFACE (interface), 0);
	return interface->iface.bDescriptorType;
}

/**
 * g_usb_interface_get_number:
 * @interface: a #GUsbInterface
 *
 * Gets the interface number.
 *
 * Return value: The interface ID
 *
 * Since: 0.2.8
 **/
guint8
g_usb_interface_get_number (GUsbInterface *interface)
{
	g_return_val_if_fail (G_USB_IS_INTERFACE (interface), 0);
	return interface->iface.bInterfaceNumber;
}

/**
 * g_usb_interface_get_alternate:
 * @interface: a #GUsbInterface
 *
 * Gets the alternate setting for the interface.
 *
 * Return value: alt setting, typically zero.
 *
 * Since: 0.2.8
 **/
guint8
g_usb_interface_get_alternate (GUsbInterface *interface)
{
	g_return_val_if_fail (G_USB_IS_INTERFACE (interface), 0);
	return interface->iface.bAlternateSetting;
}

/**
 * g_usb_interface_get_class:
 * @interface: a #GUsbInterface
 *
 * Gets the interface class, typically a #GUsbInterfaceClassCode.
 *
 * Return value: a interface class number, e.g. 0x09 is a USB hub.
 *
 * Since: 0.2.8
 **/
guint8
g_usb_interface_get_class (GUsbInterface *interface)
{
	g_return_val_if_fail (G_USB_IS_INTERFACE (interface), 0);
	return interface->iface.bInterfaceClass;
}

/**
 * g_usb_interface_get_subclass:
 * @interface: a #GUsbInterface
 *
 * Gets the interface subclass qualified by the class number.
 * See g_usb_interface_get_class().
 *
 * Return value: a interface subclass number.
 *
 * Since: 0.2.8
 **/
guint8
g_usb_interface_get_subclass (GUsbInterface *interface)
{
	g_return_val_if_fail (G_USB_IS_INTERFACE (interface), 0);
	return interface->iface.bInterfaceSubClass;
}

/**
 * g_usb_interface_get_protocol:
 * @interface: a #GUsbInterface
 *
 * Gets the interface protocol qualified by the class and subclass numbers.
 * See g_usb_interface_get_class() and g_usb_interface_get_subclass().
 *
 * Return value: a interface protocol number.
 *
 * Since: 0.2.8
 **/
guint8
g_usb_interface_get_protocol (GUsbInterface *interface)
{
	g_return_val_if_fail (G_USB_IS_INTERFACE (interface), 0);
	return interface->iface.bInterfaceProtocol;
}

/**
 * g_usb_interface_get_index:
 * @interface: a #GUsbInterface
 *
 * Gets the index for the string descriptor.
 *
 * Return value: a string descriptor index.
 *
 * Since: 0.2.8
 **/
guint8
g_usb_interface_get_index (GUsbInterface *interface)
{
	g_return_val_if_fail (G_USB_IS_INTERFACE (interface), 0);
	return interface->iface.iInterface;
}

/**
 * g_usb_interface_get_extra:
 * @interface: a #GUsbInterface
 *
 * Gets any extra data from the interface.
 *
 * Return value: (transfer none): a #GBytes, or %NULL for failure
 *
 * Since: 0.2.8
 **/
GBytes *
g_usb_interface_get_extra (GUsbInterface *interface)
{
	g_return_val_if_fail (G_USB_IS_INTERFACE (interface), NULL);
	return interface->extra;
}
