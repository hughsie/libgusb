/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2022 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

/**
 * SECTION:gusb-device-event
 * @short_description: An event that happened to a GUsbDevice.
 */

#include "config.h"

#include "gusb-device-event-private.h"
#include "gusb-json-common.h"

struct _GUsbDeviceEvent {
	GObject parent_instance;
	gchar *id;
	gint status;
	gint rc;
	GBytes *bytes;
};

G_DEFINE_TYPE(GUsbDeviceEvent, g_usb_device_event, G_TYPE_OBJECT)

static void
g_usb_device_event_finalize(GObject *object)
{
	GUsbDeviceEvent *self = G_USB_DEVICE_EVENT(object);

	g_free(self->id);
	if (self->bytes != NULL)
		g_bytes_unref(self->bytes);

	G_OBJECT_CLASS(g_usb_device_event_parent_class)->finalize(object);
}

static void
g_usb_device_event_class_init(GUsbDeviceEventClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	object_class->finalize = g_usb_device_event_finalize;
}

static void
g_usb_device_event_init(GUsbDeviceEvent *self)
{
}

gboolean
_g_usb_device_event_load(GUsbDeviceEvent *self, JsonObject *json_object, GError **error)
{
	const gchar *str;

	g_return_val_if_fail(G_USB_IS_DEVICE_EVENT(self), FALSE);
	g_return_val_if_fail(json_object != NULL, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, -1);

	/* optional properties */
	self->id = g_strdup(json_object_get_string_member_with_default(json_object, "Id", NULL));
	self->status = json_object_get_int_member_with_default(json_object,
							       "Status",
							       LIBUSB_TRANSFER_COMPLETED);
	self->rc = json_object_get_int_member_with_default(json_object, "Error", LIBUSB_SUCCESS);

	/* extra data */
	str = json_object_get_string_member_with_default(json_object, "Data", NULL);
	if (str != NULL) {
		gsize bufsz = 0;
		g_autofree guchar *buf = g_base64_decode(str, &bufsz);
		self->bytes = g_bytes_new_take(g_steal_pointer(&buf), bufsz);
	}

	/* success */
	return TRUE;
}

