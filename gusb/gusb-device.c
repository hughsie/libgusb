/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2011 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2011 Hans de Goede <hdegoede@redhat.com>
 * Copyright (C) 2011 Debarshi Ray <debarshir@src.gnome.org>
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

#include <string.h>

#include <libusb-1.0/libusb.h>

#include "gusb-context.h"
#include "gusb-util.h"
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
	gchar			*platform_id;
	GUsbContext		*context;
	libusb_device		*device;
	libusb_device_handle	*handle;
	struct libusb_device_descriptor desc;
};

enum {
	PROP_0,
	PROP_LIBUSB_DEVICE,
	PROP_CONTEXT,
	PROP_PLATFORM_ID
};

G_DEFINE_TYPE (GUsbDevice, g_usb_device, G_TYPE_OBJECT)


/**
 * g_usb_device_error_quark:
 *
 * Return value: Our personal error quark.
 *
 * Since: 0.1.0
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

static gboolean
g_usb_device_libusb_error_to_gerror (GUsbDevice *device,
				     gint rc,
				     GError **error)
{
	gint error_code = G_USB_DEVICE_ERROR_INTERNAL;
	/* Put the rc in libusb's error enum so that gcc warns us if we're
	   missing an error code */
	enum libusb_error result = rc;

	switch (result) {
	case LIBUSB_SUCCESS:
		return TRUE;
	case LIBUSB_ERROR_INVALID_PARAM:
	case LIBUSB_ERROR_NOT_FOUND:
	case LIBUSB_ERROR_NO_MEM:
	case LIBUSB_ERROR_OTHER:
	case LIBUSB_ERROR_INTERRUPTED:
		error_code = G_USB_DEVICE_ERROR_INTERNAL; break;
	case LIBUSB_ERROR_IO:
	case LIBUSB_ERROR_OVERFLOW:
	case LIBUSB_ERROR_PIPE:
		error_code = G_USB_DEVICE_ERROR_IO; break;
	case LIBUSB_ERROR_TIMEOUT:
		error_code = G_USB_DEVICE_ERROR_TIMED_OUT; break;
	case LIBUSB_ERROR_NOT_SUPPORTED:
		error_code = G_USB_DEVICE_ERROR_NOT_SUPPORTED; break;
	case LIBUSB_ERROR_NO_DEVICE:
	case LIBUSB_ERROR_ACCESS:
	case LIBUSB_ERROR_BUSY:
		error_code = G_USB_DEVICE_ERROR_NO_DEVICE; break;
	}

	g_set_error (error, G_USB_DEVICE_ERROR, error_code,
		     "USB error on device %04x:%04x : %s [%i]",
		     g_usb_device_get_vid (device),
		     g_usb_device_get_pid (device),
		     g_usb_strerror (rc), rc);

	return FALSE;
}

static gboolean g_usb_device_not_open_error(GUsbDevice *device, GError **error)
{
	g_set_error (error,
		     G_USB_DEVICE_ERROR,
		     G_USB_DEVICE_ERROR_NOT_OPEN,
		     "Device %04x:%04x has not been opened",
		     g_usb_device_get_vid (device),
		     g_usb_device_get_pid (device));
	return FALSE;
}

static void g_usb_device_async_not_open_error(GUsbDevice	  *device,
					      GAsyncReadyCallback  callback,
					      gpointer		   user_data)
{
	g_simple_async_report_error_in_idle (G_OBJECT (device),
				callback, user_data,
				G_USB_DEVICE_ERROR,
				G_USB_DEVICE_ERROR_NOT_OPEN,
				"Device %04x:%04x has not been opened",
				g_usb_device_get_vid (device),
				g_usb_device_get_pid (device));
}

/**
 * g_usb_device_open:
 * @device: a #GUsbDevice
 * @error: a #GError, or %NULL
 *
 * Opens the device for use.
 *
 * Warning: this function is synchronous.
 *
 * Return value: %TRUE on success
 *
 * Since: 0.1.0
 **/
