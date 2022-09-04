/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2022 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
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
