/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Hans de Goede <hdegoede@redhat.com>
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
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
	GUsbContext			 *context;
	GPtrArray			 *devices;
	libusb_hotplug_callback_handle	 hotplug_id;
	gboolean			 done_coldplug;
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GUsbDeviceList, g_usb_device_list, G_TYPE_OBJECT);

static void
g_usb_device_list_finalize (GObject *object)
{
	GUsbDeviceList *list = G_USB_DEVICE_LIST (object);
	GUsbDeviceListPrivate *priv = list->priv;
	libusb_context *ctx = _g_usb_context_get_context (priv->context);

	libusb_hotplug_deregister_callback (ctx, priv->hotplug_id);
	g_ptr_array_unref (priv->devices);

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

	priv->devices = g_ptr_array_new_with_free_func ((GDestroyNotify)
							g_object_unref);

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
 * g_usb_device_list_hotplug_cb:
 **/
static int
g_usb_device_list_hotplug_cb (struct libusb_context *ctx,
			      struct libusb_device *dev,
			      libusb_hotplug_event event,
			      void *user_data)
{
	GUsbDevice *device = NULL;
	GUsbDeviceList *list = G_USB_DEVICE_LIST (user_data);
	GUsbDeviceListPrivate *priv = list->priv;
	guint8 bus;
	guint8 address;

	/* does any existing device exist */
	bus = libusb_get_bus_number (dev);
	address = libusb_get_device_address (dev);
	if (priv->done_coldplug)
		device = g_usb_device_list_find_by_bus_address (list, bus, address, NULL);
	switch (event) {
	case LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED:
		if (device != NULL) {
			g_debug ("%i:%i already exists", bus, address);
			goto out;
		}
		device = _g_usb_device_new (priv->context, dev, NULL);
		if (g_usb_device_get_device_class (device) == 0x09) {
			g_debug ("%i:%i is a hub, ignoring", bus, address);
			goto out;
		}
		g_ptr_array_add (priv->devices, g_object_ref (device));
		g_signal_emit (list, signals[DEVICE_ADDED_SIGNAL], 0, device);
		break;
	case LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT:
		if (device == NULL) {
			g_debug ("%i:%i does not exist", bus, address);
			goto out;
		}
		g_signal_emit (list, signals[DEVICE_REMOVED_SIGNAL], 0, device);
		g_ptr_array_remove (priv->devices, device);
		break;
	default:
		break;
	}
out:
	if (device != NULL)
		g_object_unref (device);
	return 0;
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
	g_return_val_if_fail (G_USB_IS_DEVICE_LIST (list), NULL);
	return g_ptr_array_ref (list->priv->devices);
}

/**
 * g_usb_device_list_coldplug:
 * @list: a #GUsbDeviceList
 *
 * Finds all the USB devices and adds them to the list.
 *
 * You only need to call this function once, and any subsequent calls
 * are silently ignored.
 *
 * Since: 0.1.0
 **/
void
g_usb_device_list_coldplug (GUsbDeviceList *list)
{
	GUsbDeviceListPrivate *priv = list->priv;
	gint rc;
	libusb_context *ctx = _g_usb_context_get_context (priv->context);

	if (priv->done_coldplug)
		return;

	/* enumerate and watch for add/remove */
	rc = libusb_hotplug_register_callback (ctx,
					       LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED |
					       LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
					       LIBUSB_HOTPLUG_ENUMERATE,
					       LIBUSB_HOTPLUG_MATCH_ANY,
					       LIBUSB_HOTPLUG_MATCH_ANY,
					       LIBUSB_HOTPLUG_MATCH_ANY,
					       g_usb_device_list_hotplug_cb,
					       list,
					       &list->priv->hotplug_id);
	if (rc != LIBUSB_SUCCESS)
		g_error ("Error creating a hotplug callback");
	priv->done_coldplug = TRUE;
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
	GUsbDeviceListPrivate *priv = list->priv;
	GUsbDevice *device = NULL;
	guint i;

	/* ensure the list of coldplugged */
	g_usb_device_list_coldplug (list);

	for (i = 0; i < priv->devices->len; i++) {
		GUsbDevice *curr = g_ptr_array_index (priv->devices, i);
		if (g_usb_device_get_bus (curr) == bus &&
		    g_usb_device_get_address (curr) == address) {
			device = g_object_ref (curr);
			goto out;
		}
	}
	g_set_error (error,
		     G_USB_DEVICE_ERROR,
		     G_USB_DEVICE_ERROR_NO_DEVICE,
		     "Failed to find device %04x:%04x",
		     bus, address);
out:
	return device;
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
	GUsbDeviceListPrivate *priv = list->priv;
	GUsbDevice *device = NULL;
	guint i;

	/* ensure the list of coldplugged */
	g_usb_device_list_coldplug (list);

	for (i = 0; i < priv->devices->len; i++) {
		GUsbDevice *curr = g_ptr_array_index (priv->devices, i);

		if (g_usb_device_get_vid (curr) == vid &&
		    g_usb_device_get_pid (curr) == pid) {
			device = g_object_ref (curr);
			goto out;
		}
	}
	g_set_error (error,
		     G_USB_DEVICE_ERROR,
		     G_USB_DEVICE_ERROR_NO_DEVICE,
		     "Failed to find device %04x:%04x",
		     vid, pid);
out:
	return device;
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