gboolean
g_usb_device_open (GUsbDevice *device,
		   GError **error)
{
	gint rc;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);

	if (device->priv->handle != NULL) {
		g_set_error (error,
			     G_USB_DEVICE_ERROR,
			     G_USB_DEVICE_ERROR_ALREADY_OPEN,
			     "Device %04x:%04x is already open",
			     g_usb_device_get_vid (device),
			     g_usb_device_get_pid (device));
		return FALSE;
	}

	/* open device */
	rc = libusb_open (device->priv->device, &device->priv->handle);
	return g_usb_device_libusb_error_to_gerror (device, rc, error);
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
	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);

	if (device->priv->handle == NULL)
		return g_usb_device_not_open_error (device, error);

	libusb_close (device->priv->handle);
	device->priv->handle = NULL;
	return TRUE;
}

/**
 * g_usb_device_reset:
 * @device: a #GUsbDevice
 * @error: a #GError, or %NULL
 *
 * Perform a USB port reset to reinitialize a device.
 *
 * If the reset succeeds, the device will appear to disconnected and reconnected.
 * This means the @device will no longer be valid and should be closed and
 * rediscovered.
 *
 * This is a blocking function which usually incurs a noticeable delay.
 *
 * Return value: %TRUE on success
 **/
gboolean
g_usb_device_reset (GUsbDevice *device, GError **error)
{
	gint rc;
	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);

	if (device->priv->handle == NULL)
		return g_usb_device_not_open_error (device, error);

	rc = libusb_reset_device (device->priv->handle);
	return g_usb_device_libusb_error_to_gerror (device, rc, error);
}

/**
 * g_usb_device_get_configuration:
 * @device: a #GUsbDevice
 * @error: a #GError, or %NULL
 *
 * Get the bConfigurationValue for the active configuration of the device.
 *
 * Warning: this function is synchronous.
 *
 * Return value: The bConfigurationValue of the active config, or -1 on error
 *
 * Since: 0.1.0
 **/
gint g_usb_device_get_configuration (GUsbDevice		 *device,
				     GError		**error)
{
	gint rc;
	int config;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), -1);

	if (device->priv->handle == NULL) {
		g_usb_device_not_open_error (device, error);
		return -1;
	}

	rc = libusb_get_configuration (device->priv->handle, &config);
	if (rc != LIBUSB_SUCCESS) {
		g_usb_device_libusb_error_to_gerror (device, rc, error);
		return -1;
	}

	return config;
}

/**
 * g_usb_device_set_configuration:
 * @device: a #GUsbDevice
 * @configuration: the configuration value to set
 * @error: a #GError, or %NULL
 *
 * Set the active bConfigurationValue for the device.
 *
 * Warning: this function is synchronous.
 *
 * Return value: %TRUE on success
 *
 * Since: 0.1.0
 **/
gboolean g_usb_device_set_configuration (GUsbDevice	 *device,
					 gint		  configuration,
					 GError		**error)
{
	gint rc;
	gint config_tmp = 0;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);

	if (device->priv->handle == NULL)
		return g_usb_device_not_open_error (device, error);

	/* verify we've not already set the same configuration */
	rc = libusb_get_configuration (device->priv->handle,
				       &config_tmp);
	if (rc != LIBUSB_SUCCESS) {
		return g_usb_device_libusb_error_to_gerror (device,
							    rc,
							    error);
	}
	if (config_tmp == configuration)
		return TRUE;

	/* different, so change */
	rc = libusb_set_configuration (device->priv->handle, configuration);
	return g_usb_device_libusb_error_to_gerror (device, rc, error);
}

/**
 * g_usb_device_claim_interface:
 * @device: a #GUsbDevice
 * @interface: bInterfaceNumber of the interface you wish to claim
 * @flags: #GUsbDeviceClaimInterfaceFlags
 * @error: a #GError, or %NULL
 *
 * Claim an interface of the device.
 *
 * Return value: %TRUE on success
 *
 * Since: 0.1.0
 **/
