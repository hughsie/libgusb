/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2020 Emmanuel Pacaud <emmanuel@gnome.org>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

/**
 * SECTION:gusb-interface
 * @short_description: GLib wrapper around a USB interface.
 *
 * This object is a thin glib wrapper around a libusb_interface_descriptor.
 *
 * All the data is copied when the object is created and the original
 * descriptor can be destroyed any at point.
 */

#include "config.h"

#include <string.h>

#include "gusb-endpoint-private.h"
#include "gusb-interface-private.h"

struct _GUsbInterface {
	GObject parent_instance;

	struct libusb_interface_descriptor iface;
	GBytes *extra;

	GPtrArray *endpoints;
};

G_DEFINE_TYPE(GUsbInterface, g_usb_interface, G_TYPE_OBJECT)

static void
g_usb_interface_finalize(GObject *object)
{
	GUsbInterface *self = G_USB_INTERFACE(object);

	g_bytes_unref(self->extra);
	g_ptr_array_unref(self->endpoints);

	G_OBJECT_CLASS(g_usb_interface_parent_class)->finalize(object);
}

static void
g_usb_interface_class_init(GUsbInterfaceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->finalize = g_usb_interface_finalize;
}

static void
g_usb_interface_init(GUsbInterface *self)
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
_g_usb_interface_new(const struct libusb_interface_descriptor *iface)
{
	GUsbInterface *self;
	self = g_object_new(G_USB_TYPE_INTERFACE, NULL);

	/* copy the data */
	memcpy(&self->iface, iface, sizeof(struct libusb_interface_descriptor));
	self->extra = g_bytes_new(iface->extra, iface->extra_length);

	self->endpoints = g_ptr_array_new_with_free_func(g_object_unref);
	for (guint i = 0; i < iface->bNumEndpoints; i++)
		g_ptr_array_add(self->endpoints, _g_usb_endpoint_new(&iface->endpoint[i]));

	return G_USB_INTERFACE(self);
}

/**
 * g_usb_interface_get_length:
 * @self: a #GUsbInterface
 *
 * Gets the USB bus number for the interface.
 *
 * Return value: The 8-bit bus number
 *
 * Since: 0.2.8
 **/
guint8
g_usb_interface_get_length(GUsbInterface *self)
{
	g_return_val_if_fail(G_USB_IS_INTERFACE(self), 0);
	return self->iface.bLength;
}

/**
 * g_usb_interface_get_kind:
 * @self: a #GUsbInterface
 *
 * Gets the type of interface.
 *
 * Return value: The 8-bit address
 *
 * Since: 0.2.8
 **/
guint8
g_usb_interface_get_kind(GUsbInterface *self)
{
	g_return_val_if_fail(G_USB_IS_INTERFACE(self), 0);
	return self->iface.bDescriptorType;
}

/**
 * g_usb_interface_get_number:
 * @self: a #GUsbInterface
 *
 * Gets the interface number.
 *
 * Return value: The interface ID
 *
 * Since: 0.2.8
 **/
guint8
g_usb_interface_get_number(GUsbInterface *self)
{
	g_return_val_if_fail(G_USB_IS_INTERFACE(self), 0);
	return self->iface.bInterfaceNumber;
}

/**
 * g_usb_interface_get_alternate:
 * @self: a #GUsbInterface
 *
 * Gets the alternate setting for the interface.
 *
 * Return value: alt setting, typically zero.
 *
 * Since: 0.2.8
 **/
guint8
g_usb_interface_get_alternate(GUsbInterface *self)
{
	g_return_val_if_fail(G_USB_IS_INTERFACE(self), 0);
	return self->iface.bAlternateSetting;
}

/**
 * g_usb_interface_get_class:
 * @self: a #GUsbInterface
 *
 * Gets the interface class, typically a #GUsbInterfaceClassCode.
 *
 * Return value: a interface class number, e.g. 0x09 is a USB hub.
 *
 * Since: 0.2.8
 **/
guint8
g_usb_interface_get_class(GUsbInterface *self)
{
	g_return_val_if_fail(G_USB_IS_INTERFACE(self), 0);
	return self->iface.bInterfaceClass;
}

/**
 * g_usb_interface_get_subclass:
 * @self: a #GUsbInterface
 *
 * Gets the interface subclass qualified by the class number.
 * See g_usb_interface_get_class().
 *
 * Return value: a interface subclass number.
 *
 * Since: 0.2.8
 **/
guint8
g_usb_interface_get_subclass(GUsbInterface *self)
{
	g_return_val_if_fail(G_USB_IS_INTERFACE(self), 0);
	return self->iface.bInterfaceSubClass;
}

/**
 * g_usb_interface_get_protocol:
 * @self: a #GUsbInterface
 *
 * Gets the interface protocol qualified by the class and subclass numbers.
 * See g_usb_interface_get_class() and g_usb_interface_get_subclass().
 *
 * Return value: a interface protocol number.
 *
 * Since: 0.2.8
 **/
guint8
g_usb_interface_get_protocol(GUsbInterface *self)
{
	g_return_val_if_fail(G_USB_IS_INTERFACE(self), 0);
	return self->iface.bInterfaceProtocol;
}

/**
 * g_usb_interface_get_index:
 * @self: a #GUsbInterface
 *
 * Gets the index for the string descriptor.
 *
 * Return value: a string descriptor index.
 *
 * Since: 0.2.8
 **/
guint8
g_usb_interface_get_index(GUsbInterface *self)
{
	g_return_val_if_fail(G_USB_IS_INTERFACE(self), 0);
	return self->iface.iInterface;
}

/**
 * g_usb_interface_get_extra:
 * @self: a #GUsbInterface
 *
 * Gets any extra data from the interface.
 *
 * Return value: (transfer none): a #GBytes, or %NULL for failure
 *
 * Since: 0.2.8
 **/
GBytes *
g_usb_interface_get_extra(GUsbInterface *self)
{
	g_return_val_if_fail(G_USB_IS_INTERFACE(self), NULL);
	return self->extra;
}

/**
 * g_usb_interface_get_endpoints:
 * @self: a #GUsbInterface
 *
 * Gets interface endpoints.
 *
 * Return value: (transfer container) (element-type GUsbEndpoint): an array of endpoints, or %NULL
 *on failure
 *
 * Since: 0.3.3
 **/
GPtrArray *
g_usb_interface_get_endpoints(GUsbInterface *self)
{
	g_return_val_if_fail(G_USB_IS_INTERFACE(self), NULL);
	return g_ptr_array_ref(self->endpoints);
}
