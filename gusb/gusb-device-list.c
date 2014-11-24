/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Hans de Goede <hdegoede@redhat.com>
 * Copyright (C) 2011-2014 Richard Hughes <richard@hughsie.com>
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
 * SECTION:gusb-device-list
 * @short_description: A device list
 *
 * A device list that is updated as devices are pluged in and unplugged.
 */

#include "config.h"

#include <string.h>
#include <stdlib.h>
#include <libusb-1.0/libusb.h>

#include "gusb-context.h"
#include "gusb-context-private.h"
#include "gusb-device.h"
#include "gusb-device-list.h"
#include "gusb-device-private.h"

#define G_USB_DEVICE_LIST_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), G_USB_TYPE_DEVICE_LIST, GUsbDeviceListPrivate))

enum {
	PROP_0,
	PROP_CONTEXT
};

enum
{
	DEVICE_ADDED_SIGNAL,
	DEVICE_REMOVED_SIGNAL,
	LAST_SIGNAL
};

struct _GUsbDeviceListPrivate {
	GUsbContext			*context;
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GUsbDeviceList, g_usb_device_list, G_TYPE_OBJECT);

static void
g_usb_device_list_finalize (GObject *object)
{
	G_OBJECT_CLASS (g_usb_device_list_parent_class)->finalize (object);
}

/**
 * g_usb_device_list_get_property:
 **/
static void
g_usb_device_list_get_property (GObject		*object,
				guint		 prop_id,
				GValue		*value,
				GParamSpec	*pspec)
{
	GUsbDeviceList *list = G_USB_DEVICE_LIST (object);
	GUsbDeviceListPrivate *priv = list->priv;

	switch (prop_id) {
	case PROP_CONTEXT:
		g_value_set_object (value, priv->context);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * g_usb_device_added_cb:
 **/
static void
g_usb_device_added_cb (GUsbContext *context, GUsbDevice *device, GUsbDeviceList *list)
{
	g_signal_emit (list, signals[DEVICE_ADDED_SIGNAL], 0, device);
}

/**
 * g_usb_device_removed_cb:
 **/
static void
g_usb_device_removed_cb (GUsbContext *context, GUsbDevice *device, GUsbDeviceList *list)
{
	g_signal_emit (list, signals[DEVICE_REMOVED_SIGNAL], 0, device);
}

/**
 * usb_device_list_set_property:
 **/
static void
g_usb_device_list_set_property (GObject		*object,
				guint		 prop_id,
				const GValue	*value,
				GParamSpec	*pspec)
{
	GUsbDeviceList *list = G_USB_DEVICE_LIST (object);
	GUsbDeviceListPrivate *priv = list->priv;

	switch (prop_id) {
	case PROP_CONTEXT:
		priv->context = g_value_get_object (value);
		g_signal_connect (priv->context, "device-added",
				  G_CALLBACK (g_usb_device_added_cb), list);
		g_signal_connect (priv->context, "device-removed",
				  G_CALLBACK (g_usb_device_removed_cb), list);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * g_usb_device_list_class_init:
 **/
static void
g_usb_device_list_class_init (GUsbDeviceListClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = (GObjectClass *) klass;

	object_class->finalize		= g_usb_device_list_finalize;
	object_class->get_property	= g_usb_device_list_get_property;
	object_class->set_property	= g_usb_device_list_set_property;

	/**
	 * GUsbDeviceList:context:
	 */
	pspec = g_param_spec_object ("context", NULL, NULL,
				     G_USB_TYPE_CONTEXT,
				     G_PARAM_CONSTRUCT_ONLY|
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_CONTEXT, pspec);

	/**
	 * GUsbDeviceList::device-added:
	 * @list: the #GUsbDeviceList instance that emitted the signal
	 * @device: A #GUsbDevice
	 *
	 * This signal is emitted when a USB device is added.
	 **/
	signals[DEVICE_ADDED_SIGNAL] = g_signal_new ("device-added",
			G_TYPE_FROM_CLASS (klass),
			G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET (GUsbDeviceListClass, device_added),
			NULL,
			NULL,
			g_cclosure_marshal_VOID__OBJECT,
			G_TYPE_NONE,
			1,
			G_USB_TYPE_DEVICE);

	/**
	 * GUsbDeviceList::device-removed:
	 * @list: the #GUsbDeviceList instance that emitted the signal
	 * @device: A #GUsbDevice
	 *
	 * This signal is emitted when a USB device is removed.
	 **/
	signals[DEVICE_REMOVED_SIGNAL] = g_signal_new ("device-removed",
			G_TYPE_FROM_CLASS (klass),
			G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET (GUsbDeviceListClass, device_removed),
			NULL,
			NULL,
			g_cclosure_marshal_VOID__OBJECT,
			G_TYPE_NONE,
			1,
			G_USB_TYPE_DEVICE);

	g_type_class_add_private (klass, sizeof (GUsbDeviceListPrivate));
}

/**
 * g_usb_device_list_class_init:
 **/
static void
g_usb_device_list_init (GUsbDeviceList *list)
{
	list->priv = G_USB_DEVICE_LIST_GET_PRIVATE (list);
}

/**
 * g_usb_device_list_get_devices:
 * @list: a #GUsbDeviceList
 *
 * Return value: (transfer full): a new #GPtrArray of #GUsbDevice's.
 *
 * Since: 0.1.0
 **/
GPtrArray *
g_usb_device_list_get_devices (GUsbDeviceList *list)
{
	return g_usb_context_get_devices (list->priv->context);
}

/**
 * g_usb_device_list_coldplug:
 * @list: a #GUsbDeviceList
 *
 * This function does nothing.
 *
 * Since: 0.1.0
 **/
void
g_usb_device_list_coldplug (GUsbDeviceList *list)
{
	return;
}

/**
 * g_usb_device_list_find_by_bus_address:
 * @list: a #GUsbDeviceList
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
g_usb_device_list_find_by_bus_address (GUsbDeviceList	*list,
				       guint8		 bus,
				       guint8		 address,
				       GError		**error)
{
	return g_usb_context_find_by_bus_address (list->priv->context, bus, address, error);
}

/**
 * g_usb_device_list_find_by_vid_pid:
 * @list: a #GUsbDeviceList
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
g_usb_device_list_find_by_vid_pid (GUsbDeviceList	*list,
				   guint16		 vid,
				   guint16		 pid,
				   GError		**error)
{
	return g_usb_context_find_by_vid_pid (list->priv->context, vid, pid, error);
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
g_usb_device_list_new (GUsbContext *context)
{
	GObject *obj;
	obj = g_object_new (G_USB_TYPE_DEVICE_LIST, "context", context, NULL);
	return G_USB_DEVICE_LIST (obj);
}
