/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2020 Emmanuel Pacaud <emmanuel@gnome.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

/**
 * SECTION:gusb-endpoint
 * @short_description: GLib wrapper around a USB endpoint.
 *
 * This object is a thin glib wrapper around a libusb_endpoint_descriptor.
 *
 * All the data is copied when the object is created and the original
 * descriptor can be destroyed any at point.
 */

#include "config.h"

#include <string.h>

#include "gusb-endpoint-private.h"
#include "gusb-json-common.h"

struct _GUsbEndpoint {
	GObject parent_instance;

	struct libusb_endpoint_descriptor endpoint_descriptor;
	GBytes *extra;
};

G_DEFINE_TYPE(GUsbEndpoint, g_usb_endpoint, G_TYPE_OBJECT)

static void
g_usb_endpoint_finalize(GObject *object)
{
	GUsbEndpoint *self = G_USB_ENDPOINT(object);

	g_bytes_unref(self->extra);

	G_OBJECT_CLASS(g_usb_endpoint_parent_class)->finalize(object);
}

static void
g_usb_endpoint_class_init(GUsbEndpointClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->finalize = g_usb_endpoint_finalize;
}

static void
g_usb_endpoint_init(GUsbEndpoint *self)
{
}

gboolean
_g_usb_endpoint_load(GUsbEndpoint *self, JsonObject *json_object, GError **error)
{
	const gchar *str;

	g_return_val_if_fail(G_USB_IS_ENDPOINT(self), FALSE);
	g_return_val_if_fail(json_object != NULL, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	/* optional properties */
	self->endpoint_descriptor.bDescriptorType =
	    json_object_get_int_member_with_default(json_object, "DescriptorType", 0x0);
	self->endpoint_descriptor.bEndpointAddress =
	    json_object_get_int_member_with_default(json_object, "EndpointAddress", 0x0);
	self->endpoint_descriptor.bRefresh =
	    json_object_get_int_member_with_default(json_object, "Refresh", 0x0);
	self->endpoint_descriptor.bInterval =
	    json_object_get_int_member_with_default(json_object, "Interval", 0x0);
	self->endpoint_descriptor.bSynchAddress =
	    json_object_get_int_member_with_default(json_object, "SynchAddress", 0x0);
	self->endpoint_descriptor.wMaxPacketSize =
	    json_object_get_int_member_with_default(json_object, "MaxPacketSize", 0x0);

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
_g_usb_endpoint_save(GUsbEndpoint *self, JsonBuilder *json_builder, GError **error)
{
	g_return_val_if_fail(G_USB_IS_ENDPOINT(self), FALSE);
	g_return_val_if_fail(json_builder != NULL, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	/* start */
	json_builder_begin_object(json_builder);

	/* optional properties */
	if (self->endpoint_descriptor.bDescriptorType != 0) {
		json_builder_set_member_name(json_builder, "DescriptorType");
		json_builder_add_int_value(json_builder, self->endpoint_descriptor.bDescriptorType);
	}
	if (self->endpoint_descriptor.bEndpointAddress != 0) {
		json_builder_set_member_name(json_builder, "EndpointAddress");
		json_builder_add_int_value(json_builder,
					   self->endpoint_descriptor.bEndpointAddress);
	}
	if (self->endpoint_descriptor.bRefresh != 0) {
		json_builder_set_member_name(json_builder, "Refresh");
		json_builder_add_int_value(json_builder, self->endpoint_descriptor.bRefresh);
	}
	if (self->endpoint_descriptor.bInterval != 0) {
		json_builder_set_member_name(json_builder, "Interval");
		json_builder_add_int_value(json_builder, self->endpoint_descriptor.bInterval);
	}
	if (self->endpoint_descriptor.bSynchAddress != 0) {
		json_builder_set_member_name(json_builder, "SynchAddress");
		json_builder_add_int_value(json_builder, self->endpoint_descriptor.bSynchAddress);
	}
	if (self->endpoint_descriptor.wMaxPacketSize != 0) {
		json_builder_set_member_name(json_builder, "MaxPacketSize");
		json_builder_add_int_value(json_builder, self->endpoint_descriptor.wMaxPacketSize);
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
 * _g_usb_endpoint_new:
 *
 * Return value: a new #GUsbEndpoint object.
 *
 * Since: 0.3.3
 **/
GUsbEndpoint *
_g_usb_endpoint_new(const struct libusb_endpoint_descriptor *endpoint_descriptor)
{
	GUsbEndpoint *self;
	self = g_object_new(G_USB_TYPE_ENDPOINT, NULL);

	/* copy the data */
	memcpy(&self->endpoint_descriptor,
	       endpoint_descriptor,
	       sizeof(struct libusb_endpoint_descriptor));
	self->extra = g_bytes_new(endpoint_descriptor->extra, endpoint_descriptor->extra_length);

	return G_USB_ENDPOINT(self);
}

/**
 * g_usb_endpoint_get_kind:
 * @self: a #GUsbEndpoint
 *
 * Gets the type of endpoint.
 *
 * Return value: The 8-bit type
 *
 * Since: 0.3.3
 **/
guint8
g_usb_endpoint_get_kind(GUsbEndpoint *self)
{
	g_return_val_if_fail(G_USB_IS_ENDPOINT(self), 0);
	return self->endpoint_descriptor.bDescriptorType;
}

/**
 * g_usb_endpoint_get_maximum_packet_size:
 * @self: a #GUsbEndpoint
 *
 * Gets the maximum packet size this endpoint is capable of sending/receiving.
 *
 * Return value: The maximum packet size
 *
 * Since: 0.3.3
 **/
guint16
g_usb_endpoint_get_maximum_packet_size(GUsbEndpoint *self)
{
	g_return_val_if_fail(G_USB_IS_ENDPOINT(self), 0);
	return self->endpoint_descriptor.wMaxPacketSize;
}

/**
 * g_usb_endpoint_get_polling_interval:
 * @self: a #GUsbEndpoint
 *
 * Gets the endpoint polling interval.
 *
 * Return value: The endpoint polling interval
 *
 * Since: 0.3.3
 **/
guint8
g_usb_endpoint_get_polling_interval(GUsbEndpoint *self)
{
	g_return_val_if_fail(G_USB_IS_ENDPOINT(self), 0);
	return self->endpoint_descriptor.bInterval;
}

/**
 * g_usb_endpoint_get_refresh:
 * @self: a #GUsbEndpoint
 *
 * Gets the rate at which synchronization feedback is provided, for audio device only.
 *
 * Return value: The endpoint refresh
 *
 * Since: 0.3.3
 **/
guint8
g_usb_endpoint_get_refresh(GUsbEndpoint *self)
{
	g_return_val_if_fail(G_USB_IS_ENDPOINT(self), 0);
	return self->endpoint_descriptor.bRefresh;
}

/**
 * g_usb_endpoint_get_synch_address:
 * @self: a #GUsbEndpoint
 *
 * Gets the address if the synch endpoint, for audio device only.
 *
 * Return value: The synch endpoint address
 *
 * Since: 0.3.3
 **/
guint8
g_usb_endpoint_get_synch_address(GUsbEndpoint *self)
{
	g_return_val_if_fail(G_USB_IS_ENDPOINT(self), 0);
	return self->endpoint_descriptor.bSynchAddress;
}

/**
 * g_usb_endpoint_get_address:
 * @self: a #GUsbEndpoint
 *
 * Gets the address of the endpoint.
 *
 * Return value: The 4-bit endpoint address
 *
 * Since: 0.3.3
 **/
guint8
g_usb_endpoint_get_address(GUsbEndpoint *self)
{
	g_return_val_if_fail(G_USB_IS_ENDPOINT(self), 0);
	return self->endpoint_descriptor.bEndpointAddress;
}

/**
 * g_usb_endpoint_get_number:
 * @self: a #GUsbEndpoint
 *
 * Gets the number part of endpoint address.
 *
 * Return value: The lower 4-bit of endpoint address
 *
 * Since: 0.3.3
 **/
guint8
g_usb_endpoint_get_number(GUsbEndpoint *self)
{
	g_return_val_if_fail(G_USB_IS_ENDPOINT(self), 0);
	return (self->endpoint_descriptor.bEndpointAddress) & 0xf;
}

/**
 * g_usb_endpoint_get_direction:
 * @self: a #GUsbEndpoint
 *
 * Gets the direction of the endpoint.
 *
 * Return value: The endpoint direction
 *
 * Since: 0.3.3
 **/
GUsbDeviceDirection
g_usb_endpoint_get_direction(GUsbEndpoint *self)
{
	g_return_val_if_fail(G_USB_IS_ENDPOINT(self), 0);
	return (self->endpoint_descriptor.bEndpointAddress & 0x80)
		   ? G_USB_DEVICE_DIRECTION_DEVICE_TO_HOST
		   : G_USB_DEVICE_DIRECTION_HOST_TO_DEVICE;
}

/**
 * g_usb_endpoint_get_extra:
 * @self: a #GUsbEndpoint
 *
 * Gets any extra data from the endpoint.
 *
 * Return value: (transfer none): a #GBytes, or %NULL for failure
 *
 * Since: 0.3.3
 **/
GBytes *
g_usb_endpoint_get_extra(GUsbEndpoint *self)
{
	g_return_val_if_fail(G_USB_IS_ENDPOINT(self), NULL);
	return self->extra;
}
