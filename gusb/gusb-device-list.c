/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Hans de Goede <hdegoede@redhat.com>
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

#include <string.h>
#include <stdlib.h>
#include <gudev/gudev.h>
#include <libusb-1.0/libusb.h>

#include "gusb-marshal.h"
#include "gusb-context.h"
#include "gusb-context-private.h"
#include "gusb-device.h"
#include "gusb-device-private.h"

#include "gusb-device-list.h"

#define G_USB_DEVICE_LIST_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), G_USB_TYPE_DEVICE_LIST, GUsbDeviceListPrivate))

enum {
	PROP_0,
	PROP_CONTEXT,
};

enum
{
	DEVICE_ADDED_SIGNAL,
	DEVICE_REMOVED_SIGNAL,
	LAST_SIGNAL,
};

struct _GUsbDeviceListPrivate {
	GUsbContext	 *context;
	GUdevClient	 *udev;
	GPtrArray	 *devices;
	libusb_device	**coldplug_list;
};

static void g_usb_device_list_uevent_cb (GUdevClient	*client,
					const gchar	*action,
					GUdevDevice	*udevice,
					gpointer	 user_data);

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (GUsbDeviceList, g_usb_device_list, G_TYPE_OBJECT);

static void
g_usb_device_list_finalize (GObject *object)
{
	GUsbDeviceList *list = G_USB_DEVICE_LIST (object);
	GUsbDeviceListPrivate *priv = list->priv;

	g_object_unref (priv->udev);
	g_ptr_array_unref (priv->devices);
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
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static GObject *
g_usb_device_list_constructor (GType			 gtype,
			       guint			 n_properties,
			       GObjectConstructParam	*properties)
{
	GObject *obj;
	GUsbDeviceList *list;
	GUsbDeviceListPrivate *priv;
	const gchar *const subsystems[] = {"usb", NULL};

	{
		/* Always chain up to the parent constructor */
		GObjectClass *parent_class;
		parent_class = G_OBJECT_CLASS (g_usb_device_list_parent_class);
		obj = parent_class->constructor (gtype, n_properties,
						 properties);
	}

	list = G_USB_DEVICE_LIST (obj);
	priv = list->priv;

	if (!priv->context)
		g_error("constructed without a context");

	priv->udev = g_udev_client_new (subsystems);
	g_signal_connect (G_OBJECT (priv->udev), "uevent",
			  G_CALLBACK (g_usb_device_list_uevent_cb), list);

	priv->devices = g_ptr_array_new_with_free_func ((GDestroyNotify)
							g_object_unref);

	priv->coldplug_list = NULL;

	return obj;
}

/**
 * g_usb_device_list_class_init:
 **/
static void
g_usb_device_list_class_init (GUsbDeviceListClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = (GObjectClass *) klass;

	object_class->constructor	= g_usb_device_list_constructor;
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

	signals[DEVICE_ADDED_SIGNAL] = g_signal_new ("device_added",
			G_TYPE_FROM_CLASS (klass),
			G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET (GUsbDeviceListClass, device_added),
			NULL,
			NULL,
			g_cclosure_user_marshal_VOID__OBJECT_OBJECT,
			G_TYPE_NONE,
			2,
			G_TYPE_OBJECT,
			G_TYPE_OBJECT);

	signals[DEVICE_REMOVED_SIGNAL] = g_signal_new ("device_removed",
			G_TYPE_FROM_CLASS (klass),
			G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET (GUsbDeviceListClass, device_removed),
			NULL,
			NULL,
			g_cclosure_user_marshal_VOID__OBJECT_OBJECT,
			G_TYPE_NONE,
			2,
			G_TYPE_OBJECT,
			G_TYPE_OBJECT);

	g_type_class_add_private (klass, sizeof (GUsbDeviceListPrivate));
}

static void
g_usb_device_list_init (GUsbDeviceList *list)
{
	list->priv = G_USB_DEVICE_LIST_GET_PRIVATE (list);
}

static gboolean
g_usb_device_list_get_bus_n_address (GUdevDevice	*udev,
				     gint		*bus,
				     gint		*address)
{
	const gchar *bus_str, *address_str;

	*bus = *address = 0;

	bus_str = g_udev_device_get_property (udev, "BUSNUM");
	address_str = g_udev_device_get_property (udev, "DEVNUM");
	if (bus_str)
		*bus = atoi(bus_str);
	if (address_str)
		*address = atoi(address_str);

	return *bus && *address;
}

static void
g_usb_device_list_add_dev (GUsbDeviceList *list, GUdevDevice *udev)
{
	GUsbDeviceListPrivate *priv = list->priv;
	GUsbDevice *device = NULL;
	libusb_device **dev_list = NULL;
	const gchar *devtype, *devclass;
	gint i, bus, address;
	libusb_context *ctx = _g_usb_context_get_context (priv->context);

	devtype = g_udev_device_get_property (udev, "DEVTYPE");
	/* Check if this is a usb device (and not an interface) */
	if (!devtype || strcmp(devtype, "usb_device"))
		return;

	/* Skip hubs */
	devclass = g_udev_device_get_sysfs_attr(udev, "bDeviceClass");
	if (!devclass || !strcmp(devclass, "09"))
		return;

	if (!g_usb_device_list_get_bus_n_address (udev, &bus, &address)) {
		g_warning ("usb-device without bus number or device address");
		return;
	}

	if (priv->coldplug_list)
		dev_list = priv->coldplug_list;
	else
		libusb_get_device_list(ctx, &dev_list);

	for (i = 0; dev_list && dev_list[i]; i++) {
		if (libusb_get_bus_number (dev_list[i]) == bus &&
		    libusb_get_device_address (dev_list[i]) == address) {
			device = _g_usb_device_new (dev_list[i]);
			break;
		}
	}

	if (!priv->coldplug_list)
		libusb_free_device_list (dev_list, 1);

	if (!device) {
		g_warning ("Could not find usb dev at busnum %d devaddr %d",
			   bus, address);
		return;
	}

	g_ptr_array_add (priv->devices, device);
	g_signal_emit (list, signals[DEVICE_ADDED_SIGNAL], 0, device, udev);
}

static void
g_usb_device_list_remove_dev (GUsbDeviceList *list, GUdevDevice *udev)
{
	GUsbDeviceListPrivate *priv = list->priv;
	GUsbDevice *device;
	gint bus, address;

	if (!g_usb_device_list_get_bus_n_address (udev, &bus, &address))
		return;

	device = g_usb_device_list_get_dev_by_bus_n_address (list, bus,
							     address);
	if (!device)
		return;

	g_signal_emit (list, signals[DEVICE_REMOVED_SIGNAL], 0, device, udev);
	g_ptr_array_remove (priv->devices, device);
}

static void
g_usb_device_list_uevent_cb (GUdevClient		*client,
			     const gchar		*action,
			     GUdevDevice		*udevice,
			     gpointer			 user_data)
{
	GUsbDeviceList *list = G_USB_DEVICE_LIST (user_data);

	if (g_str_equal (action, "add"))
		g_usb_device_list_add_dev (list, udevice);
	else if (g_str_equal (action, "remove"))
		g_usb_device_list_remove_dev (list, udevice);
}

void
g_usb_device_list_coldplug (GUsbDeviceList *list)
{
	GUsbDeviceListPrivate *priv = list->priv;
	GList *devices, *elem;
	libusb_context *ctx = _g_usb_context_get_context (priv->context);

	libusb_get_device_list(ctx, &priv->coldplug_list);
	devices = g_udev_client_query_by_subsystem (priv->udev, "usb");
	for (elem = g_list_first (devices); elem; elem = g_list_next (elem)) {
		g_usb_device_list_add_dev (list, elem->data);
		g_object_unref (elem->data);
	}
	g_list_free (devices);
	libusb_free_device_list (priv->coldplug_list, 1);
	priv->coldplug_list = NULL;
}

GUsbDevice *
g_usb_device_list_get_dev_by_bus_n_address (GUsbDeviceList	*list,
					    guint8		 bus,
					    guint8		 address)
{
	GUsbDeviceListPrivate *priv = list->priv;
	GUsbDevice *device = NULL;
	guint i;

	for (i = 0; i < priv->devices->len; i++) {
		GUsbDevice *curr = g_ptr_array_index (priv->devices, i);
		if (g_usb_device_get_bus (curr) == bus &&
		    g_usb_device_get_address (curr) == address) {
			device = curr;
			break;
                }
	}

	return device;
}

GUsbDeviceList *
g_usb_device_list_new (GUsbContext *context)
{
	GObject *obj;
	obj = g_object_new (G_USB_TYPE_DEVICE_LIST, "context", context, NULL);
	return G_USB_DEVICE_LIST (obj);
}
