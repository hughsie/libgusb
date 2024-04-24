/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2022 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

/**
 * SECTION:gusb-bos-descriptor
 * @short_description: GLib wrapper around a USB BOS descriptor.
 *
 * This object is a thin glib wrapper around a `libusb_bos_dev_capability_descriptor`.
 *
 * All the data is copied when the object is created and the original descriptor can be destroyed
 * at any point.
 */

#include "config.h"

#include <string.h>

#include "gusb-bos-descriptor-private.h"
#include "gusb-json-common.h"

struct _GUsbBosDescriptor {
	GObject parent_instance;

	struct libusb_bos_dev_capability_descriptor bos_cap;
	GBytes *extra;
};

G_DEFINE_TYPE(GUsbBosDescriptor, g_usb_bos_descriptor, G_TYPE_OBJECT)

static void
g_usb_bos_descriptor_finalize(GObject *object)
{
	GUsbBosDescriptor *self = G_USB_BOS_DESCRIPTOR(object);

	g_bytes_unref(self->extra);

	G_OBJECT_CLASS(g_usb_bos_descriptor_parent_class)->finalize(object);
}

static void
g_usb_bos_descriptor_class_init(GUsbBosDescriptorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->finalize = g_usb_bos_descriptor_finalize;
}

static void
g_usb_bos_descriptor_init(GUsbBosDescriptor *self)
{
}

gboolean
_g_usb_bos_descriptor_load(GUsbBosDescriptor *self, JsonObject *json_object, GError **error)
{
	const gchar *str;

	g_return_val_if_fail(G_USB_IS_BOS_DESCRIPTOR(self), FALSE);
	g_return_val_if_fail(json_object != NULL, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, -1);

	/* optional properties */
	self->bos_cap.bDevCapabilityType =
	    json_object_get_int_member_with_default(json_object, "DevCapabilityType", 0x0);

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
_g_usb_bos_descriptor_save(GUsbBosDescriptor *self, JsonBuilder *json_builder, GError **error)
{
	g_return_val_if_fail(G_USB_IS_BOS_DESCRIPTOR(self), FALSE);
	g_return_val_if_fail(json_builder != NULL, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	/* start */
	json_builder_begin_object(json_builder);

	/* optional properties */
	if (self->bos_cap.bDevCapabilityType != 0) {
		json_builder_set_member_name(json_builder, "DevCapabilityType");
		json_builder_add_int_value(json_builder, self->bos_cap.bDevCapabilityType);
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
 * _g_usb_bos_descriptor_new:
 *
 * Return value: a new #GUsbBosDescriptor object.
 *
 * Since: 0.4.0
 **/
GUsbBosDescriptor *
_g_usb_bos_descriptor_new(const struct libusb_bos_dev_capability_descriptor *bos_cap)
{
	GUsbBosDescriptor *self;
	self = g_object_new(G_USB_TYPE_BOS_DESCRIPTOR, NULL);

	/* copy the data */
	memcpy(&self->bos_cap, bos_cap, sizeof(*bos_cap));
	self->extra = g_bytes_new(bos_cap->dev_capability_data, bos_cap->bLength - 0x03);

	return G_USB_BOS_DESCRIPTOR(self);
}

/**
 * g_usb_bos_descriptor_get_capability:
 * @self: a #GUsbBosDescriptor
 *
 * Gets the BOS descriptor capability.
 *
 * Return value: capability
 *
 * Since: 0.4.0
 **/
guint8
g_usb_bos_descriptor_get_capability(GUsbBosDescriptor *self)
{
	g_return_val_if_fail(G_USB_IS_BOS_DESCRIPTOR(self), 0);
	return self->bos_cap.bDevCapabilityType;
}

/**
 * g_usb_bos_descriptor_get_extra:
 * @self: a #GUsbBosDescriptor
 *
 * Gets any extra data from the BOS descriptor.
 *
 * Return value: (transfer none): a #GBytes, or %NULL for failure
 *
 * Since: 0.4.0
 **/
GBytes *
g_usb_bos_descriptor_get_extra(GUsbBosDescriptor *self)
{
	g_return_val_if_fail(G_USB_IS_BOS_DESCRIPTOR(self), NULL);
	return self->extra;
}