gboolean g_usb_device_claim_interface (GUsbDevice		    *device,
				       gint			     interface,
				       GUsbDeviceClaimInterfaceFlags flags,
				       GError			   **error)
{
	gint rc;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);

	if (device->priv->handle == NULL)
		return g_usb_device_not_open_error (device, error);

	if (flags & G_USB_DEVICE_CLAIM_INTERFACE_BIND_KERNEL_DRIVER) {
		rc = libusb_detach_kernel_driver (device->priv->handle,
						  interface);
		if (rc != LIBUSB_SUCCESS &&
		    rc != LIBUSB_ERROR_NOT_FOUND && /* No driver attached */
		    rc != LIBUSB_ERROR_BUSY /* driver rebound already */)
			return g_usb_device_libusb_error_to_gerror (device, rc,
								    error);
	}

	rc = libusb_claim_interface (device->priv->handle, interface);
	return g_usb_device_libusb_error_to_gerror (device, rc, error);
}

/**
 * g_usb_device_release_interface:
 * @device: a #GUsbDevice
 * @interface: bInterfaceNumber of the interface you wish to release
 * @flags: #GUsbDeviceClaimInterfaceFlags
 * @error: a #GError, or %NULL
 *
 * Release an interface of the device.
 *
 * Return value: %TRUE on success
 *
 * Since: 0.1.0
 **/
gboolean g_usb_device_release_interface (GUsbDevice		      *device,
					 gint                          interface,
					 GUsbDeviceClaimInterfaceFlags flags,
					 GError			     **error)
{
	gint rc;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);

	if (device->priv->handle == NULL)
		return g_usb_device_not_open_error (device, error);

	rc = libusb_release_interface (device->priv->handle, interface);
	if (rc != LIBUSB_SUCCESS)
		return g_usb_device_libusb_error_to_gerror (device, rc, error);

	if (flags & G_USB_DEVICE_CLAIM_INTERFACE_BIND_KERNEL_DRIVER) {
		rc = libusb_attach_kernel_driver (device->priv->handle,
						  interface);
		if (rc != LIBUSB_SUCCESS &&
		    rc != LIBUSB_ERROR_NOT_FOUND && /* No driver attached */
		    rc != LIBUSB_ERROR_BUSY /* driver rebound already */)
			return g_usb_device_libusb_error_to_gerror (device, rc,
								    error);
	}

	return TRUE;
}

/**
 * g_usb_device_get_string_descriptor:
 * @desc_index: the index for the string descriptor to retreive
 * @error: a #GError, or %NULL
 *
 * Get a string descriptor from the device. The returned string should be freed
 * with g_free() when no longer needed.
 *
 * Return value: a newly-allocated string holding the descriptor, or NULL on error.
 *
 * Since: 0.1.0
 **/
gchar *g_usb_device_get_string_descriptor (GUsbDevice		 *device,
					   guint8		  desc_index,
					   GError		**error)
{
	gint rc;
	/* libusb_get_string_descriptor_ascii returns max 128 bytes */
	unsigned char buf[128];

	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);

	if (device->priv->handle == NULL) {
		g_usb_device_not_open_error (device, error);
		return NULL;
	}

	rc = libusb_get_string_descriptor_ascii (device->priv->handle,
						 desc_index, buf, sizeof(buf));
	if (rc < 0) {
		g_usb_device_libusb_error_to_gerror (device, rc, error);
		return NULL;
	}

	return g_strdup ((char *)buf);
}

typedef gssize (GUsbDeviceTransferFinishFunc) (GUsbDevice *device, GAsyncResult *res, GError **error);

typedef struct {
	GError				**error;
	GMainLoop			*loop;
	GUsbDeviceTransferFinishFunc	*finish_func;
	gssize				 ret;
} GUsbSyncHelper;

static void
g_usb_device_sync_transfer_cb (GUsbDevice *device,
				  GAsyncResult *res,
				  GUsbSyncHelper *helper)
{
	helper->ret = (*helper->finish_func) (device, res, helper->error);
	g_main_loop_quit (helper->loop);
}

/**
 * g_usb_device_control_transfer:
 * @device: a #GUsbDevice
 * @request_type: the request type field for the setup packet
 * @request: the request field for the setup packet
 * @value: the value field for the setup packet
 * @idx: the index field for the setup packet
 * @data: (array length=length): a suitably-sized data buffer for
 * either input or output
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
 * Warning: this function is synchronous, and cannot be cancelled.
 *
 * Return value: %TRUE on success
 *
 * Since: 0.1.0
 **/
