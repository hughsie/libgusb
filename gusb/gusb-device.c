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
 * SECTION:gusb-device
 * @short_description: GLib device integration for libusb
 *
 * This object is a thin glib wrapper around a libusb_device
 */

#include "config.h"

#include <libusb-1.0/libusb.h>

#include "gusb-context.h"
#include "gusb-device.h"
#include "gusb-device-private.h"

/* libusb_strerror is awaiting merging upstream */
#define libusb_strerror(error) "unknown"

static void     g_usb_device_finalize	(GObject     *object);

#define G_USB_DEVICE_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), G_USB_TYPE_DEVICE, GUsbDevicePrivate))

/**
 * GUsbDevicePrivate:
 *
 * Private #GUsbDevice data
 **/
struct _GUsbDevicePrivate
{
	GUsbContext		*context;
	libusb_device		*device;
	libusb_device_handle	*handle;
	gboolean		 has_descriptor;
	struct libusb_device_descriptor desc;
};

enum {
	PROP_0,
	PROP_LIBUSB_DEVICE,
	PROP_CONTEXT,
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
 * g_usb_device_get_descriptor:
 * @device: a #GUsbDevice
 * @error: a #GError, or %NULL
 *
 * Gets the USB descriptor for the device.
 *
 * Return value: %TRUE on success
 **/
gboolean
g_usb_device_get_descriptor (GUsbDevice *device, GError **error)
{
	int r;
	gboolean ret = TRUE;
	GUsbDevicePrivate *priv = device->priv;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);

	/* already got */
	if (priv->has_descriptor)
		goto out;

	r = libusb_get_device_descriptor (priv->device, &priv->desc);
	if (r < 0) {
		ret = FALSE;
		g_set_error (error,
			     G_USB_DEVICE_ERROR,
			     G_USB_DEVICE_ERROR_INTERNAL,
			     "Failed to get USB descriptor for device: %s",
			     libusb_strerror (r));
		goto out;
	}
	priv->has_descriptor = TRUE;
out:
	return ret;
}

static gboolean
g_usb_device_libusb_error_to_gerror (GUsbDevice *device,
				     gint rc,
				     GError **error)
{
	gboolean ret = FALSE;

	switch (rc) {
	case LIBUSB_SUCCESS:
		ret = TRUE;
		break;
	case LIBUSB_ERROR_TIMEOUT:
		g_set_error (error,
			     G_USB_DEVICE_ERROR,
			     G_USB_DEVICE_ERROR_TIMED_OUT,
			     "the request timed out on %04x:%04x",
			     g_usb_device_get_vid (device),
			     g_usb_device_get_pid (device));
		break;
	case LIBUSB_ERROR_PIPE:
		g_set_error (error,
			     G_USB_DEVICE_ERROR,
			     G_USB_DEVICE_ERROR_NOT_SUPPORTED,
			     "not supported on %04x:%04x",
			     g_usb_device_get_vid (device),
			     g_usb_device_get_pid (device));
		break;
	case LIBUSB_ERROR_NO_DEVICE:
		g_set_error (error,
			     G_USB_DEVICE_ERROR,
			     G_USB_DEVICE_ERROR_NO_DEVICE,
			     "device %04x:%04x has been disconnected",
			     g_usb_device_get_vid (device),
			     g_usb_device_get_pid (device));
		break;
	case LIBUSB_ERROR_ACCESS:
		g_set_error_literal (error,
				     G_USB_DEVICE_ERROR,
				     G_USB_DEVICE_ERROR_NO_DEVICE,
				     "no permissions to open device");
		break;
	case LIBUSB_ERROR_BUSY:
		g_set_error_literal (error,
				     G_USB_DEVICE_ERROR,
				     G_USB_DEVICE_ERROR_NO_DEVICE,
				     "device is busy");
		break;
	default:
		g_set_error (error,
			     G_USB_DEVICE_ERROR,
			     G_USB_DEVICE_ERROR_INTERNAL,
			     "unknown failure: %s [%i]",
			     libusb_strerror (rc), rc);
	}
	return ret;
}

/**
 * g_usb_device_open:
 * @device: a #GUsbDevice
 * @configuration: the configuration index to use, usually '1'
 * @interface: the configuration interface to use, usually '0'
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Opens the device for use.
 *
 * Warning: this function is syncronous.
 *
 * Return value: %TRUE on success
 **/