gboolean
_g_usb_device_event_save(GUsbDeviceEvent *self, JsonBuilder *json_builder, GError **error)
{
	g_return_val_if_fail(G_USB_IS_DEVICE_EVENT(self), FALSE);
	g_return_val_if_fail(json_builder != NULL, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	/* start */
	json_builder_begin_object(json_builder);

	if (self->id != NULL) {
		json_builder_set_member_name(json_builder, "Id");
		json_builder_add_string_value(json_builder, self->id);
	}
	if (self->status != LIBUSB_TRANSFER_COMPLETED) {
		json_builder_set_member_name(json_builder, "Status");
		json_builder_add_int_value(json_builder, self->status);
	}
	if (self->rc != LIBUSB_SUCCESS) {
		json_builder_set_member_name(json_builder, "Error");
		json_builder_add_int_value(json_builder, self->rc);
	}
	if (self->bytes != NULL) {
		g_autofree gchar *str = g_base64_encode(g_bytes_get_data(self->bytes, NULL),
							g_bytes_get_size(self->bytes));
		json_builder_set_member_name(json_builder, "Data");
		json_builder_add_string_value(json_builder, str);
	}

	/* success */
	json_builder_end_object(json_builder);
	return TRUE;
}

/**
 * _g_usb_device_event_new:
 * @id: a cache key
 *
 * Return value: a new #GUsbDeviceEvent object.
 *
 * Since: 0.4.0
 **/
GUsbDeviceEvent *
_g_usb_device_event_new(const gchar *id)
{
	GUsbDeviceEvent *self;
	self = g_object_new(G_USB_TYPE_DEVICE_EVENT, NULL);
	self->id = g_strdup(id);
	return G_USB_DEVICE_EVENT(self);
}

/**
 * g_usb_device_event_get_id:
 * @self: a #GUsbDeviceEvent
 *
 * Gets the event ID.
 *
 * Return value: string, or %NULL
 *
 * Since: 0.4.0
 **/
const gchar *
g_usb_device_event_get_id(GUsbDeviceEvent *self)
{
	g_return_val_if_fail(G_USB_IS_DEVICE_EVENT(self), NULL);
	return self->id;
}

/**
 * g_usb_device_event_get_status:
 * @self: a #GUsbDeviceEvent
 *
 * Gets any status data from the event.
 *
 * Return value: a `enum libusb_transfer_status`, or -1 for failure
 *
 * Since: 0.4.0
 **/
gint
g_usb_device_event_get_status(GUsbDeviceEvent *self)
{
	g_return_val_if_fail(G_USB_IS_DEVICE_EVENT(self), -1);
	return self->status;
}

/**
 * _g_usb_device_event_set_status:
 * @self: a #GUsbDeviceEvent
 * @status: `enum libusb_transfer_status`
 *
 * Set the status of the event, e.g. `LIBUSB_TRANSFER_COMPLETED`.
 *
 * Since: 0.4.0
 **/
void
_g_usb_device_event_set_status(GUsbDeviceEvent *self, gint status)
{
	g_return_if_fail(G_USB_IS_DEVICE_EVENT(self));
	self->status = status;
}

/**
 * g_usb_device_event_get_rc:
 * @self: a #GUsbDeviceEvent
 *
 * Gets any return code from the event.
 *
 * Return value: a `enum libusb_error`
 *
 * Since: 0.4.5
 **/
gint
g_usb_device_event_get_rc(GUsbDeviceEvent *self)
{
	g_return_val_if_fail(G_USB_IS_DEVICE_EVENT(self), LIBUSB_ERROR_OTHER);
	return self->rc;
}

/**
 * _g_usb_device_event_set_rc:
 * @self: a #GUsbDeviceEvent
 * @status: `enum libusb_error`
 *
 * Set the return code of the event, e.g. `LIBUSB_ERROR_TIMEOUT`.
 *
 * Since: 0.4.5
 **/
void
_g_usb_device_event_set_rc(GUsbDeviceEvent *self, gint rc)
{
	g_return_if_fail(G_USB_IS_DEVICE_EVENT(self));
	g_return_if_fail(rc <= 0);
	self->rc = rc;
}

/**
 * g_usb_device_event_get_bytes:
 * @self: a #GUsbDeviceEvent
 *
 * Gets any bytes data from the event.
 *
 * Return value: (transfer none): a #GBytes, or %NULL
 *
 * Since: 0.4.0
 **/
GBytes *
g_usb_device_event_get_bytes(GUsbDeviceEvent *self)
{
	g_return_val_if_fail(G_USB_IS_DEVICE_EVENT(self), NULL);
	return self->bytes;
}

/**
 * g_usb_device_event_set_bytes:
 * @self: a #GUsbDeviceEvent
 * @bytes: a #GBytes
 *
 * Set the bytes data to the event.
 *
 * Since: 0.4.0
 **/
void
g_usb_device_event_set_bytes(GUsbDeviceEvent *self, GBytes *bytes)
{
	g_return_if_fail(G_USB_IS_DEVICE_EVENT(self));
	g_return_if_fail(bytes != NULL);
	if (self->bytes != NULL)
		g_bytes_unref(self->bytes);
	self->bytes = g_bytes_ref(bytes);
}

void
_g_usb_device_event_set_bytes_raw(GUsbDeviceEvent *self, gconstpointer buf, gsize bufsz)
{
	g_return_if_fail(G_USB_IS_DEVICE_EVENT(self));
	g_return_if_fail(buf != NULL);
	if (self->bytes != NULL)
		g_bytes_unref(self->bytes);
	self->bytes = g_bytes_new(buf, bufsz);
}