gboolean
g_usb_device_control_transfer	(GUsbDevice	*device,
				 GUsbDeviceDirection direction,
				 GUsbDeviceRequestType request_type,
				 GUsbDeviceRecipient recipient,
				 guint8		 request,
				 guint16	 value,
				 guint16	 idx,
				 guint8		*data,
				 gsize		 length,
				 gsize		*actual_length,
				 guint		 timeout,
				 GCancellable	*cancellable,
				 GError		**error)
{
	GUsbSyncHelper helper;

	helper.ret = -1;
	helper.loop = g_main_loop_new (NULL, FALSE);
	helper.error = error;
	helper.finish_func = g_usb_device_control_transfer_finish;

	g_usb_device_control_transfer_async (device,
					     direction,
					     request_type,
					     recipient,
					     request,
					     value,
					     idx,
					     data,
					     length,
					     timeout,
					     cancellable,
					     (GAsyncReadyCallback) g_usb_device_sync_transfer_cb,
					     &helper);
	g_main_loop_run (helper.loop);
	g_main_loop_unref (helper.loop);

	if (actual_length != NULL)
		*actual_length = (gsize) helper.ret;

	return helper.ret != -1;
}

/**
 * g_usb_device_bulk_transfer:
 * @device: a #GUsbDevice
 * @endpoint: the address of a valid endpoint to communicate with
 * @data: (array length=length): a suitably-sized data buffer for
 * either input or output
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
 * Warning: this function is synchronous, and cannot be cancelled.
 *
 * Return value: %TRUE on success
 *
 * Since: 0.1.0
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
	GUsbSyncHelper helper;

	helper.ret = -1;
	helper.loop = g_main_loop_new (NULL, FALSE);
	helper.error = error;
	helper.finish_func = g_usb_device_bulk_transfer_finish;

	g_usb_device_bulk_transfer_async (device,
					  endpoint,
					  data,
					  length,
					  timeout,
					  cancellable,
					  (GAsyncReadyCallback) g_usb_device_sync_transfer_cb,
					  &helper);
	g_main_loop_run (helper.loop);
	g_main_loop_unref (helper.loop);

	if (actual_length != NULL)
		*actual_length = (gsize) helper.ret;

	return helper.ret != -1;
}

/**
 * g_usb_device_interrupt_transfer:
 * @device: a #GUsbDevice
 * @endpoint: the address of a valid endpoint to communicate with
 * @data: (array length=length): a suitably-sized data buffer for
 * either input or output
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
 * Warning: this function is synchronous, and cannot be cancelled.
 *
 * Return value: %TRUE on success
 *
 * Since: 0.1.0
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
	GUsbSyncHelper helper;

	helper.ret = -1;
	helper.loop = g_main_loop_new (NULL, FALSE);
	helper.error = error;
	helper.finish_func = g_usb_device_interrupt_transfer_finish;

	g_usb_device_interrupt_transfer_async (device,
					       endpoint,
					       data,
					       length,
					       timeout,
					       cancellable,
					       (GAsyncReadyCallback) g_usb_device_sync_transfer_cb,
					       &helper);
	g_main_loop_run (helper.loop);
	g_main_loop_unref (helper.loop);

	if (actual_length != NULL)
		*actual_length = helper.ret;

	return helper.ret != -1;
}

typedef struct {
	GCancellable		*cancellable;
	gulong			 cancellable_id;
	struct libusb_transfer	*transfer;
	GSimpleAsyncResult	*res;
	guint8			*data; /* owned by the user */
} GcmDeviceReq;

static void
g_usb_device_req_free (GcmDeviceReq *req)
{
	if (req->cancellable_id > 0) {
		g_cancellable_disconnect (req->cancellable,
					  req->cancellable_id);
		g_object_unref (req->cancellable);
	}

	libusb_free_transfer (req->transfer);
	g_object_unref (req->res);
	g_slice_free (GcmDeviceReq, req);
}