gboolean
g_usb_device_open (GUsbDevice *device,
		   guint configuration,
		   guint interface,
		   GCancellable *cancellable,
		   GError **error)
{
	gboolean ret = FALSE;
	gint rc;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);

	if (device->priv->handle != NULL) {
		g_set_error_literal (error,
				     G_USB_DEVICE_ERROR,
				     G_USB_DEVICE_ERROR_ALREADY_OPEN,
				     "The device is already open");
		goto out;
	}

	/* open device */
	rc = libusb_open (device->priv->device, &device->priv->handle);
	ret = g_usb_device_libusb_error_to_gerror (device, rc, error);
	if (!ret)
		goto out;

	/* set configuration */
	rc = libusb_set_configuration (device->priv->handle, configuration);
	ret = g_usb_device_libusb_error_to_gerror (device, rc, error);
	if (!ret)
		goto out;

	/* claim interface */
	rc = libusb_claim_interface (device->priv->handle, interface);
	ret = g_usb_device_libusb_error_to_gerror (device, rc, error);
	if (!ret)
		goto out;
out:
	return ret;
}

/**
 * g_usb_device_close:
 * @device: a #GUsbDevice
 * @error: a #GError, or %NULL
 *
 * Closes the device when it is no longer required.
 *
 * Return value: %TRUE on success
 **/
gboolean
g_usb_device_close (GUsbDevice *device, GError **error)
{
	gboolean ret = FALSE;

	if (device->priv->handle == NULL) {
		g_set_error_literal (error,
				     G_USB_DEVICE_ERROR,
				     G_USB_DEVICE_ERROR_NOT_OPEN,
				     "The device has not been opened");
		goto out;
	}

	libusb_close (device->priv->handle);
	device->priv->handle = NULL;
	ret = TRUE;
out:
	return ret;
}

/**
 * g_usb_device_control_transfer:
 * @device: a #GUsbDevice
 * @request_type: the request type field for the setup packet
 * @request: the request field for the setup packet
 * @value: the value field for the setup packet
 * @index: the index field for the setup packet
 * @data: a suitably-sized data buffer for either input or output
 * @length: the length field for the setup packet.
 * @actual_length: the actual number of bytes sent, or %NULL
 * @timeout: timeout timeout (in millseconds) that this function should wait
 * before giving up due to no response being received. For an unlimited
 * timeout, use 0.
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Perform a USB control transfer.
 *
 * Warning: this function is syncronous, and cannot be cancelled.
 *
 * Return value: %TRUE on success
 **/
gboolean
g_usb_device_control_transfer	(GUsbDevice	*device,
				 GUsbDeviceDirection direction,
				 GUsbDeviceRequestType request_type,
				 GUsbDeviceRecipient recipient,
				 guint8		 request,
				 guint16	 value,
				 guint16	 index,
				 guint8		*data,
				 gsize		 length,
				 gsize		*actual_length,
				 guint		 timeout,
				 GCancellable	*cancellable,
				 GError		**error)
{
	gboolean ret = TRUE;
	gint rc;
	guint8 request_type_raw = 0;

	/* munge back to flags */
	if (direction == G_USB_DEVICE_DIRECTION_DEVICE_TO_HOST)
		request_type_raw |= 0x80;
	request_type_raw |= (request_type << 5);
	request_type_raw |= recipient;

	if (device->priv->handle == NULL) {
		ret = FALSE;
		g_set_error_literal (error,
				     G_USB_DEVICE_ERROR,
				     G_USB_DEVICE_ERROR_NOT_OPEN,
				     "The device has not been opened");
		goto out;
	}

	/* TODO: setup an async transfer so we can cancel it */
	rc = libusb_control_transfer (device->priv->handle,
				      request_type_raw,
				      request,
				      value,
				      index,
				      data,
				      length,
				      timeout);
	if (rc < 0) {
		ret = g_usb_device_libusb_error_to_gerror (device, rc, error);
		goto out;
	}
	if (actual_length != NULL)
		*actual_length = rc;
out:
	return ret;
}

/**
 * g_usb_device_bulk_transfer:
 * @device: a #GUsbDevice
 * @endpoint: the address of a valid endpoint to communicate with
 * @data: a suitably-sized data buffer for either input or output
 * @length: the length field for the setup packet.
 * @actual_length: the actual number of bytes sent, or %NULL
 * @timeout: timeout timeout (in millseconds) that this function should wait
 * before giving up due to no response being received. For an unlimited
 * timeout, use 0.
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Perform a USB bulk transfer.
 *
 * Warning: this function is syncronous, and cannot be cancelled.
 *
 * Return value: %TRUE on success
 **/
gboolean
g_usb_device_bulk_transfer	(GUsbDevice	*device,
				 guint8		 endpoint,
				 guint8		*data,
				 gsize		 length,
				 gsize		*actual_length,
				 guint		 timeout,
				 GCancellable	*cancellable,
				 GError		**error)
{
	gboolean ret = TRUE;
	gint rc;
	gint transferred;

	if (device->priv->handle == NULL) {
		ret = FALSE;
		g_set_error_literal (error,
				     G_USB_DEVICE_ERROR,
				     G_USB_DEVICE_ERROR_NOT_OPEN,
				     "The device has not been opened");
		goto out;
	}

	/* TODO: setup an async transfer so we can cancel it */
	rc = libusb_bulk_transfer (device->priv->handle,
				   endpoint,
				   data,
				   length,
				   &transferred,
				   timeout);
	if (rc < 0) {
		ret = g_usb_device_libusb_error_to_gerror (device, rc, error);
		goto out;
	}
	if (actual_length != NULL)
		*actual_length = transferred;
out:
	return ret;
}

