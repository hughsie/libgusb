/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011-2014 Richard Hughes <richard@hughsie.com>
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
 * SECTION:gusb-context
 * @short_description: Per-thread instance integration for libusb
 *
 * This object is used to get a context that is thread safe.
 */

#include "config.h"

#include <libusb-1.0/libusb.h>

#include "gusb-context.h"
#include "gusb-context-private.h"
#include "gusb-device-private.h"
#include "gusb-util.h"

enum {
	PROP_0,
	PROP_LIBUSB_CONTEXT,
	PROP_DEBUG_LEVEL
};

enum {
	DEVICE_ADDED_SIGNAL,
	DEVICE_REMOVED_SIGNAL,
	LAST_SIGNAL
};

/**
 * GUsbContextPrivate:
 *
 * Private #GUsbContext data
 **/
struct _GUsbContextPrivate
{
	GPtrArray			*devices;
	GThread				*thread_event;
	gboolean			 done_enumerate;
	volatile gint			 thread_event_run;
	guint				 hotplug_poll_id;
	int				 debug_level;
	libusb_context			*ctx;
	libusb_hotplug_callback_handle	 hotplug_id;
};

static guint signals[LAST_SIGNAL] = { 0 };

static void g_usb_context_initable_iface_init (GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE (GUsbContext, g_usb_context, G_TYPE_OBJECT,
                         G_ADD_PRIVATE (GUsbContext)
                         G_IMPLEMENT_INTERFACE(G_TYPE_INITABLE,
                                               g_usb_context_initable_iface_init))

static void
g_usb_context_finalize (GObject *object)
{
	GUsbContext *context = G_USB_CONTEXT (object);
	GUsbContextPrivate *priv = context->priv;

	/* this is safe to call even when priv->hotplug_id is unset */
	g_atomic_int_dec_and_test (&priv->thread_event_run);
	libusb_hotplug_deregister_callback (priv->ctx, priv->hotplug_id);
	g_thread_join (priv->thread_event);

	if (priv->hotplug_poll_id > 0)
		g_source_remove (priv->hotplug_poll_id);
	g_ptr_array_unref (priv->devices);

	libusb_exit (priv->ctx);

	G_OBJECT_CLASS (g_usb_context_parent_class)->finalize (object);
}

static void
g_usb_context_get_property (GObject    *object,
                            guint       prop_id,
                            GValue     *value,
                            GParamSpec *pspec)
{
	GUsbContext *context = G_USB_CONTEXT (object);
	GUsbContextPrivate *priv = context->priv;

	switch (prop_id) {
	case PROP_LIBUSB_CONTEXT:
		g_value_set_pointer (value, priv->ctx);
		break;
	case PROP_DEBUG_LEVEL:
		g_value_set_int (value, priv->debug_level);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
g_usb_context_set_property (GObject      *object,
                            guint         prop_id,
                            const GValue *value,
                            GParamSpec   *pspec)
{
	GUsbContext *context = G_USB_CONTEXT (object);
	GUsbContextPrivate *priv = context->priv;

	switch (prop_id) {
	case PROP_DEBUG_LEVEL:
		priv->debug_level = g_value_get_int (value);
		libusb_set_debug (priv->ctx, priv->debug_level);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
g_usb_context_class_init (GUsbContextClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize		= g_usb_context_finalize;
	object_class->get_property	= g_usb_context_get_property;
	object_class->set_property	= g_usb_context_set_property;

	/**
	 * GUsbContext:libusb_context:
	 */
	pspec = g_param_spec_pointer ("libusb_context", NULL, NULL,
				      G_PARAM_READABLE);
	g_object_class_install_property (object_class, PROP_LIBUSB_CONTEXT,
					 pspec);

	/**
	 * GUsbContext:debug_level:
	 */
	pspec = g_param_spec_int ("debug_level", NULL, NULL,
				  0, 3, 0,
				  G_PARAM_READWRITE);
	g_object_class_install_property (object_class, PROP_DEBUG_LEVEL,
					 pspec);

	/**
	 * GUsbContext::device-added:
	 * @context: the #GUsbContext instance that emitted the signal
	 * @device: A #GUsbDevice
	 *
	 * This signal is emitted when a USB device is added.
	 **/
	signals[DEVICE_ADDED_SIGNAL] = g_signal_new ("device-added",
			G_TYPE_FROM_CLASS (klass),
			G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET (GUsbContextClass, device_added),
			NULL,
			NULL,
			g_cclosure_marshal_VOID__OBJECT,
			G_TYPE_NONE,
			1,
			G_USB_TYPE_DEVICE);

	/**
	 * GUsbContext::device-removed:
	 * @context: the #GUsbContext instance that emitted the signal
	 * @device: A #GUsbDevice
	 *
	 * This signal is emitted when a USB device is removed.
	 **/
	signals[DEVICE_REMOVED_SIGNAL] = g_signal_new ("device-removed",
			G_TYPE_FROM_CLASS (klass),
			G_SIGNAL_RUN_LAST,
			G_STRUCT_OFFSET (GUsbContextClass, device_removed),
			NULL,
			NULL,
			g_cclosure_marshal_VOID__OBJECT,
			G_TYPE_NONE,
			1,
			G_USB_TYPE_DEVICE);
}

typedef struct {
	GUsbContext	*context;
	GUsbDevice	*device;
	guint		 signal_id;
} GUsbContextIdleHelper;

static void
g_usb_context_idle_helper_free (GUsbContextIdleHelper *helper)
{
	g_object_unref (helper->context);
	g_object_unref (helper->device);
	g_free (helper);
}

static gboolean
g_usb_context_idle_signal_cb (gpointer user_data)
{
	GUsbContextIdleHelper *helper = (GUsbContextIdleHelper *) user_data;
	g_signal_emit (helper->context, signals[helper->signal_id], 0, helper->device);
	g_usb_context_idle_helper_free (helper);
	return FALSE;
}

static void
g_usb_context_emit_device_add (GUsbContext *context,
                               GUsbDevice  *device)
{
	GUsbContextIdleHelper *helper;
	helper = g_new0 (GUsbContextIdleHelper, 1);
	helper->context = g_object_ref (context);
	helper->device = g_object_ref (device);
	helper->signal_id = DEVICE_ADDED_SIGNAL;
	if (!context->priv->done_enumerate) {
		g_usb_context_idle_signal_cb (helper);
		return;
	}
	g_idle_add (g_usb_context_idle_signal_cb, helper);
}

static void
g_usb_context_emit_device_remove (GUsbContext *context,
                                  GUsbDevice  *device)
{
	GUsbContextIdleHelper *helper;
	helper = g_new0 (GUsbContextIdleHelper, 1);
	helper->context = g_object_ref (context);
	helper->device = g_object_ref (device);
	helper->signal_id = DEVICE_REMOVED_SIGNAL;
	if (!context->priv->done_enumerate) {
		g_usb_context_idle_signal_cb (helper);
		return;
	}
	g_idle_add (g_usb_context_idle_signal_cb, helper);
}

static void
g_usb_context_build_platform_id (GString       *str,
                                 libusb_device *dev)
{
	libusb_device *parent;
	parent = libusb_get_parent (dev);
	if (parent != NULL)
		g_usb_context_build_platform_id (str, parent);
	if (str->len == 0) {
		g_string_append_printf (str, "%02x:",
					libusb_get_bus_number (dev));
	}
	g_string_append_printf (str, "%02x:",
				libusb_get_port_number (dev));
}

static void
g_usb_context_add_device (GUsbContext          *context,
                          struct libusb_device *dev)
{
	GUsbDevice *device = NULL;
	GUsbContextPrivate *priv = context->priv;
	GString *platform_id = NULL;
	guint8 bus;
	guint8 address;
	GError *error = NULL;

	/* does any existing device exist */
	bus = libusb_get_bus_number (dev);
	address = libusb_get_device_address (dev);

	if (priv->done_enumerate)
		device = g_usb_context_find_by_bus_address (context, bus, address, NULL);
	if (device != NULL) {
		g_debug ("%i:%i already exists", bus, address);
		goto out;
	}

	/* build a topology of the device */
	platform_id = g_string_new ("usb:");
	g_usb_context_build_platform_id (platform_id, dev);
	g_string_truncate (platform_id, platform_id->len - 1);

	/* add the device */
	device = _g_usb_device_new (context, dev, platform_id->str, &error);
	if (device == NULL) {
		g_debug ("There was a problem creating the device: %s",
		         error->message);
		goto out;
	}

	if (g_usb_device_get_device_class (device) == 0x09) {
		g_debug ("%02x:%02x is a hub, ignoring", bus, address);
		goto out;
	}
	g_ptr_array_add (priv->devices, g_object_ref (device));
	g_usb_context_emit_device_add (context, device);
out:
	if (device != NULL)
		g_object_unref (device);
	g_string_free (platform_id, TRUE);
}

static void
g_usb_context_remove_device (GUsbContext          *context,
                             struct libusb_device *dev)
{
	GUsbDevice *device = NULL;
	GUsbContextPrivate *priv = context->priv;
	guint8 bus;
	guint8 address;

	/* does any existing device exist */
	bus = libusb_get_bus_number (dev);
	address = libusb_get_device_address (dev);
	device = g_usb_context_find_by_bus_address (context, bus, address, NULL);
	if (device == NULL) {
		g_debug ("%i:%i does not exist", bus, address);
		return;
	}
	g_usb_context_emit_device_remove (context, device);
	g_ptr_array_remove (priv->devices, device);
	g_object_unref (device);
}

static int
g_usb_context_hotplug_cb (struct libusb_context *ctx,
                          struct libusb_device  *dev,
                          libusb_hotplug_event   event,
                          void                  *user_data)
{
	GUsbContext *context = G_USB_CONTEXT (user_data);

	switch (event) {
	case LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED:
		g_usb_context_add_device (context, dev);
		break;
	case LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT:
		g_usb_context_remove_device (context, dev);
		break;
	default:
		break;
	}
	return 0;
}

static void
g_usb_context_rescan (GUsbContext *context)
{
	GList *existing_devices = NULL;
	GList *l;
	GUsbDevice *device;
	GUsbContextPrivate *priv = context->priv;
	gboolean found;
	guint i;
	libusb_device **dev_list = NULL;

	/* add any devices not yet added (duplicates will be filtered */
	libusb_get_device_list (priv->ctx, &dev_list);
	for (i = 0; dev_list && dev_list[i]; i++)
		g_usb_context_add_device (context, dev_list[i]);

	/* copy to a context so we can remove from the array */
	for (i = 0; i < priv->devices->len; i++) {
		device = g_ptr_array_index (priv->devices, i);
		existing_devices = g_list_prepend (existing_devices, device);
	}

	/* look for any removed devices */
	for (l = existing_devices; l != NULL; l = l->next) {
		device = G_USB_DEVICE (l->data);
		found = FALSE;
		for (i = 0; dev_list && dev_list[i]; i++) {
			if (libusb_get_bus_number (dev_list[i]) == g_usb_device_get_bus (device) &&
			    libusb_get_device_address (dev_list[i]) == g_usb_device_get_address (device)) {
				found = TRUE;
				break;
			}
		}
		if (!found) {
			g_usb_context_emit_device_remove (context, device);
			g_ptr_array_remove (priv->devices, device);
		}
	}

	g_list_free (existing_devices);
	libusb_free_device_list (dev_list, 1);
}

static gboolean
g_usb_context_rescan_cb (gpointer user_data)
{
	GUsbContext *context = G_USB_CONTEXT (user_data);
	g_usb_context_rescan (context);
	return TRUE;
}

/**
 * g_usb_context_enumerate:
 * @context: a #GUsbContext
 *
 * Enumerates all the USB devices and adds them to the context.
 *
 * You only need to call this function once, and any subsequent calls
 * are silently ignored.
 *
 * Since: 0.2.2
 **/
void
g_usb_context_enumerate (GUsbContext *context)
{
	GUsbContextPrivate *priv = context->priv;

	/* only ever initially scan once, then rely on hotplug / poll */
	if (priv->done_enumerate)
		return;

	g_usb_context_rescan (context);
	if (!libusb_has_capability (LIBUSB_CAP_HAS_HOTPLUG)) {
		g_debug ("platform does not do hotplug, using polling");
		priv->hotplug_poll_id = g_timeout_add_seconds (1,
							       g_usb_context_rescan_cb,
							       context);
	}
	priv->done_enumerate = TRUE;
}

static gpointer
g_usb_context_event_thread_cb (gpointer data)
{
	GUsbContext *context = G_USB_CONTEXT (data);
	GUsbContextPrivate *priv = context->priv;

	while (g_atomic_int_get (&priv->thread_event_run) > 0)
		libusb_handle_events (priv->ctx);

	return NULL;
}

static void
g_usb_context_init (GUsbContext *context)
{
	GUsbContextPrivate *priv;

	priv = context->priv = g_usb_context_get_instance_private (context);
	priv->devices = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
}

static gboolean
g_usb_context_initable_init (GInitable     *initable,
                             GCancellable  *cancellable,
                             GError       **error)
{
	GUsbContext *context = G_USB_CONTEXT (initable);
	GUsbContextPrivate *priv;
	gint rc;
	libusb_context *ctx;

	priv = context->priv;

	rc = libusb_init (&ctx);
	if (rc < 0) {
		g_set_error (error,
		             G_USB_CONTEXT_ERROR,
		             G_USB_CONTEXT_ERROR_INTERNAL,
		             "failed to init libusb: %s [%i]",
		             g_usb_strerror (rc), rc);
		return FALSE;
	}

	priv->ctx = ctx;
	priv->thread_event_run = 1;
	priv->thread_event = g_thread_new ("GUsbEventThread",
	                                   g_usb_context_event_thread_cb,
	                                   context);

	/* watch for add/remove */
	if (libusb_has_capability (LIBUSB_CAP_HAS_HOTPLUG)) {
		rc = libusb_hotplug_register_callback (priv->ctx,
		                                       LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED |
		                                       LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
		                                       0,
		                                       LIBUSB_HOTPLUG_MATCH_ANY,
		                                       LIBUSB_HOTPLUG_MATCH_ANY,
		                                       LIBUSB_HOTPLUG_MATCH_ANY,
		                                       g_usb_context_hotplug_cb,
		                                       context,
		                                       &context->priv->hotplug_id);
		if (rc != LIBUSB_SUCCESS) {
			g_warning ("Error creating a hotplug callback: %s",
			           g_usb_strerror (rc));
		}
	}

	return TRUE;
}

static void
g_usb_context_initable_iface_init (GInitableIface *iface)
{
	iface->init = g_usb_context_initable_init;
}

/**
 * g_usb_context_error_quark:
 *
 * Return value: Our personal error quark.
 *
 * Since: 0.1.0
 **/
GQuark
g_usb_context_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("g_usb_context_error");
	return quark;
}

/**
 * _g_usb_context_get_context:
 * @context: a #GUsbContext
 *
 * Gets the internal libusb_context.
 *
 * Return value: (transfer none): the libusb_context
 *
 * Since: 0.1.0
 **/
libusb_context *
_g_usb_context_get_context (GUsbContext *context)
{
	return context->priv->ctx;
}

/**
 * g_usb_context_get_source:
 * @context: a #GUsbContext
 * @main_ctx: a #GMainContext, or %NULL
 *
 * This function does nothing.
 *
 * Return value: (transfer none): the #GUsbSource.
 *
 * Since: 0.1.0
 **/
GUsbSource *
g_usb_context_get_source (GUsbContext  *context,
                          GMainContext *main_ctx)
{
	return NULL;
}

/**
 * g_usb_context_set_debug:
 * @context: a #GUsbContext
 * @flags: a GLogLevelFlags such as %G_LOG_LEVEL_ERROR | %G_LOG_LEVEL_INFO, or 0
 *
 * Sets the debug flags which control what is logged to the console.
 *
 * Using %G_LOG_LEVEL_INFO will output to standard out, and everything
 * else logs to standard error.
 *
 * Since: 0.1.0
 **/
void
g_usb_context_set_debug (GUsbContext    *context,
                         GLogLevelFlags  flags)
{
	GUsbContextPrivate *priv;

	g_return_if_fail (G_USB_IS_CONTEXT (context));

	priv = context->priv;

	if (flags & (G_LOG_LEVEL_DEBUG | G_LOG_LEVEL_INFO))
		priv->debug_level = 3;
	else if (flags & (G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_WARNING))
		priv->debug_level = 2;
	else if (flags & (G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_ERROR))
		priv->debug_level = 1;
	else
		priv->debug_level = 0;

	libusb_set_debug (priv->ctx, priv->debug_level);
}

/**
 * g_usb_context_find_by_bus_address:
 * @context: a #GUsbContext
 * @bus: a bus number
 * @address: a bus address
 * @error: A #GError or %NULL
 *
 * Finds a device based on its bus and address values.
 *
 * Return value: (transfer full): a new #GUsbDevice, or %NULL if not found.
 *
 * Since: 0.2.2
 **/
GUsbDevice *
g_usb_context_find_by_bus_address (GUsbContext  *context,
                                   guint8        bus,
                                   guint8        address,
                                   GError      **error)
{
	GUsbContextPrivate *priv;
	GUsbDevice *device = NULL;
	guint i;

	g_return_val_if_fail (G_USB_IS_CONTEXT (context), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	priv = context->priv;

	g_usb_context_enumerate (context);
	for (i = 0; i < priv->devices->len; i++) {
		GUsbDevice *curr = g_ptr_array_index (priv->devices, i);
		if (g_usb_device_get_bus (curr) == bus &&
		    g_usb_device_get_address (curr) == address) {
			device = g_object_ref (curr);
			break;
		}
	}

	if (device == NULL) {
		g_set_error (error,
		             G_USB_DEVICE_ERROR,
		             G_USB_DEVICE_ERROR_NO_DEVICE,
		             "Failed to find device %04x:%04x",
		             bus, address);
	}

	return device;
}

/**
 * g_usb_context_find_by_vid_pid:
 * @context: a #GUsbContext
 * @vid: a vendor ID
 * @pid: a product ID
 * @error: A #GError or %NULL
 *
 * Finds a device based on its bus and address values.
 *
 * Return value: (transfer full): a new #GUsbDevice, or %NULL if not found.
 *
 * Since: 0.2.2
 **/
GUsbDevice *
g_usb_context_find_by_vid_pid (GUsbContext  *context,
                               guint16       vid,
                               guint16       pid,
                               GError      **error)
{
	GUsbContextPrivate *priv;
	GUsbDevice *device = NULL;
	guint i;

	g_return_val_if_fail (G_USB_IS_CONTEXT (context), NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	priv = context->priv;

	g_usb_context_enumerate (context);
	for (i = 0; i < priv->devices->len; i++) {
		GUsbDevice *curr = g_ptr_array_index (priv->devices, i);
		if (g_usb_device_get_vid (curr) == vid &&
		    g_usb_device_get_pid (curr) == pid) {
			device = g_object_ref (curr);
			break;
		}
	}

	if (device == NULL) {
		g_set_error (error,
		             G_USB_DEVICE_ERROR,
		             G_USB_DEVICE_ERROR_NO_DEVICE,
		             "Failed to find device %04x:%04x",
		             vid, pid);
	}

	return device;
}

/**
 * g_usb_context_get_devices:
 * @context: a #GUsbContext
 *
 * Return value: (transfer full): a new #GPtrArray of #GUsbDevice's.
 *
 * Since: 0.2.2
 **/
GPtrArray *
g_usb_context_get_devices (GUsbContext *context)
{
	g_return_val_if_fail (G_USB_IS_CONTEXT (context), NULL);

	g_usb_context_enumerate (context);

	return g_ptr_array_ref (context->priv->devices);
}

/**
 * g_usb_context_new:
 * @error: a #GError, or %NULL
 *
 * Creates a new context for accessing USB devices.
 *
 * Return value: a new %GUsbContext object or %NULL on error.
 *
 * Since: 0.1.0
 **/
GUsbContext *
g_usb_context_new (GError **error)
{
	return g_initable_new (G_USB_TYPE_CONTEXT, NULL, error, NULL);
}