static gboolean
g_usb_device_libusb_status_to_gerror (gint status,
				      GError **error)
{
	gboolean ret = FALSE;

	switch (status) {
	case LIBUSB_TRANSFER_COMPLETED:
		ret = TRUE;
		break;
	case LIBUSB_TRANSFER_ERROR:
		g_set_error_literal (error,
				     G_USB_DEVICE_ERROR,
				     G_USB_DEVICE_ERROR_FAILED,
				     "transfer failed");
		break;
	case LIBUSB_TRANSFER_TIMED_OUT:
		g_set_error_literal (error,
				     G_USB_DEVICE_ERROR,
				     G_USB_DEVICE_ERROR_TIMED_OUT,
				     "transfer timed out");
		break;
	case LIBUSB_TRANSFER_CANCELLED:
		g_set_error_literal (error,
				     G_USB_DEVICE_ERROR,
				     G_USB_DEVICE_ERROR_CANCELLED,
				     "transfer cancelled");
		break;
	case LIBUSB_TRANSFER_STALL:
		g_set_error_literal (error,
				     G_USB_DEVICE_ERROR,
				     G_USB_DEVICE_ERROR_NOT_SUPPORTED,
				     "endpoint stalled or request not supported");
		break;
	case LIBUSB_TRANSFER_NO_DEVICE:
		g_set_error_literal (error,
				     G_USB_DEVICE_ERROR,
				     G_USB_DEVICE_ERROR_NO_DEVICE,
				     "device was disconnected");
		break;
	case LIBUSB_TRANSFER_OVERFLOW:
		g_set_error_literal (error,
				     G_USB_DEVICE_ERROR,
				     G_USB_DEVICE_ERROR_INTERNAL,
				     "device sent more data than requested");
		break;
	default:
		g_set_error (error,
			     G_USB_DEVICE_ERROR,
			     G_USB_DEVICE_ERROR_INTERNAL,
			     "unknown status [%i]", status);
	}
	return ret;
}



static void
g_usb_device_async_transfer_cb (struct libusb_transfer *transfer)
{
	gboolean ret;
	GError *error = NULL;
	GcmDeviceReq *req = transfer->user_data;

	/* did request fail? */
	ret = g_usb_device_libusb_status_to_gerror (transfer->status,
						    &error);
	if (!ret) {
		g_simple_async_result_set_from_error (req->res, error);
		g_error_free (error);
		goto out;
	}

	/* success */
	g_simple_async_result_set_op_res_gssize (req->res, transfer->actual_length);
out:
	g_simple_async_result_complete_in_idle (req->res);
	g_usb_device_req_free (req);
}

static void
g_usb_device_cancelled_cb (GCancellable *cancellable,
			   GcmDeviceReq *req)
{
	libusb_cancel_transfer (req->transfer);
}

/**********************************************************************/

/**
 * g_usb_device_control_transfer_finish:
 * @device: a #GUsbDevice instance.
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: the actual number of bytes sent, or -1 on error.
 *
 * Since: 0.1.0
 **/
gssize
g_usb_device_control_transfer_finish (GUsbDevice *device,
				      GAsyncResult *res,
				      GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (G_IS_OBJECT (device), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error))
		return -1;

	return g_simple_async_result_get_op_res_gssize (simple);
}

static void
g_usb_device_control_transfer_cb (struct libusb_transfer *transfer)
{
	gboolean ret;
	GError *error = NULL;
	GcmDeviceReq *req = transfer->user_data;

	/* did request fail? */
	ret = g_usb_device_libusb_status_to_gerror (transfer->status,
						    &error);
	if (!ret) {
		g_simple_async_result_set_from_error (req->res, error);
		g_error_free (error);
		goto out;
	}

	/* success */
	g_simple_async_result_set_op_res_gssize (req->res, transfer->actual_length);
	memmove (req->data,
		 transfer->buffer + LIBUSB_CONTROL_SETUP_SIZE,
		 (gsize) transfer->actual_length);
out:
	g_simple_async_result_complete_in_idle (req->res);
	g_usb_device_req_free (req);
}

/**
 * g_usb_device_control_transfer_async:
 * @device: a #GUsbDevice
 * @data: (array length=length): a suitably-sized data buffer for
 * either input or output
 * @length: the length field for the setup packet.
 * @timeout: timeout timeout (in millseconds) that this function should wait
 * before giving up due to no response being received. For an unlimited
 * timeout, use 0.
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Do an async control transfer
 *
 * Since: 0.1.0
 **/