/**
 * g_usb_device_interrupt_transfer:
 * @device: a #GUsbDevice
 * @endpoint: the address of a valid endpoint to communicate with
 * @data: a suitably-sized data buffer for either input or output
 * @length: the length field for the setup packet.
 * @actual_length: the actual number of bytes sent, or %NULL
 * @timeout: timeout timeout (in millseconds) that this function should wait
 * before giving up due to no response being received. For an unlimited
 * timeout, use 0.
 * @cancellable: a #GCancellable, or %NULL
 * @error: a #GError, or %NULL
 *
 * Perform a USB interrupt transfer.
 *
 * Warning: this function is syncronous, and cannot be cancelled.
 *
 * Return value: %TRUE on success
 **/
gboolean
g_usb_device_interrupt_transfer	(GUsbDevice	*device,
				 guint8		 endpoint,
				 guint8		*data,
				 gsize		 length,
				 gsize		*actual_length,
				 guint		 timeout,
				 GCancellable	*cancellable,
				 GError		**error)
{
	gboolean ret = TRUE;
	gint rc;
	gint transferred;

	if (device->priv->handle == NULL) {
		ret = FALSE;
		g_set_error_literal (error,
				     G_USB_DEVICE_ERROR,
				     G_USB_DEVICE_ERROR_NOT_OPEN,
				     "The device has not been opened");
		goto out;
	}

	/* TODO: setup an async transfer so we can cancel it */
	rc = libusb_interrupt_transfer (device->priv->handle,
					endpoint,
					data,
					length,
					&transferred,
					timeout);
	if (rc < 0) {
		ret = g_usb_device_libusb_error_to_gerror (device, rc, error);
		goto out;
	}
	if (actual_length != NULL)
		*actual_length = transferred;
out:
	return ret;
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
	case PROP_CONTEXT:
		priv->context = g_value_dup_object (value);
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
	pspec = g_param_spec_pointer ("libusb-device", NULL, NULL,
				      G_PARAM_CONSTRUCT_ONLY|
				      G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_LIBUSB_DEVICE,
					 pspec);

	/**
	 * GUsbDevice:context:
	 */
	pspec = g_param_spec_object ("context", NULL, NULL,
				     G_USB_TYPE_CONTEXT,
				     G_PARAM_CONSTRUCT_ONLY|
				     G_PARAM_WRITABLE);
	g_object_class_install_property (object_class, PROP_CONTEXT,
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

	libusb_unref_device (priv->device);
	g_object_unref (priv->context);

	G_OBJECT_CLASS (g_usb_device_parent_class)->finalize (object);
}

/**
 * _g_usb_device_new:
 *
 * Return value: a new #GUsbDevice object.
 **/
GUsbDevice *
_g_usb_device_new (GUsbContext *context, libusb_device *device)
{
	GObject *obj;
	obj = g_object_new (G_USB_TYPE_DEVICE,
			    "context", context,
			    "libusb-device", device,
			    NULL);
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

/**
 * g_usb_device_get_bus:
 * @device: a #GUsbDevice
 *
 * Gets the USB bus number for the device.
 *
 * Return value: The 8-bit bus number
 **/
guint8
g_usb_device_get_bus (GUsbDevice *device)
{
	return libusb_get_bus_number (device->priv->device);
}

/**
 * g_usb_device_get_address:
 * @device: a #GUsbDevice
 *
 * Gets the USB address for the device.
 *
 * Return value: The 8-bit address
 **/
guint8
g_usb_device_get_address (GUsbDevice *device)
{
	return libusb_get_device_address (device->priv->device);
}

/**
 * g_usb_device_get_vid:
 * @device: a #GUsbDevice
 *
 * Gets the vendor ID for the device.
 *
 * If g_usb_device_get_descriptor() has never been called, then this
 * function will return with 0x0000.
 *
 * Return value: an ID.
 **/
guint16
g_usb_device_get_vid (GUsbDevice *device)
{
	if (!device->priv->has_descriptor)
		return 0x0000;
	return device->priv->desc.idVendor;
}

/**
 * g_usb_device_get_pid:
 * @device: a #GUsbDevice
 *
 * Gets the product ID for the device.
 *
 * If g_usb_device_get_descriptor() has never been called, then this
 * function will return with 0x0000.
 *
 * Return value: an ID.
 **/
guint16
g_usb_device_get_pid (GUsbDevice *device)
{
	if (!device->priv->has_descriptor)
		return 0x0000;
	return device->priv->desc.idProduct;
}
