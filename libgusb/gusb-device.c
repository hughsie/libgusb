/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2011 Richard Hughes <richard@hughsie.com>
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

/**
 * SECTION:usb-device
 * @short_description: GLib device integration for libusb
 *
 * This object is a thin glib wrapper around a libusb_device
 */

#include "config.h"

#include <libusb-1.0/libusb.h>

#include "gusb-device.h"
#include "gusb-device-private.h"

static void     g_usb_device_finalize	(GObject     *object);

#define G_USB_DEVICE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), G_USB_TYPE_DEVICE, GUsbDevicePrivate))

/**
 * GUsbDevicePrivate:
 *
 * Private #GUsbDevice data
 **/
struct _GUsbDevicePrivate
{
	libusb_device		*device;
};

enum {
	PROP_0,
	PROP_LIBUSB_DEVICE,
};

G_DEFINE_TYPE (GUsbDevice, g_usb_device, G_TYPE_OBJECT)


/**
 * g_usb_device_error_quark:
 *
 * Return value: Our personal error quark.
 *
 * Since: 0.0.1
 **/
GQuark
g_usb_device_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("g_usb_device_error");
	return quark;
}

/**
 * usb_device_get_property:
 **/
static void
g_usb_device_get_property (GObject		*object,
			   guint		 prop_id,
			   GValue		*value,
			   GParamSpec		*pspec)
{
	GUsbDevice *device = G_USB_DEVICE (object);
	GUsbDevicePrivate *priv = device->priv;

	switch (prop_id) {
	case PROP_LIBUSB_DEVICE:
		g_value_set_pointer (value, priv->device);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * usb_device_set_property:
 **/
static void
g_usb_device_set_property (GObject		*object,
			   guint		 prop_id,
			   const GValue		*value,
			   GParamSpec		*pspec)
{
	GUsbDevice *device = G_USB_DEVICE (object);
	GUsbDevicePrivate *priv = device->priv;

	switch (prop_id) {
	case PROP_LIBUSB_DEVICE:
		priv->device = g_value_get_pointer (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static GObject *
g_usb_device_constructor (GType			 gtype,
			  guint			 n_properties,
			  GObjectConstructParam	*properties)
{
	GObject *obj;
	GUsbDevice *device;
	GUsbDevicePrivate *priv;

	{
		/* Always chain up to the parent constructor */
		GObjectClass *parent_class;
		parent_class = G_OBJECT_CLASS (g_usb_device_parent_class);
		obj = parent_class->constructor (gtype, n_properties,
						 properties);
	}

	device = G_USB_DEVICE (obj);
	priv = device->priv;

	if (!priv->device)
		g_error("constructed without a libusb_device");

	libusb_ref_device(priv->device);

	return obj;
}

/**
 * usb_device_class_init:
 **/
static void
g_usb_device_class_init (GUsbDeviceClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->constructor	= g_usb_device_constructor;
	object_class->finalize		= g_usb_device_finalize;
	object_class->get_property	= g_usb_device_get_property;
	object_class->set_property	= g_usb_device_set_property;

	/**
	 * GUsbDevice:libusb_device:
	 */
	pspec = g_param_spec_pointer ("libusb_device", NULL, NULL,
				     G_PARAM_CONSTRUCT_ONLY|
				     G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_LIBUSB_DEVICE,
					 pspec);

	g_type_class_add_private (klass, sizeof (GUsbDevicePrivate));
}

/**
 * g_usb_device_init:
 **/
static void
g_usb_device_init (GUsbDevice *device)
{
	device->priv = G_USB_DEVICE_GET_PRIVATE (device);
}

/**
 * g_usb_device_finalize:
 **/
static void
g_usb_device_finalize (GObject *object)
{
	GUsbDevice *device = G_USB_DEVICE (object);
	GUsbDevicePrivate *priv = device->priv;

	libusb_unref_device(priv->device);

	G_OBJECT_CLASS (g_usb_device_parent_class)->finalize (object);
}

/**
 * _g_usb_device_new:
 *
 * Return value: a new #GUsbDevice object.
 **/
GUsbDevice *
_g_usb_device_new (libusb_device	*device)
{
	GObject *obj;
	obj = g_object_new (G_USB_TYPE_DEVICE, "libusb_device", device, NULL);
	return G_USB_DEVICE (obj);
}

/**
 * _g_usb_device_get_device:
 * @device: a #GUsbDevice instance
 *
 * Gets the low-level libusb_device
 *
 * Return value: The #libusb_device or %NULL. Do not unref this value.
 **/
libusb_device *
_g_usb_device_get_device (GUsbDevice	*device)
{
	return device->priv->device;
}

guint8
g_usb_device_get_bus (GUsbDevice	*device)
{
	return libusb_get_bus_number (device->priv->device);
}

guint8
g_usb_device_get_address (GUsbDevice	*device)
{
	return libusb_get_device_address (device->priv->device);
}