void
g_usb_device_control_transfer_async	(GUsbDevice	*device,
					 GUsbDeviceDirection direction,
					 GUsbDeviceRequestType request_type,
					 GUsbDeviceRecipient recipient,
					 guint8		 request,
					 guint16	 value,
					 guint16	 idx,
					 guint8		*data,
					 gsize		 length,
					 guint		 timeout,
					 GCancellable	*cancellable,
					 GAsyncReadyCallback callback,
					 gpointer user_data)
{
	GcmDeviceReq *req;
	GError *error = NULL;
	gint rc;
	guint8 request_type_raw = 0;
	guint8 *data_raw;
	GSimpleAsyncResult *res;

	g_return_if_fail (G_USB_IS_DEVICE (device));

	if (device->priv->handle == NULL) {
		g_usb_device_async_not_open_error (device, callback, user_data);
		return;
	}

	res = g_simple_async_result_new (G_OBJECT (device),
					 callback,
					 user_data,
					 g_usb_device_control_transfer_async);

	req = g_slice_new0 (GcmDeviceReq);
	req->res = res;
	req->transfer = libusb_alloc_transfer (0);
	req->data = data;

	/* setup cancellation */
	if (cancellable != NULL) {
		req->cancellable = g_object_ref (cancellable);
		req->cancellable_id = g_cancellable_connect (req->cancellable,
							     G_CALLBACK (g_usb_device_cancelled_cb),
							     req,
							     NULL);
	}

	/* munge back to flags */
	if (direction == G_USB_DEVICE_DIRECTION_DEVICE_TO_HOST)
		request_type_raw |= 0x80;
	request_type_raw |= (request_type << 5);
	request_type_raw |= recipient;

	data_raw = g_malloc0 (length + LIBUSB_CONTROL_SETUP_SIZE);
	memmove (data_raw + LIBUSB_CONTROL_SETUP_SIZE, data, length);

	/* fill in setup packet */
	libusb_fill_control_setup (data_raw, request_type_raw, request, value, idx, length);

	/* fill in transfer details */
	libusb_fill_control_transfer (req->transfer,
				      device->priv->handle,
				      data_raw,
				      g_usb_device_control_transfer_cb,
				      req,
				      timeout);

	/* submit transfer */
	rc = libusb_submit_transfer (req->transfer);
	if (rc < 0) {
		g_usb_device_libusb_error_to_gerror (device, rc, &error);
		g_simple_async_report_gerror_in_idle (G_OBJECT (device),
						      callback,
						      user_data,
						      error);
		g_error_free (error);
		g_usb_device_req_free (req);
		return;
	}

	/* setup with the default mainloop */
	g_usb_context_get_source (device->priv->context, NULL);
}

/**********************************************************************/

/**
 * g_usb_device_bulk_transfer_finish:
 * @device: a #GUsbDevice instance.
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: the actual number of bytes sent, or -1 on error.
 *
 * Since: 0.1.0
 **/
gssize
g_usb_device_bulk_transfer_finish (GUsbDevice *device,
				   GAsyncResult *res,
				   GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (G_IS_OBJECT (device), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error))
		return -1;

	return g_simple_async_result_get_op_res_gssize (simple);
}

/**
 * g_usb_device_bulk_transfer_async:
 * @device: a #GUsbDevice instance.
 * @endpoint: the address of a valid endpoint to communicate with
 * @data: (array length=length): a suitably-sized data buffer for
 * either input or output
 * @length: the length field for the setup packet.
 * @timeout: timeout timeout (in millseconds) that this function should wait
 * before giving up due to no response being received. For an unlimited
 * timeout, use 0.
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Do an async bulk transfer
 *
 * Since: 0.1.0
 **/
