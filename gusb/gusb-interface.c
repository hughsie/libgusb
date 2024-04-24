/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2015 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2020 Emmanuel Pacaud <emmanuel@gnome.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
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
#include "gusb-json-common.h"

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

	if (self->extra != NULL)
		g_bytes_unref(self->extra);
	if (self->endpoints != NULL)
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

gboolean
_g_usb_interface_load(GUsbInterface *self, JsonObject *json_object, GError **error)
{
	const gchar *str;

	g_return_val_if_fail(G_USB_IS_INTERFACE(self), FALSE);
	g_return_val_if_fail(json_object != NULL, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, -1);

	/* optional properties */
	self->iface.bLength = json_object_get_int_member_with_default(json_object, "Length", 0x0);
	self->iface.bDescriptorType =
	    json_object_get_int_member_with_default(json_object, "DescriptorType", 0x0);
	self->iface.bInterfaceNumber =
	    json_object_get_int_member_with_default(json_object, "InterfaceNumber", 0x0);
	self->iface.bAlternateSetting =
	    json_object_get_int_member_with_default(json_object, "AlternateSetting", 0x0);
	self->iface.bInterfaceClass =
	    json_object_get_int_member_with_default(json_object, "InterfaceClass", 0x0);
	self->iface.bInterfaceSubClass =
	    json_object_get_int_member_with_default(json_object, "InterfaceSubClass", 0x0);
	self->iface.bInterfaceProtocol =
	    json_object_get_int_member_with_default(json_object, "InterfaceProtocol", 0x0);
	self->iface.iInterface =
	    json_object_get_int_member_with_default(json_object, "Interface", 0x0);

	/* array of endpoints */
	if (json_object_has_member(json_object, "UsbEndpoints")) {
		JsonArray *json_array = json_object_get_array_member(json_object, "UsbEndpoints");
		self->endpoints = g_ptr_array_new_with_free_func(g_object_unref);
		for (guint i = 0; i < json_array_get_length(json_array); i++) {
			JsonNode *node_tmp = json_array_get_element(json_array, i);
			JsonObject *obj_tmp = json_node_get_object(node_tmp);
			g_autoptr(GUsbEndpoint) endpoint = g_object_new(G_USB_TYPE_ENDPOINT, NULL);
			if (!_g_usb_endpoint_load(endpoint, obj_tmp, error))
				return FALSE;
			g_ptr_array_add(self->endpoints, g_object_ref(endpoint));
		}
	}

	/* extra data */
	str = json_object_get_string_member_with_default(json_object, "ExtraData", NULL);
	if (str != NULL) {
		gsize bufsz = 0;
		g_autofree guchar *buf = g_base64_decode(str, &bufsz);
		if (self->extra != NULL)
			g_bytes_unref(self->extra);
		self->extra = g_bytes_new_take(g_steal_pointer(&buf), bufsz);
	}

	/* success */
	return TRUE;
}

gboolean
_g_usb_interface_save(GUsbInterface *self, JsonBuilder *json_builder, GError **error)
{
	g_return_val_if_fail(G_USB_IS_INTERFACE(self), FALSE);
	g_return_val_if_fail(json_builder != NULL, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	/* start */
	json_builder_begin_object(json_builder);

	/* optional properties */
	if (self->iface.bLength != 0) {
		json_builder_set_member_name(json_builder, "Length");
		json_builder_add_int_value(json_builder, self->iface.bLength);
	}
	if (self->iface.bDescriptorType != 0) {
		json_builder_set_member_name(json_builder, "DescriptorType");
		json_builder_add_int_value(json_builder, self->iface.bDescriptorType);
	}
	if (self->iface.bInterfaceNumber != 0) {
		json_builder_set_member_name(json_builder, "InterfaceNumber");
		json_builder_add_int_value(json_builder, self->iface.bInterfaceNumber);
	}
	if (self->iface.bAlternateSetting != 0) {
		json_builder_set_member_name(json_builder, "AlternateSetting");
		json_builder_add_int_value(json_builder, self->iface.bAlternateSetting);
	}
	if (self->iface.bInterfaceClass != 0) {
		json_builder_set_member_name(json_builder, "InterfaceClass");
		json_builder_add_int_value(json_builder, self->iface.bInterfaceClass);
	}
	if (self->iface.bInterfaceSubClass != 0) {
		json_builder_set_member_name(json_builder, "InterfaceSubClass");
		json_builder_add_int_value(json_builder, self->iface.bInterfaceSubClass);
	}
	if (self->iface.bInterfaceProtocol != 0) {
		json_builder_set_member_name(json_builder, "InterfaceProtocol");
		json_builder_add_int_value(json_builder, self->iface.bInterfaceProtocol);
	}
	if (self->iface.iInterface != 0) {
		json_builder_set_member_name(json_builder, "Interface");
		json_builder_add_int_value(json_builder, self->iface.iInterface);
	}

	/* array of endpoints */
	if (self->endpoints != NULL && self->endpoints->len > 0) {
		json_builder_set_member_name(json_builder, "UsbEndpoints");
		json_builder_begin_array(json_builder);
		for (guint i = 0; i < self->endpoints->len; i++) {
			GUsbEndpoint *endpoint = g_ptr_array_index(self->endpoints, i);
			if (!_g_usb_endpoint_save(endpoint, json_builder, error))
				return FALSE;
		}
		json_builder_end_array(json_builder);
	}

	/* extra data */
	if (self->extra != NULL && g_bytes_get_size(self->extra) > 0) {
		g_autofree gchar *str = g_base64_encode(g_bytes_get_data(self->extra, NULL),
							g_bytes_get_size(self->extra));
		json_builder_set_member_name(json_builder, "ExtraData");
		json_builder_add_string_value(json_builder, str);
	}

	/* success */
	json_builder_end_object(json_builder);
	return TRUE;
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
 * Return value: (transfer container) (element-type GUsbEndpoint): an array of endpoints,
 * or %NULL on failure.
 *
 * Since: 0.3.3
 **/
GPtrArray *
g_usb_interface_get_endpoints(GUsbInterface *self)
{
	g_return_val_if_fail(G_USB_IS_INTERFACE(self), NULL);
	if (self->endpoints == NULL)
		return NULL;
	return g_ptr_array_ref(self->endpoints);
}
