/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Hans de Goede <hdegoede@redhat.com>
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

/**
 * SECTION:gusb-device-list
 * @short_description: A device list
 *
 * A device list that is updated as devices are pluged in and unplugged.
 */

#include "config.h"

#include <libusb.h>
#include <stdlib.h>
#include <string.h>

#include "gusb-context-private.h"
#include "gusb-device-list.h"
#include "gusb-device-private.h"

enum { PROP_0, PROP_CONTEXT };

enum { DEVICE_ADDED_SIGNAL, DEVICE_REMOVED_SIGNAL, LAST_SIGNAL };

typedef struct {
	GUsbContext *context;
} GUsbDeviceListPrivate;

static guint signals[LAST_SIGNAL] = {0};

G_DEFINE_TYPE_WITH_PRIVATE(GUsbDeviceList, g_usb_device_list, G_TYPE_OBJECT);

#define GET_PRIVATE(o) (g_usb_device_list_get_instance_private(o))

static void
g_usb_device_list_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GUsbDeviceList *self = G_USB_DEVICE_LIST(object);
	GUsbDeviceListPrivate *priv = GET_PRIVATE(self);

	switch (prop_id) {
	case PROP_CONTEXT:
		g_value_set_object(value, priv->context);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
g_usb_device_added_cb(GUsbContext *context, GUsbDevice *device, GUsbDeviceList *self)
{
	g_signal_emit(self, signals[DEVICE_ADDED_SIGNAL], 0, device);
}

static void
g_usb_device_removed_cb(GUsbContext *context, GUsbDevice *device, GUsbDeviceList *self)
{
	g_signal_emit(self, signals[DEVICE_REMOVED_SIGNAL], 0, device);
}

static void
g_usb_device_list_set_property(GObject *object,
			       guint prop_id,
			       const GValue *value,
			       GParamSpec *pspec)
{
	GUsbDeviceList *self = G_USB_DEVICE_LIST(object);
	GUsbDeviceListPrivate *priv = GET_PRIVATE(self);

	switch (prop_id) {
	case PROP_CONTEXT:
		priv->context = g_value_get_object(value);
		g_signal_connect(priv->context,
				 "device-added",
				 G_CALLBACK(g_usb_device_added_cb),
				 self);
		g_signal_connect(priv->context,
				 "device-removed",
				 G_CALLBACK(g_usb_device_removed_cb),
				 self);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
g_usb_device_list_class_init(GUsbDeviceListClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->get_property = g_usb_device_list_get_property;
	object_class->set_property = g_usb_device_list_set_property;

	/**
	 * GUsbDeviceList:context:
	 */
	pspec = g_param_spec_object("context",
				    NULL,
				    NULL,
				    G_USB_TYPE_CONTEXT,
				    G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
	g_object_class_install_property(object_class, PROP_CONTEXT, pspec);

	/**
	 * GUsbDeviceList::device-added:
	 * @self: the #GUsbDeviceList instance that emitted the signal
	 * @device: A #GUsbDevice
	 *
	 * This signal is emitted when a USB device is added.
	 **/
	signals[DEVICE_ADDED_SIGNAL] =
	    g_signal_new("device-added",
			 G_TYPE_FROM_CLASS(klass),
			 G_SIGNAL_RUN_LAST,
			 G_STRUCT_OFFSET(GUsbDeviceListClass, device_added),
			 NULL,
			 NULL,
			 g_cclosure_marshal_VOID__OBJECT,
			 G_TYPE_NONE,
			 1,
			 G_USB_TYPE_DEVICE);

	/**
	 * GUsbDeviceList::device-removed:
	 * @self: the #GUsbDeviceList instance that emitted the signal
	 * @device: A #GUsbDevice
	 *
	 * This signal is emitted when a USB device is removed.
	 **/
	signals[DEVICE_REMOVED_SIGNAL] =
	    g_signal_new("device-removed",
			 G_TYPE_FROM_CLASS(klass),
			 G_SIGNAL_RUN_LAST,
			 G_STRUCT_OFFSET(GUsbDeviceListClass, device_removed),
			 NULL,
			 NULL,
			 g_cclosure_marshal_VOID__OBJECT,
			 G_TYPE_NONE,
			 1,
			 G_USB_TYPE_DEVICE);
}

static void
g_usb_device_list_init(GUsbDeviceList *self)
{
}

/**
 * g_usb_device_list_get_devices:
 * @self: a #GUsbDeviceList
 *
 * Return value: (transfer full) (element-type GUsbDevice): a new #GPtrArray of #GUsbDevice's.
 *
 * Since: 0.1.0
 **/
GPtrArray *
g_usb_device_list_get_devices(GUsbDeviceList *self)
{
	GUsbDeviceListPrivate *priv = GET_PRIVATE(self);
	g_return_val_if_fail(G_USB_IS_DEVICE_LIST(self), NULL);
	return g_usb_context_get_devices(priv->context);
}

/**
 * g_usb_device_list_coldplug:
 * @self: a #GUsbDeviceList
 *
 * This function does nothing.
 *
 * Since: 0.1.0
 **/
void
g_usb_device_list_coldplug(GUsbDeviceList *self)
{
	return;
}

/**
 * g_usb_device_list_find_by_bus_address:
 * @self: a #GUsbDeviceList
 * @bus: a bus number
 * @address: a bus address
 * @error: A #GError or %NULL
 *
 * Finds a device based on its bus and address values.
 *
 * Return value: (transfer full): a new #GUsbDevice, or %NULL if not found.
 *
 * Since: 0.1.0
 **/
GUsbDevice *
g_usb_device_list_find_by_bus_address(GUsbDeviceList *self,
				      guint8 bus,
				      guint8 address,
				      GError **error)
{
	GUsbDeviceListPrivate *priv = GET_PRIVATE(self);
	g_return_val_if_fail(G_USB_IS_DEVICE_LIST(self), NULL);
	g_return_val_if_fail(error == NULL || *error == NULL, NULL);
	return g_usb_context_find_by_bus_address(priv->context, bus, address, error);
}

/**
 * g_usb_device_list_find_by_vid_pid:
 * @self: a #GUsbDeviceList
 * @vid: a vendor ID
 * @pid: a product ID
 * @error: A #GError or %NULL
 *
 * Finds a device based on its bus and address values.
 *
 * Return value: (transfer full): a new #GUsbDevice, or %NULL if not found.
 *
 * Since: 0.1.0
 **/
GUsbDevice *
g_usb_device_list_find_by_vid_pid(GUsbDeviceList *self, guint16 vid, guint16 pid, GError **error)
{
	GUsbDeviceListPrivate *priv = GET_PRIVATE(self);
	g_return_val_if_fail(G_USB_IS_DEVICE_LIST(self), NULL);
	g_return_val_if_fail(error == NULL || *error == NULL, NULL);
	return g_usb_context_find_by_vid_pid(priv->context, vid, pid, error);
}

/**
 * g_usb_device_list_new:
 * @context: a #GUsbContext
 *
 * Creates a new device list.
 *
 * You will need to call g_usb_device_list_coldplug() to coldplug the
 * list of devices after creating a device list.
 *
 * Return value: a new #GUsbDeviceList
 *
 * Since: 0.1.0
 **/
GUsbDeviceList *
g_usb_device_list_new(GUsbContext *context)
{
	return g_object_new(G_USB_TYPE_DEVICE_LIST, "context", context, NULL);
}