void
g_usb_device_bulk_transfer_async (GUsbDevice *device,
				  guint8 endpoint,
				  guint8 *data,
				  gsize length,
				  guint timeout,
				  GCancellable *cancellable,
				  GAsyncReadyCallback callback,
				  gpointer user_data)
{
	GcmDeviceReq *req;
	GError *error = NULL;
	gint rc;
	GSimpleAsyncResult *res;

	g_return_if_fail (G_USB_IS_DEVICE (device));

	if (device->priv->handle == NULL) {
		g_usb_device_async_not_open_error (device, callback, user_data);
		return;
	}

	res = g_simple_async_result_new (G_OBJECT (device),
					 callback,
					 user_data,
					 g_usb_device_bulk_transfer_async);

	req = g_slice_new0 (GcmDeviceReq);
	req->res = res;
	req->transfer = libusb_alloc_transfer (0);

	/* setup cancellation */
	if (cancellable != NULL) {
		req->cancellable = g_object_ref (cancellable);
		req->cancellable_id = g_cancellable_connect (req->cancellable,
							     G_CALLBACK (g_usb_device_cancelled_cb),
							     req,
							     NULL);
	}

	/* fill in transfer details */
	libusb_fill_bulk_transfer (req->transfer,
				   device->priv->handle,
				   endpoint,
				   data,
				   length,
				   g_usb_device_async_transfer_cb,
				   req,
				   timeout);

	/* submit transfer */
	rc = libusb_submit_transfer (req->transfer);
	if (rc < 0) {
		g_usb_device_libusb_error_to_gerror (device, rc, &error);
		g_simple_async_report_gerror_in_idle (G_OBJECT (device),
						      callback,
						      user_data,
						      error);
		g_error_free (error);
		g_usb_device_req_free (req);
		return;
	}

	/* setup with the default mainloop */
	g_usb_context_get_source (device->priv->context, NULL);
}

/**********************************************************************/

/**
 * g_usb_device_interrupt_transfer_finish:
 * @device: a #GUsbDevice instance.
 * @res: the #GAsyncResult
 * @error: A #GError or %NULL
 *
 * Gets the result from the asynchronous function.
 *
 * Return value: the actual number of bytes sent, or -1 on error.
 *
 * Since: 0.1.0
 **/
