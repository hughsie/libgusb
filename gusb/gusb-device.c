/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010-2015 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2011 Hans de Goede <hdegoede@redhat.com>
 * Copyright (C) 2011 Debarshi Ray <debarshir@src.gnome.org>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

/**
 * SECTION:gusb-device
 * @short_description: GLib device integration for libusb
 *
 * This object is a thin glib wrapper around a libusb_device
 */

#include "config.h"

#include <string.h>

#include <libusb.h>

#include "gusb-context.h"
#include "gusb-context-private.h"
#include "gusb-util.h"
#include "gusb-device.h"
#include "gusb-device-private.h"
#include "gusb-interface-private.h"

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
	PROP_PLATFORM_ID,
	N_PROPERTIES
};

static GParamSpec *pspecs[N_PROPERTIES] = { NULL, };

static void g_usb_device_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GUsbDevice, g_usb_device, G_TYPE_OBJECT,
			 G_ADD_PRIVATE (GUsbDevice)
			 G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
						g_usb_device_initable_iface_init))

/**
 * g_usb_device_error_quark:
 *
 * Return value: Our personal error quark.
 *
 * Since: 0.1.0
 **/
G_DEFINE_QUARK (g-usb-device-error-quark, g_usb_device_error)

static void
g_usb_device_finalize (GObject *object)
{
	GUsbDevice *device = G_USB_DEVICE (object);
	GUsbDevicePrivate *priv = device->priv;

	g_free (priv->platform_id);

	G_OBJECT_CLASS (g_usb_device_parent_class)->finalize (object);
}

static void
g_usb_device_dispose (GObject *object)
{
	GUsbDevice *device = G_USB_DEVICE (object);
	GUsbDevicePrivate *priv = device->priv;

	g_clear_pointer (&priv->device, libusb_unref_device);
	g_clear_object (&priv->context);

	G_OBJECT_CLASS (g_usb_device_parent_class)->dispose (object);
}

static void
g_usb_device_get_property (GObject    *object,
			   guint       prop_id,
			   GValue     *value,
			   GParamSpec *pspec)
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

static void
set_libusb_device (GUsbDevice           *device,
		   struct libusb_device *dev)
{
	GUsbDevicePrivate *priv = device->priv;

	g_clear_pointer (&priv->device, libusb_unref_device);

	if (dev != NULL)
		priv->device = libusb_ref_device (dev);
}

