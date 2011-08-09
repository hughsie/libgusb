/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2011 Richard Hughes <richard@hughsie.com>
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

/**
 * SECTION:usb-device
 * @short_description: GLib device integration for libusb
 *
 * This object can be used to integrate libusb into the GLib event loop.
 */

#include "config.h"

#include <glib-object.h>
#include <sys/poll.h>
#include <libusb-1.0/libusb.h>

#include "gusb-device.h"

static void     g_usb_device_finalize	(GObject     *object);

#define G_USB_DEVICE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), G_USB_TYPE_DEVICE, GUsbDevicePrivate))

/**
 * GUsbDevicePrivate:
 *
 * Private #GUsbDevice data
 **/
struct _GUsbDevicePrivate
{
	gboolean			 connected;
	libusb_device_handle		*handle;
	libusb_context			*ctx;
};

enum {
	PROP_0,
	PROP_CONNECTED,
	PROP_LAST
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
 * usb_device_get_handle:
 * @usb:  a #GUsbDevice instance
 *
 * Gets the low-level device handle
 *
 * Return value: The #libusb_device_handle or %NULL. Do not unref this value.
 **/
libusb_device_handle *
g_usb_device_get_handle (GUsbDevice *usb)
{
	return usb->priv->handle;
}

/**
 * usb_device_connect:
 * @usb:  a #GUsbDevice instance
 * @vendor_id: the vendor ID to connect to
 * @product_id: the product ID to connect to
 * @configuration: the configuration index to use, usually '1'
 * @interface: the configuration interface to use, usually '0'
 * @error:  a #GError, or %NULL
 *
 * Connects to a specific device.
 *
 * Return value: %TRUE for success
 **/
gboolean
g_usb_device_connect (GUsbDevice *usb,
		      guint vendor_id,
		      guint product_id,
		      guint configuration,
		      guint interface,
		      GError **error)
{
	gint rc;
	gboolean ret = FALSE;
	GUsbDevicePrivate *priv = usb->priv;

	/* already connected */
	if (priv->handle != NULL) {
		g_set_error_literal (error, G_USB_DEVICE_ERROR,
				     G_USB_DEVICE_ERROR_INTERNAL,
				     "already connected to a device");
		goto out;
	}

	/* open device */
	priv->handle = libusb_open_device_with_vid_pid (priv->ctx,
							vendor_id,
							product_id);
	if (priv->handle == NULL) {
		g_set_error (error, G_USB_DEVICE_ERROR,
			     G_USB_DEVICE_ERROR_INTERNAL,
			     "failed to find device %04x:%04x",
			     vendor_id, product_id);
		ret = FALSE;
		goto out;
	}

	/* set configuration and interface */
	rc = libusb_set_configuration (priv->handle, configuration);
	if (rc < 0) {
		g_set_error (error, G_USB_DEVICE_ERROR,
			     G_USB_DEVICE_ERROR_INTERNAL,
			     "failed to set configuration 0x%02x: %s [%i]",
			     configuration,
			     libusb_strerror (rc), rc);
		ret = FALSE;
		goto out;
	}
	rc = libusb_claim_interface (priv->handle, interface);
	if (rc < 0) {
		g_set_error (error, G_USB_DEVICE_ERROR,
			     G_USB_DEVICE_ERROR_INTERNAL,
			     "failed to claim interface 0x%02x: %s [%i]",
			     interface,
			     libusb_strerror (rc), rc);
		ret = FALSE;
		goto out;
	}
out:
	return ret;
}

/**
 * usb_device_disconnect:
 * @usb:  a #GUsbDevice instance
 * @error:  a #GError, or %NULL
 *
 * Disconnecs from the current device.
 *
 * Return value: %TRUE for success
 **/
gboolean
g_usb_device_disconnect (GUsbDevice *usb,
			 GError **error)
{
	gboolean ret = FALSE;
	GUsbDevicePrivate *priv = usb->priv;

	/* already connected */
	if (priv->handle == NULL) {
		g_set_error_literal (error, G_USB_DEVICE_ERROR,
				     G_USB_DEVICE_ERROR_INTERNAL,
				     "not connected to a device");
		goto out;
	}

	/* just close */
	libusb_close (priv->handle);
	priv->handle = NULL;

	/* success */
	ret = TRUE;
out:
	return ret;
}

/**
 * usb_device_get_property:
 **/
static void
g_usb_device_get_property (GObject *object,
			   guint prop_id,
			   GValue *value,
			   GParamSpec *pspec)
{
	GUsbDevice *usb = G_USB_DEVICE (object);
	GUsbDevicePrivate *priv = usb->priv;

	switch (prop_id) {
	case PROP_CONNECTED:
		g_value_set_boolean (value, priv->connected);
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
g_usb_device_set_property (GObject *object,
			   guint prop_id,
			   const GValue *value,
			   GParamSpec *pspec)
{
	switch (prop_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * usb_device_class_init:
 **/
static void
g_usb_device_class_init (GUsbDeviceClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = g_usb_device_finalize;
	object_class->get_property = g_usb_device_get_property;
	object_class->set_property = g_usb_device_set_property;

	/**
	 * GUsbDevice:connected:
	 */
	pspec = g_param_spec_boolean ("connected", NULL, NULL,
				      FALSE,
				      G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_CONNECTED, pspec);

	g_type_class_add_private (klass, sizeof (GUsbDevicePrivate));
}

/**
 * g_usb_device_init:
 **/
static void
g_usb_device_init (GUsbDevice *usb)
{
	usb->priv = G_USB_DEVICE_GET_PRIVATE (usb);
}

/**
 * g_usb_device_finalize:
 **/
static void
g_usb_device_finalize (GObject *object)
{
	GUsbDevice *usb = G_USB_DEVICE (object);
	GUsbDevicePrivate *priv = usb->priv;

	if (priv->handle != NULL)
		libusb_close (priv->handle);

	G_OBJECT_CLASS (g_usb_device_parent_class)->finalize (object);
}

/**
 * usb_device_new:
 *
 * Return value: a new #GUsbDevice object.
 **/
GUsbDevice *
g_usb_device_new (void)
{
	GUsbDevice *usb;
	usb = g_object_new (G_USB_TYPE_DEVICE, NULL);
	return G_USB_DEVICE (usb);
}