gssize
g_usb_device_interrupt_transfer_finish (GUsbDevice *device,
					GAsyncResult *res,
					GError **error)
{
	GSimpleAsyncResult *simple;

	g_return_val_if_fail (G_IS_OBJECT (device), FALSE);
	g_return_val_if_fail (G_IS_SIMPLE_ASYNC_RESULT (res), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	simple = G_SIMPLE_ASYNC_RESULT (res);
	if (g_simple_async_result_propagate_error (simple, error))
		return -1;

	return g_simple_async_result_get_op_res_gssize (simple);
}

/**
 * g_usb_device_interrupt_transfer_async:
 * @device: a #GUsbDevice instance.
 * @endpoint: the address of a valid endpoint to communicate with
 * @data: (array length=length): a suitably-sized data buffer for
 * either input or output
 * @length: the length field for the setup packet.
 * @timeout: timeout timeout (in millseconds) that this function should wait
 * before giving up due to no response being received. For an unlimited
 * timeout, use 0.
 * @cancellable: a #GCancellable, or %NULL
 * @callback: the function to run on completion
 * @user_data: the data to pass to @callback
 *
 * Do an async interrupt transfer
 *
 * Since: 0.1.0
 **/
void
g_usb_device_interrupt_transfer_async (GUsbDevice *device,
				       guint8 endpoint,
				       guint8 *data,
				       gsize length,
				       guint timeout,
				       GCancellable *cancellable,
				       GAsyncReadyCallback callback,
				       gpointer user_data)
{
	GcmDeviceReq *req;
	GError *error = NULL;
	gint rc;
	GSimpleAsyncResult *res;

	g_return_if_fail (G_USB_IS_DEVICE (device));

	if (device->priv->handle == NULL) {
		g_usb_device_async_not_open_error (device, callback, user_data);
		return;
	}

	res = g_simple_async_result_new (G_OBJECT (device),
					 callback,
					 user_data,
					 g_usb_device_interrupt_transfer_async);

	req = g_slice_new0 (GcmDeviceReq);
	req->res = res;
	req->transfer = libusb_alloc_transfer (0);

	/* setup cancellation */
	if (cancellable != NULL) {
		req->cancellable = g_object_ref (cancellable);
		req->cancellable_id = g_cancellable_connect (req->cancellable,
							     G_CALLBACK (g_usb_device_cancelled_cb),
							     req,
							     NULL);
	}

	/* fill in transfer details */
	libusb_fill_interrupt_transfer (req->transfer,
					device->priv->handle,
					endpoint,
					data,
					length,
					g_usb_device_async_transfer_cb,
					req,
					timeout);

	/* submit transfer */
	rc = libusb_submit_transfer (req->transfer);
	if (rc < 0) {
		g_usb_device_libusb_error_to_gerror (device, rc, &error);
		g_simple_async_report_gerror_in_idle (G_OBJECT (device),
						      callback,
						      user_data,
						      error);
		g_error_free (error);
		g_usb_device_req_free (req);
		return;
	}

	/* setup with the default mainloop */
	g_usb_context_get_source (device->priv->context, NULL);
}

/**********************************************************************/

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
	case PROP_PLATFORM_ID:
		priv->platform_id = g_value_dup_string (value);
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
	gint rc;

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

	rc = libusb_get_device_descriptor (priv->device, &priv->desc);
	if (rc != LIBUSB_SUCCESS)
		g_warning ("Failed to get USB descriptor for device: %s",
			   g_usb_strerror (rc));

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

	/**
	 * GUsbDevice:platform-id:
	 */
	pspec = g_param_spec_string ("platform-id", NULL, NULL,
				     NULL,
				     G_PARAM_CONSTRUCT_ONLY|
				     G_PARAM_WRITABLE);
	g_object_class_install_property (object_class, PROP_PLATFORM_ID,
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

	g_free (priv->platform_id);
	libusb_unref_device (priv->device);
	g_object_unref (priv->context);

	G_OBJECT_CLASS (g_usb_device_parent_class)->finalize (object);
}

/**
 * _g_usb_device_new:
 *
 * Return value: a new #GUsbDevice object.
 *
 * Since: 0.1.0
 **/
GUsbDevice *
_g_usb_device_new (GUsbContext *context,
		   libusb_device *device,
		   GUdevDevice *udev)
{
	GObject *obj;
	obj = g_object_new (G_USB_TYPE_DEVICE,
			    "context", context,
			    "libusb-device", device,
			    "platform-id", g_udev_device_get_sysfs_path (udev),
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
 * g_usb_device_get_platform_id:
 * @device: a #GUsbDevice
 *
 * Gets the platform identifier for the device.
 * On Linux, this is the full sysfs path of the device
 *
 * Return value: The platform ID, or %NULL
 *
 * Since: 0.1.1
 **/
const gchar *
g_usb_device_get_platform_id (GUsbDevice *device)
{
	return device->priv->platform_id;
}

/**
 * g_usb_device_get_bus:
 * @device: a #GUsbDevice
 *
 * Gets the USB bus number for the device.
 *
 * Return value: The 8-bit bus number
 *
 * Since: 0.1.0
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
 *
 * Since: 0.1.0
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
 * Return value: an ID.
 *
 * Since: 0.1.0
 **/
guint16
g_usb_device_get_vid (GUsbDevice *device)
{
	return device->priv->desc.idVendor;
}

/**
 * g_usb_device_get_pid:
 * @device: a #GUsbDevice
 *
 * Gets the product ID for the device.
 *
 * Return value: an ID.
 *
 * Since: 0.1.0
 **/
guint16
g_usb_device_get_pid (GUsbDevice *device)
{
	return device->priv->desc.idProduct;
}

/**
 * g_usb_device_get_manufacturer_index:
 * @device: a #GUsbDevice
 *
 * Gets the index for the Manufacturer string descriptor.
 *
 * Return value: a string descriptor index.
 *
 * Since: 0.1.0
 **/
guint8
g_usb_device_get_manufacturer_index (GUsbDevice *device)
{
	return device->priv->desc.iManufacturer;
}

/**
 * g_usb_device_get_product_index:
 * @device: a #GUsbDevice
 *
 * Gets the index for the Product string descriptor.
 *
 * Return value: a string descriptor index.
 *
 * Since: 0.1.0
 **/
guint8
g_usb_device_get_product_index (GUsbDevice *device)
{
	return device->priv->desc.iProduct;
}

/**
 * g_usb_device_get_serial_number_index:
 * @device: a #GUsbDevice
 *
 * Gets the index for the Serial Number string descriptor.
 *
 * Return value: a string descriptor index.
 *
 * Since: 0.1.0
 **/
guint8
g_usb_device_get_serial_number_index (GUsbDevice *device)
{
	return device->priv->desc.iSerialNumber;
}