static void
g_usb_device_set_property (GObject      *object,
			   guint	 prop_id,
			   const GValue *value,
			   GParamSpec   *pspec)
{
	GUsbDevice *device = G_USB_DEVICE (object);
	GUsbDevicePrivate *priv = device->priv;

	switch (prop_id) {
	case PROP_LIBUSB_DEVICE:
		set_libusb_device (device, g_value_get_pointer (value));
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

static void
g_usb_device_constructed (GObject *object)
{
	GUsbDevice *device = G_USB_DEVICE (object);
	GUsbDevicePrivate *priv;
	gint rc;

	priv = device->priv;

	if (!priv->device)
		g_error("constructed without a libusb_device");

	rc = libusb_get_device_descriptor (priv->device, &priv->desc);
	if (rc != LIBUSB_SUCCESS)
		g_warning ("Failed to get USB descriptor for device: %s",
			   g_usb_strerror (rc));

	G_OBJECT_CLASS (g_usb_device_parent_class)->constructed (object);
}

static void
g_usb_device_class_init (GUsbDeviceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = g_usb_device_finalize;
	object_class->dispose = g_usb_device_dispose;
	object_class->get_property = g_usb_device_get_property;
	object_class->set_property = g_usb_device_set_property;
	object_class->constructed = g_usb_device_constructed;

	/**
	 * GUsbDevice:libusb_device:
	 */
	pspecs[PROP_LIBUSB_DEVICE] =
		g_param_spec_pointer ("libusb-device", NULL, NULL,
				      G_PARAM_CONSTRUCT_ONLY|
				      G_PARAM_READWRITE);

	/**
	 * GUsbDevice:context:
	 */
	pspecs[PROP_CONTEXT] =
		g_param_spec_object ("context", NULL, NULL,
				     G_USB_TYPE_CONTEXT,
				     G_PARAM_CONSTRUCT_ONLY|
				     G_PARAM_WRITABLE);

	/**
	 * GUsbDevice:platform-id:
	 */
	pspecs[PROP_PLATFORM_ID] =
		g_param_spec_string ("platform-id", NULL, NULL,
				     NULL,
				     G_PARAM_CONSTRUCT_ONLY|
				     G_PARAM_WRITABLE);

	g_object_class_install_properties (object_class, N_PROPERTIES, pspecs);
}

static void
g_usb_device_init (GUsbDevice *device)
{
	device->priv = g_usb_device_get_instance_private (device);
}

/* not defined in FreeBSD */
#ifndef HAVE_LIBUSB_GET_PARENT
static libusb_device *
libusb_get_parent (libusb_device *dev)
{
	return NULL;
}
#endif

/* not defined in DragonFlyBSD */
#ifndef HAVE_LIBUSB_GET_PORT_NUMBER
static guint8
libusb_get_port_number (libusb_device *dev)
{
	return 0xff;
}
#endif

static void
g_usb_device_build_parent_port_number (GString *str, libusb_device *dev)
{
	libusb_device *parent = libusb_get_parent (dev);
	if (parent != NULL)
		g_usb_device_build_parent_port_number (str, parent);
	g_string_append_printf (str, "%02x:", libusb_get_port_number (dev));
}

static gchar *
g_usb_device_build_platform_id (struct libusb_device *dev)
{
	GString *platform_id;

	/* build a topology of the device */
	platform_id = g_string_new ("usb:");
	g_string_append_printf (platform_id, "%02x:", libusb_get_bus_number (dev));
	g_usb_device_build_parent_port_number (platform_id, dev);
	g_string_truncate (platform_id, platform_id->len - 1);
	return g_string_free (platform_id, FALSE);
}

static gboolean
g_usb_device_initable_init (GInitable     *initable,
			    GCancellable  *cancellable,
			    GError       **error)
{
	GUsbDevice *device = G_USB_DEVICE (initable);
	GUsbDevicePrivate *priv;
	gint rc;

	priv = device->priv;

	if (!priv->device) {
		g_set_error_literal (error, G_USB_DEVICE_ERROR, G_USB_DEVICE_ERROR_INTERNAL,
				     "Constructed without a libusb_device");
		return FALSE;
	}

	rc = libusb_get_device_descriptor (priv->device, &priv->desc);
	if (rc != LIBUSB_SUCCESS) {
		g_set_error (error, G_USB_DEVICE_ERROR, G_USB_DEVICE_ERROR_INTERNAL,
			     "Failed to get USB descriptor for device: %s",
			     g_usb_strerror (rc));
		return FALSE;
	}

	/* this does not change on plug->unplug->plug */
	priv->platform_id = g_usb_device_build_platform_id (priv->device);

	return TRUE;
}

static void
g_usb_device_initable_iface_init (GInitableIface *iface)
{
	iface->init = g_usb_device_initable_init;
}

/**
 * _g_usb_device_new:
 *
 * Return value: a new #GUsbDevice object.
 *
 * Since: 0.1.0
 **/
GUsbDevice *
_g_usb_device_new (GUsbContext    *context,
		   libusb_device  *device,
		   GError	**error)
{
	return g_initable_new (G_USB_TYPE_DEVICE,
			       NULL, error,
			       "context", context,
			       "libusb-device", device,
			       NULL);
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
_g_usb_device_get_device (GUsbDevice *device)
{
	return device->priv->device;
}

static gboolean
g_usb_device_libusb_error_to_gerror (GUsbDevice  *device,
				     gint	 rc,
				     GError     **error)
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
		error_code = G_USB_DEVICE_ERROR_INTERNAL;
		break;
	case LIBUSB_ERROR_IO:
	case LIBUSB_ERROR_OVERFLOW:
	case LIBUSB_ERROR_PIPE:
		error_code = G_USB_DEVICE_ERROR_IO;
		break;
	case LIBUSB_ERROR_TIMEOUT:
		error_code = G_USB_DEVICE_ERROR_TIMED_OUT;
		break;
	case LIBUSB_ERROR_NOT_SUPPORTED:
		error_code = G_USB_DEVICE_ERROR_NOT_SUPPORTED;
		break;
	case LIBUSB_ERROR_ACCESS:
		error_code = G_USB_DEVICE_ERROR_PERMISSION_DENIED;
		break;
	case LIBUSB_ERROR_NO_DEVICE:
	case LIBUSB_ERROR_BUSY:
		error_code = G_USB_DEVICE_ERROR_NO_DEVICE;
		break;
	default:
		break;
	}

	g_set_error (error, G_USB_DEVICE_ERROR, error_code,
		     "USB error on device %04x:%04x : %s [%i]",
		     g_usb_device_get_vid (device),
		     g_usb_device_get_pid (device),
		     g_usb_strerror (rc), rc);

	return FALSE;
}

static gboolean
g_usb_device_not_open_error (GUsbDevice  *device,
			     GError     **error)
{
	g_set_error (error,
		     G_USB_DEVICE_ERROR,
		     G_USB_DEVICE_ERROR_NOT_OPEN,
		     "Device %04x:%04x has not been opened",
		     g_usb_device_get_vid (device),
		     g_usb_device_get_pid (device));
	return FALSE;
}

static void
g_usb_device_async_not_open_error (GUsbDevice	  *device,
				   GAsyncReadyCallback  callback,
				   gpointer	     user_data,
				   gpointer	     source_tag)
{
	g_task_report_new_error (device, callback, user_data, source_tag,
				 G_USB_DEVICE_ERROR,
				 G_USB_DEVICE_ERROR_NOT_OPEN,
				 "Device %04x:%04x has not been opened",
				 g_usb_device_get_vid (device),
				 g_usb_device_get_pid (device));
}

gboolean
_g_usb_device_open_internal (GUsbDevice *device, GError **error)
{
	gint rc;

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
g_usb_device_open (GUsbDevice *device, GError **error)
{
	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* ignore */
	if (g_usb_context_get_flags (device->priv->context) & G_USB_CONTEXT_FLAGS_AUTO_OPEN_DEVICES)
		return TRUE;

	/* open */
	return _g_usb_device_open_internal (device, error);
}

/**
 * g_usb_device_get_custom_index:
 * @device: a #GUsbDevice
 * @class_id: a device class, e.g. 0xff for VENDOR
 * @subclass_id: a device subclass
 * @protocol_id: a protocol number
 * @error: a #GError, or %NULL
 *
 * Gets the string index from the vendor class interface descriptor.
 *
 * Return value: a non-zero index, or 0x00 for failure
 *
 * Since: 0.2.5
 **/
guint8
g_usb_device_get_custom_index (GUsbDevice *device,
			       guint8      class_id,
			       guint8      subclass_id,
			       guint8      protocol_id,
			       GError    **error)
{
	const struct libusb_interface_descriptor *ifp;
	gint rc;
	guint8 idx = 0x00;
	guint i;
	struct libusb_config_descriptor *config;

	rc = libusb_get_active_config_descriptor (device->priv->device, &config);
	if (!g_usb_device_libusb_error_to_gerror (device, rc, error))
		return 0x00;

	/* find the right data */
	for (i = 0; i < config->bNumInterfaces; i++) {
		ifp = &config->interface[i].altsetting[0];
		if (ifp->bInterfaceClass != class_id)
			continue;
		if (ifp->bInterfaceSubClass != subclass_id)
			continue;
		if (ifp->bInterfaceProtocol != protocol_id)
			continue;
		idx = ifp->iInterface;
		break;
	}

	/* nothing matched */
	if (idx == 0x00) {
		g_set_error (error,
			     G_USB_DEVICE_ERROR,
			     G_USB_DEVICE_ERROR_NOT_SUPPORTED,
			     "no vendor descriptor for class 0x%02x, "
			     "subclass 0x%02x and protocol 0x%02x",
			     class_id, subclass_id, protocol_id);
	}

	libusb_free_config_descriptor (config);
	return idx;
}

/**
 * g_usb_device_get_interface:
 * @device: a #GUsbDevice
 * @class_id: a device class, e.g. 0xff for VENDOR
 * @subclass_id: a device subclass
 * @protocol_id: a protocol number
 * @error: a #GError, or %NULL
 *
 * Gets the first interface that matches the vendor class interface descriptor.
 * If you want to find all the interfaces that match (there may be other
 * 'alternate' interfaces you have to use g_usb_device_get_interfaces() and
 * check each one manally.
 *
 * Return value: (transfer full): a #GUsbInterface or %NULL for not found
 *
 * Since: 0.2.8
 **/
GUsbInterface *
g_usb_device_get_interface (GUsbDevice *device,
			    guint8      class_id,
			    guint8      subclass_id,
			    guint8      protocol_id,
			    GError    **error)
{
	const struct libusb_interface_descriptor *ifp;
	gint rc;
	GUsbInterface *interface = NULL;
	guint i;
	struct libusb_config_descriptor *config;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	rc = libusb_get_active_config_descriptor (device->priv->device, &config);
	if (!g_usb_device_libusb_error_to_gerror (device, rc, error))
		return NULL;

	/* find the right data */
	for (i = 0; i < config->bNumInterfaces; i++) {
		ifp = &config->interface[i].altsetting[0];
		if (ifp->bInterfaceClass != class_id)
			continue;
		if (ifp->bInterfaceSubClass != subclass_id)
			continue;
		if (ifp->bInterfaceProtocol != protocol_id)
			continue;
		interface = _g_usb_interface_new (ifp);
		break;
	}

	/* nothing matched */
	if (interface == NULL) {
		g_set_error (error,
			     G_USB_DEVICE_ERROR,
			     G_USB_DEVICE_ERROR_NOT_SUPPORTED,
			     "no interface for class 0x%02x, "
			     "subclass 0x%02x and protocol 0x%02x",
			     class_id, subclass_id, protocol_id);
	}

	libusb_free_config_descriptor (config);
	return interface;
}

/**
 * g_usb_device_get_interfaces:
 * @device: a #GUsbDevice
 * @error: a #GError, or %NULL
 *
 * Gets all the interfaces exported by the device.
 *
 * Return value: (transfer container) (element-type GUsbInterface): an array of interfaces or %NULL for error
 *
 * Since: 0.2.8
 **/
GPtrArray *
g_usb_device_get_interfaces (GUsbDevice *device, GError **error)
{
	const struct libusb_interface_descriptor *ifp;
	gint rc;
	guint i;
	guint j;
	struct libusb_config_descriptor *config;
	GPtrArray *array = NULL;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	rc = libusb_get_active_config_descriptor (device->priv->device, &config);
	if (!g_usb_device_libusb_error_to_gerror (device, rc, error))
		return NULL;

	/* get all interfaces */
	array = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	for (i = 0; i < config->bNumInterfaces; i++) {
		GUsbInterface *interface = NULL;
		for (j = 0; j < (guint) config->interface[i].num_altsetting; j++) {
			ifp = &config->interface[i].altsetting[j];
			interface = _g_usb_interface_new (ifp);
			g_ptr_array_add (array, interface);
		}
	}

	libusb_free_config_descriptor (config);
	return array;
}

/**
 * g_usb_device_close:
 * @device: a #GUsbDevice
 * @error: a #GError, or %NULL
 *
 * Closes the device when it is no longer required.
 *
 * Return value: %TRUE on success
 *
 * Since: 0.1.0
 **/
gboolean
g_usb_device_close (GUsbDevice  *device,
		    GError     **error)
{
	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* ignore */
	if (g_usb_context_get_flags (device->priv->context) & G_USB_CONTEXT_FLAGS_AUTO_OPEN_DEVICES)
		return TRUE;

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
 *
 * Since: 0.1.0
 **/
gboolean
g_usb_device_reset (GUsbDevice  *device,
		    GError     **error)
{
	gint rc;
	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (device->priv->handle == NULL)
		return g_usb_device_not_open_error (device, error);

	rc = libusb_reset_device (device->priv->handle);
	if (rc == LIBUSB_ERROR_NOT_FOUND)
		return TRUE;
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
gint
g_usb_device_get_configuration (GUsbDevice  *device,
				GError     **error)
{
	gint rc;
	int config;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), -1);
	g_return_val_if_fail (error == NULL || *error == NULL, -1);

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
gboolean
g_usb_device_set_configuration (GUsbDevice  *device,
				gint	 configuration,
				GError     **error)
{
	gint rc;
	gint config_tmp = 0;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

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
gboolean
g_usb_device_claim_interface (GUsbDevice		     *device,
			      gint			    interface,
			      GUsbDeviceClaimInterfaceFlags   flags,
			      GError			**error)
{
	gint rc;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (device->priv->handle == NULL)
		return g_usb_device_not_open_error (device, error);

	if (flags & G_USB_DEVICE_CLAIM_INTERFACE_BIND_KERNEL_DRIVER) {
		rc = libusb_detach_kernel_driver (device->priv->handle,
						  interface);
		if (rc != LIBUSB_SUCCESS &&
		    rc != LIBUSB_ERROR_NOT_FOUND && /* No driver attached */
		    rc != LIBUSB_ERROR_NOT_SUPPORTED && /* win32 */
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
gboolean
g_usb_device_release_interface (GUsbDevice		     *device,
				gint			    interface,
				GUsbDeviceClaimInterfaceFlags   flags,
				GError			**error)
{
	gint rc;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

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
		    rc != LIBUSB_ERROR_NOT_SUPPORTED && /* win32 */
		    rc != LIBUSB_ERROR_BUSY /* driver rebound already */)
			return g_usb_device_libusb_error_to_gerror (device, rc,
								    error);
	}

	return TRUE;
}

/**
 * g_usb_device_set_interface_alt:
 * @device: a #GUsbDevice
 * @interface: bInterfaceNumber of the interface you wish to release
 * @alt: alternative setting number
 * @error: a #GError, or %NULL
 *
 * Sets an alternate setting on an interface.
 *
 * Return value: %TRUE on success
 *
 * Since: 0.2.8
 **/
gboolean
g_usb_device_set_interface_alt (GUsbDevice *device, gint interface,
				guint8 alt, GError **error)
{
	gint rc;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	if (device->priv->handle == NULL)
		return g_usb_device_not_open_error (device, error);

	rc = libusb_set_interface_alt_setting (device->priv->handle, interface, (gint) alt);
	if (rc != LIBUSB_SUCCESS)
		return g_usb_device_libusb_error_to_gerror (device, rc, error);

	return TRUE;
}

/**
 * g_usb_device_get_string_descriptor:
 * @desc_index: the index for the string descriptor to retrieve
 * @error: a #GError, or %NULL
 *
 * Get a string descriptor from the device. The returned string should be freed
 * with g_free() when no longer needed.
 *
 * Return value: a newly-allocated string holding the descriptor, or NULL on error.
 *
 * Since: 0.1.0
 **/
gchar *
g_usb_device_get_string_descriptor (GUsbDevice  *device,
				    guint8       desc_index,
				    GError     **error)
{
	gint rc;
	/* libusb_get_string_descriptor_ascii returns max 128 bytes */
	unsigned char buf[128];

	g_return_val_if_fail (G_USB_IS_DEVICE (device), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

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

	return g_strdup ((const gchar *)buf);
}

/**
 * g_usb_device_get_string_descriptor_bytes_full:
 * @desc_index: the index for the string descriptor to retrieve
 * @langid: the language ID
 * @length: size of the request data buffer
 * @error: a #GError, or %NULL
 *
 * Get a raw string descriptor from the device. The returned string should be freed
 * with g_bytes_unref() when no longer needed.
 *
 * Return value: (transfer full): a possibly UTF-16 string, or NULL on error.
 *
 * Since: 0.3.8
 **/
GBytes *
g_usb_device_get_string_descriptor_bytes_full (GUsbDevice  *device,
					       guint8       desc_index,
					       guint16      langid,
					       gsize        length,
					       GError     **error)
{
	gint rc;
	g_autofree guint8 *buf = g_malloc0(length);

	g_return_val_if_fail (G_USB_IS_DEVICE (device), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	if (device->priv->handle == NULL) {
		g_usb_device_not_open_error (device, error);
		return NULL;
	}

	rc = libusb_get_string_descriptor (device->priv->handle,
					   desc_index, langid,
					   buf, length);
	if (rc < 0) {
		g_usb_device_libusb_error_to_gerror (device, rc, error);
		return NULL;
	}

	return g_bytes_new (buf, rc);
}

/**
 * g_usb_device_get_string_descriptor_bytes:
 * @desc_index: the index for the string descriptor to retrieve
 * @langid: the language ID
 * @error: a #GError, or %NULL
 *
 * Get a raw string descriptor from the device. The returned string should be freed
 * with g_bytes_unref() when no longer needed.
 * The descriptor will be at most 128 btes in length, if you need to
 * issue a request with either a smaller or larger descriptor, you can
 * use g_usb_device_get_string_descriptor_bytes_full instead.
 *
 * Return value: (transfer full): a possibly UTF-16 string, or NULL on error.
 *
 * Since: 0.3.6
 **/
GBytes *
g_usb_device_get_string_descriptor_bytes (GUsbDevice  *device,
					  guint8       desc_index,
					  guint16      langid,
					  GError     **error)
{
	return g_usb_device_get_string_descriptor_bytes_full(device,
							     desc_index,
							     langid,
							     128,
							     error);
}

typedef gssize (GUsbDeviceTransferFinishFunc) (GUsbDevice *device, GAsyncResult *res, GError **error);

typedef struct {
	GError				**error;
	GMainContext			*context;
	GMainLoop			*loop;
	GUsbDeviceTransferFinishFunc	*finish_func;
	gssize				 ret;
} GUsbSyncHelper;

static void
g_usb_device_sync_transfer_cb (GUsbDevice     *device,
			       GAsyncResult   *res,
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
 * @actual_length: (out) (optional): the actual number of bytes sent, or %NULL
 * @timeout: timeout timeout (in milliseconds) that this function should wait
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
g_usb_device_control_transfer (GUsbDevice	    *device,
			       GUsbDeviceDirection    direction,
			       GUsbDeviceRequestType  request_type,
			       GUsbDeviceRecipient    recipient,
			       guint8		 request,
			       guint16		value,
			       guint16		idx,
			       guint8		*data,
			       gsize		  length,
			       gsize		 *actual_length,
			       guint		  timeout,
			       GCancellable	  *cancellable,
			       GError	       **error)
{
	GUsbSyncHelper helper;

	helper.ret = -1;
	helper.context = g_usb_context_get_main_context (device->priv->context);
	helper.loop = g_main_loop_new (helper.context, FALSE);
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
 * @actual_length: (out) (optional): the actual number of bytes sent, or %NULL
 * @timeout: timeout timeout (in milliseconds) that this function should wait
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
g_usb_device_bulk_transfer (GUsbDevice    *device,
			    guint8	 endpoint,
			    guint8	*data,
			    gsize	  length,
			    gsize	 *actual_length,
			    guint	  timeout,
			    GCancellable  *cancellable,
			    GError       **error)
{
	GUsbSyncHelper helper;

	helper.ret = -1;
	helper.context = g_usb_context_get_main_context (device->priv->context);
	helper.loop = g_main_loop_new (helper.context, FALSE);
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
 * @actual_length: (out) (optional): the actual number of bytes sent, or %NULL
 * @timeout: timeout timeout (in milliseconds) that this function should wait
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
g_usb_device_interrupt_transfer	(GUsbDevice    *device,
				 guint8	 endpoint,
				 guint8	*data,
				 gsize	  length,
				 gsize	 *actual_length,
				 guint	  timeout,
				 GCancellable  *cancellable,
				 GError       **error)
{
	GUsbSyncHelper helper;

	helper.ret = -1;
	helper.context = g_usb_context_get_main_context (device->priv->context);
	helper.loop = g_main_loop_new (helper.context, FALSE);
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
	guint8			*data; /* owned by the user */
	guint8			*data_raw; /* owned by the task */
} GcmDeviceReq;

static void
g_usb_device_req_free (GcmDeviceReq *req)
{
	g_free (req->data_raw);
	if (req->cancellable_id > 0) {
		g_cancellable_disconnect (req->cancellable,
					  req->cancellable_id);
		g_object_unref (req->cancellable);
	}

	libusb_free_transfer (req->transfer);
	g_slice_free (GcmDeviceReq, req);
}

static gboolean
g_usb_device_libusb_status_to_gerror (gint     status,
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
	GTask *task = transfer->user_data;
	gboolean ret;
	GError *error = NULL;

	/* did request fail? */
	ret = g_usb_device_libusb_status_to_gerror (transfer->status, &error);
	if (!ret) {
		g_task_return_error (task, error);
	} else {
		g_task_return_int (task, transfer->actual_length);
	}

	g_object_unref (task);
}

static void
g_usb_device_cancelled_cb (GCancellable *cancellable,
			   GcmDeviceReq *req)
{
	libusb_cancel_transfer (req->transfer);
}

static void
g_usb_device_control_transfer_cb (struct libusb_transfer *transfer)
{
	GTask *task = transfer->user_data;
	GcmDeviceReq *req = g_task_get_task_data (task);
	gboolean ret;
	GError *error = NULL;

	/* did request fail? */
	ret = g_usb_device_libusb_status_to_gerror (transfer->status,
						    &error);
	if (!ret) {
		g_task_return_error (task, error);
	} else {
		memmove (req->data,
			 transfer->buffer + LIBUSB_CONTROL_SETUP_SIZE,
			 (gsize) transfer->actual_length);
		g_task_return_int (task, transfer->actual_length);
	}

	g_object_unref (task);
}

/**
 * g_usb_device_control_transfer_async:
 * @device: a #GUsbDevice
 * @data: (array length=length): a suitably-sized data buffer for
 * either input or output
 * @length: the length field for the setup packet.
 * @timeout: timeout timeout (in milliseconds) that this function should wait
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
g_usb_device_control_transfer_async (GUsbDevice	   *device,
				     GUsbDeviceDirection   direction,
				     GUsbDeviceRequestType request_type,
				     GUsbDeviceRecipient   recipient,
				     guint8		request,
				     guint16	       value,
				     guint16	       idx,
				     guint8	       *data,
				     gsize		 length,
				     guint		 timeout,
				     GCancellable	 *cancellable,
				     GAsyncReadyCallback   callback,
				     gpointer	      user_data)
{
	GTask *task;
	GcmDeviceReq *req;
	gint rc;
	guint8 request_type_raw = 0;
	GError *error = NULL;

	g_return_if_fail (G_USB_IS_DEVICE (device));

	if (device->priv->handle == NULL) {
		g_usb_device_async_not_open_error (device, callback, user_data,
						   g_usb_device_control_transfer_async);
		return;
	}

	req = g_slice_new0 (GcmDeviceReq);
	req->transfer = libusb_alloc_transfer (0);
	req->data = data;

	task = g_task_new (device, cancellable, callback, user_data);
	g_task_set_task_data (task, req, (GDestroyNotify)g_usb_device_req_free);

	if (g_task_return_error_if_cancelled (task)) {
		g_object_unref (task);
		return;
	}

	/* munge back to flags */
	if (direction == G_USB_DEVICE_DIRECTION_DEVICE_TO_HOST)
		request_type_raw |= 0x80;
	request_type_raw |= (request_type << 5);
	request_type_raw |= recipient;

	req->data_raw = g_malloc0 (length + LIBUSB_CONTROL_SETUP_SIZE);
	memmove (req->data_raw + LIBUSB_CONTROL_SETUP_SIZE, data, length);

	/* fill in setup packet */
	libusb_fill_control_setup (req->data_raw, request_type_raw,
				   request, value, idx, length);

	/* fill in transfer details */
	libusb_fill_control_transfer (req->transfer,
				      device->priv->handle,
				      req->data_raw,
				      g_usb_device_control_transfer_cb,
				      task,
				      timeout);

	/* submit transfer */
	rc = libusb_submit_transfer (req->transfer);
	if (rc < 0) {
		g_usb_device_libusb_error_to_gerror (device, rc, &error);
		g_task_return_error (task, error);
		g_object_unref (task);
	}

	/* setup cancellation after submission */
	if (cancellable != NULL) {
		req->cancellable = g_object_ref (cancellable);
		req->cancellable_id = g_cancellable_connect (req->cancellable,
							     G_CALLBACK (g_usb_device_cancelled_cb),
							     req,
							     NULL);
	}
}

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
g_usb_device_control_transfer_finish (GUsbDevice    *device,
				      GAsyncResult  *res,
				      GError       **error)
{
	g_return_val_if_fail (G_USB_IS_DEVICE (device), -1);
	g_return_val_if_fail (g_task_is_valid (res, device), -1);
	g_return_val_if_fail (error == NULL || *error == NULL, -1);

	return g_task_propagate_int (G_TASK (res), error);
}

/**
 * g_usb_device_bulk_transfer_async:
 * @device: a #GUsbDevice instance.
 * @endpoint: the address of a valid endpoint to communicate with
 * @data: (array length=length): a suitably-sized data buffer for
 * either input or output
 * @length: the length field for the setup packet.
 * @timeout: timeout timeout (in milliseconds) that this function should wait
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
g_usb_device_bulk_transfer_async (GUsbDevice	  *device,
				  guint8	       endpoint,
				  guint8	      *data,
				  gsize		length,
				  guint		timeout,
				  GCancellable	*cancellable,
				  GAsyncReadyCallback  callback,
				  gpointer	     user_data)
{
	GTask *task;
	GcmDeviceReq *req;
	gint rc;
	GError *error = NULL;

	g_return_if_fail (G_USB_IS_DEVICE (device));

	if (device->priv->handle == NULL) {
		g_usb_device_async_not_open_error (device, callback, user_data,
						   g_usb_device_bulk_transfer_async);
		return;
	}

	req = g_slice_new0 (GcmDeviceReq);
	req->transfer = libusb_alloc_transfer (0);

	task = g_task_new (device, cancellable, callback, user_data);
	g_task_set_task_data (task, req, (GDestroyNotify)g_usb_device_req_free);

	if (g_task_return_error_if_cancelled (task)) {
		g_object_unref (task);
		return;
	}

	/* fill in transfer details */
	libusb_fill_bulk_transfer (req->transfer,
				   device->priv->handle,
				   endpoint,
				   data,
				   length,
				   g_usb_device_async_transfer_cb,
				   task,
				   timeout);

	/* submit transfer */
	rc = libusb_submit_transfer (req->transfer);
	if (rc < 0) {
		g_usb_device_libusb_error_to_gerror (device, rc, &error);
		g_task_return_error (task, error);
		g_object_unref (task);
	}

	/* setup cancellation after submission */
	if (cancellable != NULL) {
		req->cancellable = g_object_ref (cancellable);
		req->cancellable_id = g_cancellable_connect (req->cancellable,
							     G_CALLBACK (g_usb_device_cancelled_cb),
							     req,
							     NULL);
	}
}

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
g_usb_device_bulk_transfer_finish (GUsbDevice    *device,
				   GAsyncResult  *res,
				   GError       **error)
{
	g_return_val_if_fail (G_USB_IS_DEVICE (device), -1);
	g_return_val_if_fail (g_task_is_valid (res, device), -1);
	g_return_val_if_fail (error == NULL || *error == NULL, -1);

	return g_task_propagate_int (G_TASK (res), error);
}

/**
 * g_usb_device_interrupt_transfer_async:
 * @device: a #GUsbDevice instance.
 * @endpoint: the address of a valid endpoint to communicate with
 * @data: (array length=length): a suitably-sized data buffer for
 * either input or output
 * @length: the length field for the setup packet.
 * @timeout: timeout timeout (in milliseconds) that this function should wait
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
g_usb_device_interrupt_transfer_async (GUsbDevice	  *device,
				       guint8	       endpoint,
				       guint8	      *data,
				       gsize		length,
				       guint		timeout,
				       GCancellable	*cancellable,
				       GAsyncReadyCallback  callback,
				       gpointer	     user_data)
{
	GTask *task;
	GcmDeviceReq *req;
	GError *error = NULL;
	gint rc;

	g_return_if_fail (G_USB_IS_DEVICE (device));

	if (device->priv->handle == NULL) {
		g_usb_device_async_not_open_error (device, callback, user_data,
						   g_usb_device_interrupt_transfer_async);
		return;
	}

	req = g_slice_new0 (GcmDeviceReq);
	req->transfer = libusb_alloc_transfer (0);

	task = g_task_new (device, cancellable, callback, user_data);
	g_task_set_task_data (task, req, (GDestroyNotify)g_usb_device_req_free);

	if (g_task_return_error_if_cancelled (task)) {
		g_object_unref (task);
		return;
	}

	/* fill in transfer details */
	libusb_fill_interrupt_transfer (req->transfer,
					device->priv->handle,
					endpoint,
					data,
					length,
					g_usb_device_async_transfer_cb,
					task,
					timeout);

	/* submit transfer */
	rc = libusb_submit_transfer (req->transfer);
	if (rc < 0) {
		g_usb_device_libusb_error_to_gerror (device, rc, &error);
		g_task_return_error (task, error);
		g_object_unref (task);
	}

	/* setup cancellation after submission */
	if (cancellable != NULL) {
		req->cancellable = g_object_ref (cancellable);
		req->cancellable_id = g_cancellable_connect (req->cancellable,
							     G_CALLBACK (g_usb_device_cancelled_cb),
							     req,
							     NULL);
	}
}

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
g_usb_device_interrupt_transfer_finish (GUsbDevice    *device,
					GAsyncResult  *res,
					GError       **error)
{
	g_return_val_if_fail (G_USB_IS_DEVICE (device), -1);
	g_return_val_if_fail (g_task_is_valid (res, device), -1);
	g_return_val_if_fail (error == NULL || *error == NULL, -1);

	return g_task_propagate_int (G_TASK (res), error);
}

/**
 * g_usb_device_get_platform_id:
 * @device: a #GUsbDevice
 *
 * Gets the platform identifier for the device.
 *
 * When the device is removed and then replugged, this value is not expected to
 * be different.
 *
 * Return value: The platform ID, e.g. "usb:02:00:03:01"
 *
 * Since: 0.1.1
 **/
const gchar *
g_usb_device_get_platform_id (GUsbDevice *device)
{
	g_return_val_if_fail (G_USB_IS_DEVICE (device), NULL);

	return device->priv->platform_id;
}

/**
 * g_usb_device_get_parent:
 * @device: a #GUsbDevice instance
 *
 * Gets the device parent if one exists.
 *
 * Return value: (transfer full): #GUsbDevice or %NULL
 *
 * Since: 0.2.4
 **/
GUsbDevice *
g_usb_device_get_parent (GUsbDevice *device)
{
	GUsbDevicePrivate *priv = device->priv;
	libusb_device *parent;

	parent = libusb_get_parent (priv->device);
	if (parent == NULL)
		return NULL;

	return g_usb_context_find_by_bus_address (priv->context,
						  libusb_get_bus_number (parent),
						  libusb_get_device_address (parent),
						  NULL);
}

/**
 * g_usb_device_get_children:
 * @device: a #GUsbDevice instance
 *
 * Gets the device children if any exist.
 *
 * Return value: (transfer full) (element-type GUsbDevice): an array of #GUsbDevice
 *
 * Since: 0.2.4
 **/
GPtrArray *
g_usb_device_get_children (GUsbDevice *device)
{
	GPtrArray *children;
	GUsbDevice *device_tmp;
	GUsbDevicePrivate *priv = device->priv;
	guint i;
	GPtrArray *devices = NULL;

	/* find any devices that have @device as a parent */
	children = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	devices = g_usb_context_get_devices (priv->context);
	for (i = 0; i < devices->len; i++) {
		device_tmp = g_ptr_array_index (devices, i);
		if (priv->device == libusb_get_parent (device_tmp->priv->device))
			g_ptr_array_add (children, g_object_ref (device_tmp));
	}

	g_ptr_array_unref (devices);

	return children;
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
	g_return_val_if_fail (G_USB_IS_DEVICE (device), 0);

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
	g_return_val_if_fail (G_USB_IS_DEVICE (device), 0);

	return libusb_get_device_address (device->priv->device);
}

/**
 * g_usb_device_get_port_number:
 * @device: a #GUsbDevice
 *
 * Gets the USB port number for the device.
 *
 * Return value: The 8-bit port number
 *
 * Since: 0.2.4
 **/
guint8
g_usb_device_get_port_number (GUsbDevice *device)
{
	g_return_val_if_fail (G_USB_IS_DEVICE (device), 0);
	return libusb_get_port_number (device->priv->device);
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
	g_return_val_if_fail (G_USB_IS_DEVICE (device), 0);

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
	g_return_val_if_fail (G_USB_IS_DEVICE (device), 0);

	return device->priv->desc.idProduct;
}

/**
 * g_usb_device_get_release:
 * @device: a #GUsbDevice
 *
 * Gets the BCD firmware version number for the device.
 *
 * Return value: a version number in BCD format.
 *
 * Since: 0.2.8
 **/
guint16
g_usb_device_get_release (GUsbDevice *device)
{
	g_return_val_if_fail (G_USB_IS_DEVICE (device), 0);

	return device->priv->desc.bcdDevice;
}

/**
 * g_usb_device_get_spec:
 * @device: a #GUsbDevice
 *
 * Gets the BCD specification revision for the device. For example,
 * `0x0110` indicates USB 1.1 and 0x0320 indicates USB 3.2
 *
 * Return value: a specification revision in BCD format.
 *
 * Since: 0.3.1
 **/
guint16
g_usb_device_get_spec (GUsbDevice *device)
{
	g_return_val_if_fail (G_USB_IS_DEVICE (device), 0);

	return device->priv->desc.bcdUSB;
}

/**
 * g_usb_device_get_vid_as_str:
 * @device: a #GUsbDevice
 *
 * Gets the vendor ID for the device as a string.
 *
 * Return value: an string ID, or %NULL if not available.
 *
 * Since: 0.2.4
 **/
const gchar *
g_usb_device_get_vid_as_str (GUsbDevice *device)
{
	g_return_val_if_fail (G_USB_IS_DEVICE (device), NULL);
	return _g_usb_context_lookup_vendor (device->priv->context,
					     device->priv->desc.idVendor,
					     NULL);
}

/**
 * g_usb_device_get_pid_as_str:
 * @device: a #GUsbDevice
 *
 * Gets the product ID for the device as a string.
 *
 * Return value: an string ID, or %NULL if not available.
 *
 * Since: 0.2.4
 **/
const gchar *
g_usb_device_get_pid_as_str (GUsbDevice *device)
{
	g_return_val_if_fail (G_USB_IS_DEVICE (device), NULL);
	return _g_usb_context_lookup_product (device->priv->context,
					      device->priv->desc.idVendor,
					      device->priv->desc.idProduct,
					      NULL);
}

/**
 * g_usb_device_get_configuration_index
 * @device: a #GUsbDevice
 *
 * Get the index for the active Configuration string descriptor
 * ie, iConfiguration.
 *
 * Return value: a string descriptor index.
 *
 * Since: 0.3.5
 **/
guint8
g_usb_device_get_configuration_index (GUsbDevice *device)
{
	struct libusb_config_descriptor *config;
	gint rc;
	guint8 index;

	g_return_val_if_fail (G_USB_IS_DEVICE (device), 0);

	rc = libusb_get_active_config_descriptor (device->priv->device, &config);
	g_return_val_if_fail (rc == 0, 0);

	index = config->iConfiguration;

	libusb_free_config_descriptor (config);
	return index;
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
	g_return_val_if_fail (G_USB_IS_DEVICE (device), 0);

	return device->priv->desc.iManufacturer;
}

/**
 * g_usb_device_get_device_class:
 * @device: a #GUsbDevice
 *
 * Gets the device class, typically a #GUsbDeviceClassCode.
 *
 * Return value: a device class number, e.g. 0x09 is a USB hub.
 *
 * Since: 0.1.7
 **/
guint8
g_usb_device_get_device_class (GUsbDevice *device)
{
	g_return_val_if_fail (G_USB_IS_DEVICE (device), 0);

	return device->priv->desc.bDeviceClass;
}

/**
 * g_usb_device_get_device_subclass:
 * @device: a #GUsbDevice
 *
 * Gets the device subclass qualified by the class number.
 * See g_usb_device_get_device_class().
 *
 * Return value: a device subclass number.
 *
 * Since: 0.2.4
 **/
guint8
g_usb_device_get_device_subclass (GUsbDevice *device)
{
	g_return_val_if_fail (G_USB_IS_DEVICE (device), 0);

	return device->priv->desc.bDeviceSubClass;
}

/**
 * g_usb_device_get_device_protocol:
 * @device: a #GUsbDevice
 *
 * Gets the device protocol qualified by the class and subclass numbers.
 * See g_usb_device_get_device_class() and g_usb_device_get_device_subclass().
 *
 * Return value: a device protocol number.
 *
 * Since: 0.2.4
 **/
guint8
g_usb_device_get_device_protocol (GUsbDevice *device)
{
	g_return_val_if_fail (G_USB_IS_DEVICE (device), 0);

	return device->priv->desc.bDeviceProtocol;
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
	g_return_val_if_fail (G_USB_IS_DEVICE (device), 0);

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
	g_return_val_if_fail (G_USB_IS_DEVICE (device), 0);

	return device->priv->desc.iSerialNumber;
}
