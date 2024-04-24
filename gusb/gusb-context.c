/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2011 Hans de Goede <hdegoede@redhat.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

/**
 * SECTION:gusb-context
 * @short_description: Per-thread instance integration for libusb
 *
 * This object is used to get a context that is thread safe.
 */

#include "config.h"

#include <libusb.h>

#include "gusb-context-private.h"
#include "gusb-device-private.h"
#include "gusb-util.h"

enum { PROP_0, PROP_LIBUSB_CONTEXT, PROP_DEBUG_LEVEL, N_PROPERTIES };

enum { DEVICE_ADDED_SIGNAL, DEVICE_REMOVED_SIGNAL, DEVICE_CHANGED_SIGNAL, LAST_SIGNAL };

#define G_USB_CONTEXT_HOTPLUG_POLL_INTERVAL_DEFAULT 1000 /* ms */

#define GET_PRIVATE(o) (g_usb_context_get_instance_private(o))

/**
 * GUsbContextPrivate:
 *
 * Private #GUsbContext data
 **/
typedef struct {
	GMainContext *main_ctx;
	GPtrArray *devices;
	GPtrArray *devices_removed;
	GHashTable *dict_usb_ids;
	GHashTable *dict_replug;
	GThread *thread_event;
	gboolean done_enumerate;
	volatile gint thread_event_run;
	guint hotplug_poll_id;
	guint hotplug_poll_interval;
	int debug_level;
	GUsbContextFlags flags;
	libusb_context *ctx;
	libusb_hotplug_callback_handle hotplug_id;
	GPtrArray *idle_events;
	GMutex idle_events_mutex;
	guint idle_events_id;
} GUsbContextPrivate;

/* not defined in FreeBSD */
#ifndef HAVE_LIBUSB_CAP_HAS_HOTPLUG
#define LIBUSB_CAP_HAS_HOTPLUG 0x0001
#endif

typedef struct {
	GMainLoop *loop;
	GUsbDevice *device;
	guint timeout_id;
} GUsbContextReplugHelper;

static guint signals[LAST_SIGNAL] = {0};
static GParamSpec *pspecs[N_PROPERTIES] = {
    NULL,
};

static void
g_usb_context_initable_iface_init(GInitableIface *iface);

G_DEFINE_TYPE_WITH_CODE(GUsbContext,
			g_usb_context,
			G_TYPE_OBJECT,
			G_ADD_PRIVATE(GUsbContext)
			    G_IMPLEMENT_INTERFACE(G_TYPE_INITABLE,
						  g_usb_context_initable_iface_init))

/* not defined in FreeBSD */
#ifndef HAVE_LIBUSB_HAS_CAPABILITY
static gboolean
libusb_has_capability(int cap)
{
	if (cap == LIBUSB_CAP_HAS_HOTPLUG)
		return TRUE;
	return FALSE;
}
#endif

static void
g_usb_context_replug_helper_free(GUsbContextReplugHelper *replug_helper)
{
	if (replug_helper->timeout_id != 0)
		g_source_remove(replug_helper->timeout_id);
	g_main_loop_unref(replug_helper->loop);
	g_object_unref(replug_helper->device);
	g_free(replug_helper);
}

/* clang-format off */
/**
 * g_usb_context_error_quark:
 *
 * Return value: Our personal error quark.
 *
 * Since: 0.1.0
 **/
G_DEFINE_QUARK (g-usb-context-error-quark, g_usb_context_error)
/* clang-format on */

static void
g_usb_context_dispose(GObject *object)
{
	GUsbContext *self = G_USB_CONTEXT(object);
	GUsbContextPrivate *priv = GET_PRIVATE(self);

	/* this is safe to call even when priv->hotplug_id is unset */
	if (g_atomic_int_dec_and_test(&priv->thread_event_run)) {
		libusb_hotplug_deregister_callback(priv->ctx, priv->hotplug_id);
		g_thread_join(priv->thread_event);
	}

	if (priv->hotplug_poll_id > 0) {
		g_source_remove(priv->hotplug_poll_id);
		priv->hotplug_poll_id = 0;
	}
	if (priv->idle_events_id > 0) {
		g_source_remove(priv->idle_events_id);
		priv->idle_events_id = 0;
	}

	g_clear_pointer(&priv->main_ctx, g_main_context_unref);
	g_clear_pointer(&priv->devices, g_ptr_array_unref);
	g_clear_pointer(&priv->devices_removed, g_ptr_array_unref);
	g_clear_pointer(&priv->dict_usb_ids, g_hash_table_unref);
	g_clear_pointer(&priv->dict_replug, g_hash_table_unref);
	g_clear_pointer(&priv->ctx, libusb_exit);
	g_clear_pointer(&priv->idle_events, g_ptr_array_unref);
	g_mutex_clear(&priv->idle_events_mutex);

	G_OBJECT_CLASS(g_usb_context_parent_class)->dispose(object);
}

static void
g_usb_context_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GUsbContext *self = G_USB_CONTEXT(object);
	GUsbContextPrivate *priv = GET_PRIVATE(self);

	switch (prop_id) {
	case PROP_LIBUSB_CONTEXT:
		g_value_set_pointer(value, priv->ctx);
		break;
	case PROP_DEBUG_LEVEL:
		g_value_set_int(value, priv->debug_level);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
g_usb_context_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GUsbContext *self = G_USB_CONTEXT(object);
	GUsbContextPrivate *priv = GET_PRIVATE(self);

	switch (prop_id) {
	case PROP_DEBUG_LEVEL:
		priv->debug_level = g_value_get_int(value);
#ifdef HAVE_LIBUSB_SET_OPTION
		libusb_set_option(priv->ctx, LIBUSB_OPTION_LOG_LEVEL, priv->debug_level);
#else
		libusb_set_debug(priv->ctx, priv->debug_level);
#endif
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
g_usb_context_class_init(GUsbContextClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->dispose = g_usb_context_dispose;
	object_class->get_property = g_usb_context_get_property;
	object_class->set_property = g_usb_context_set_property;

	/**
	 * GUsbContext:libusb_context:
	 */
	pspecs[PROP_LIBUSB_CONTEXT] =
	    g_param_spec_pointer("libusb_context", NULL, NULL, G_PARAM_READABLE);

	/**
	 * GUsbContext:debug_level:
	 */
	pspecs[PROP_DEBUG_LEVEL] =
	    g_param_spec_int("debug_level", NULL, NULL, 0, 3, 0, G_PARAM_READWRITE);

	g_object_class_install_properties(object_class, N_PROPERTIES, pspecs);

	/**
	 * GUsbContext::device-added:
	 * @self: the #GUsbContext instance that emitted the signal
	 * @device: A #GUsbDevice
	 *
	 * This signal is emitted when a USB device is added.
	 **/
	signals[DEVICE_ADDED_SIGNAL] = g_signal_new("device-added",
						    G_TYPE_FROM_CLASS(klass),
						    G_SIGNAL_RUN_LAST,
						    G_STRUCT_OFFSET(GUsbContextClass, device_added),
						    NULL,
						    NULL,
						    g_cclosure_marshal_VOID__OBJECT,
						    G_TYPE_NONE,
						    1,
						    G_USB_TYPE_DEVICE);

	/**
	 * GUsbContext::device-removed:
	 * @self: the #GUsbContext instance that emitted the signal
	 * @device: A #GUsbDevice
	 *
	 * This signal is emitted when a USB device is removed.
	 **/
	signals[DEVICE_REMOVED_SIGNAL] =
	    g_signal_new("device-removed",
			 G_TYPE_FROM_CLASS(klass),
			 G_SIGNAL_RUN_LAST,
			 G_STRUCT_OFFSET(GUsbContextClass, device_removed),
			 NULL,
			 NULL,
			 g_cclosure_marshal_VOID__OBJECT,
			 G_TYPE_NONE,
			 1,
			 G_USB_TYPE_DEVICE);

	/**
	 * GUsbContext::device-changed:
	 * @self: the #GUsbContext instance that emitted the signal
	 * @device: A #GUsbDevice
	 *
	 * This signal is emitted when a USB device is changed.
	 **/
	signals[DEVICE_CHANGED_SIGNAL] =
	    g_signal_new("device-changed",
			 G_TYPE_FROM_CLASS(klass),
			 G_SIGNAL_RUN_LAST,
			 G_STRUCT_OFFSET(GUsbContextClass, device_changed),
			 NULL,
			 NULL,
			 g_cclosure_marshal_VOID__OBJECT,
			 G_TYPE_NONE,
			 1,
			 G_USB_TYPE_DEVICE);
}

static void
g_usb_context_emit_device_add(GUsbContext *self, GUsbDevice *device)
{
	GUsbContextPrivate *priv = GET_PRIVATE(self);

	/* emitted directly by g_usb_context_enumerate */
	if (!priv->done_enumerate)
		return;

	if (_g_usb_context_has_flag(self, G_USB_CONTEXT_FLAGS_DEBUG))
		g_debug("emitting ::device-added(%s)", g_usb_device_get_platform_id(device));
	g_signal_emit(self, signals[DEVICE_ADDED_SIGNAL], 0, device);
}

static void
g_usb_context_emit_device_remove(GUsbContext *self, GUsbDevice *device)
{
	GUsbContextPrivate *priv = GET_PRIVATE(self);

	/* should not happen, if it does we would not fire any signal */
	if (!priv->done_enumerate)
		return;

	if (_g_usb_context_has_flag(self, G_USB_CONTEXT_FLAGS_DEBUG))
		g_debug("emitting ::device-removed(%s)", g_usb_device_get_platform_id(device));
	g_signal_emit(self, signals[DEVICE_REMOVED_SIGNAL], 0, device);
}

static void
g_usb_context_emit_device_changed(GUsbContext *self, GUsbDevice *device)
{
	GUsbContextPrivate *priv = GET_PRIVATE(self);

	/* should not happen, if it does we would not fire any signal */
	if (!priv->done_enumerate)
		return;

	if (_g_usb_context_has_flag(self, G_USB_CONTEXT_FLAGS_DEBUG))
		g_debug("emitting ::device-changed(%s)", g_usb_device_get_platform_id(device));
	g_signal_emit(self, signals[DEVICE_CHANGED_SIGNAL], 0, device);
}

static void
g_usb_context_add_device(GUsbContext *self, struct libusb_device *dev)
{
	GUsbContextPrivate *priv = GET_PRIVATE(self);
	GUsbContextReplugHelper *replug_helper;
	const gchar *platform_id;
	guint8 bus;
	guint8 address;
	g_autoptr(GError) error = NULL;
	g_autoptr(GUsbDevice) device = NULL;

	/* does any existing device exist */
	bus = libusb_get_bus_number(dev);
	address = libusb_get_device_address(dev);

	if (priv->done_enumerate)
		device = g_usb_context_find_by_bus_address(self, bus, address, NULL);
	if (device != NULL)
		return;

	/* add the device */
	device = _g_usb_device_new(self, dev, &error);
	if (device == NULL) {
		g_debug("There was a problem creating the device: %s", error->message);
		return;
	}

	/* auto-open */
	if (priv->flags & G_USB_CONTEXT_FLAGS_AUTO_OPEN_DEVICES) {
		if (!_g_usb_device_open_internal(device, &error)) {
			g_warning("cannot open the device: %s", error->message);
			return;
		}
	}

	/* add to enumerated list */
	g_ptr_array_add(priv->devices, g_object_ref(device));

	/* if we're waiting for replug, suppress the signal */
	platform_id = g_usb_device_get_platform_id(device);
	replug_helper = g_hash_table_lookup(priv->dict_replug, platform_id);
	if (replug_helper != NULL) {
		g_debug("%s is in replug, ignoring add", platform_id);
		g_object_unref(replug_helper->device);
		replug_helper->device = g_object_ref(device);
		g_main_loop_quit(replug_helper->loop);
		return;
	}

	/* emit signal */
	g_usb_context_emit_device_add(self, device);
}

static void
g_usb_context_remove_device(GUsbContext *self, struct libusb_device *dev)
{
	GUsbContextPrivate *priv = GET_PRIVATE(self);
	GUsbContextReplugHelper *replug_helper;
	const gchar *platform_id;
	guint8 bus;
	guint8 address;
	g_autoptr(GUsbDevice) device = NULL;

	/* does any existing device exist */
	bus = libusb_get_bus_number(dev);
	address = libusb_get_device_address(dev);
	device = g_usb_context_find_by_bus_address(self, bus, address, NULL);
	if (device == NULL) {
		g_debug("%i:%i does not exist", bus, address);
		return;
	}

	/* save this to a lookaside */
	if (priv->flags & G_USB_CONTEXT_FLAGS_SAVE_EVENTS) {
		g_ptr_array_add(priv->devices_removed, g_object_ref(device));
	}

	/* remove from enumerated list */
	g_ptr_array_remove(priv->devices, device);

	/* if we're waiting for replug, suppress the signal */
	platform_id = g_usb_device_get_platform_id(device);
	replug_helper = g_hash_table_lookup(priv->dict_replug, platform_id);
	if (replug_helper != NULL) {
		g_debug("%s is in replug, ignoring remove", platform_id);
		return;
	}

	/* emit signal */
	g_usb_context_emit_device_remove(self, device);
}

/**
 * g_usb_context_load:
 * @self: a #GUsbContext
 * @json_object: a #JsonObject
 * @error: a #GError, or %NULL
 *
 * Loads the context from a JSON object.
 *
 * Return value: %TRUE on success
 *
 * Since: 0.4.0
 **/
gboolean
g_usb_context_load(GUsbContext *self, JsonObject *json_object, GError **error)
{
	return g_usb_context_load_with_tag(self, json_object, NULL, error);
}

/**
 * g_usb_context_load_with_tag:
 * @self: a #GUsbContext
 * @json_object: a #JsonObject
 * @tag: a string tag, e.g. `runtime-reload`, or %NULL
 * @error: a #GError, or %NULL
 *
 * Loads any devices with a specified tag into the context from a JSON object.
 *
 * Return value: %TRUE on success
 *
 * Since: 0.4.1
 **/
gboolean
g_usb_context_load_with_tag(GUsbContext *self,
			    JsonObject *json_object,
			    const gchar *tag,
			    GError **error)
{
	GUsbContextPrivate *priv = GET_PRIVATE(self);
	JsonArray *json_array;
	g_autoptr(GPtrArray) devices_added =
	    g_ptr_array_new_with_free_func((GDestroyNotify)g_object_unref);
	g_autoptr(GPtrArray) devices_remove =
	    g_ptr_array_new_with_free_func((GDestroyNotify)g_object_unref);

	g_return_val_if_fail(G_USB_IS_CONTEXT(self), FALSE);
	g_return_val_if_fail(json_object != NULL, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	/* if not already set */
	priv->done_enumerate = TRUE;

	/* sanity check */
	if (!json_object_has_member(json_object, "UsbDevices")) {
		g_set_error_literal(error,
				    G_IO_ERROR,
				    G_IO_ERROR_INVALID_DATA,
				    "no UsbDevices array");
		return FALSE;
	}

	/* four steps:
	 *
	 * 1. store all the existing devices matching the tag in devices_remove
	 * 2. read the devices in the array:
	 *    - if the platform-id exists: replace the event data & remove from devices_remove
	 *    - otherwise add to devices_added
	 * 3. emit devices in devices_remove
	 * 4. emit devices in devices_added
	 */
	for (guint i = 0; i < priv->devices->len; i++) {
		GUsbDevice *device = g_ptr_array_index(priv->devices, i);
		if (tag == NULL || g_usb_device_has_tag(device, tag))
			g_ptr_array_add(devices_remove, g_object_ref(device));
	}
	json_array = json_object_get_array_member(json_object, "UsbDevices");
	for (guint i = 0; i < json_array_get_length(json_array); i++) {
		JsonNode *node_tmp = json_array_get_element(json_array, i);
		JsonObject *obj_tmp = json_node_get_object(node_tmp);
		g_autoptr(GUsbDevice) device_old = NULL;
		g_autoptr(GUsbDevice) device_tmp =
		    g_object_new(G_USB_TYPE_DEVICE, "context", self, NULL);
		if (!_g_usb_device_load(device_tmp, obj_tmp, error))
			return FALSE;
		if (tag != NULL && !g_usb_device_has_tag(device_tmp, tag))
			continue;

		/* does a device with this platform ID [and the same created date] already exist */
		device_old =
		    g_usb_context_find_by_platform_id(self,
						      g_usb_device_get_platform_id(device_tmp),
						      NULL);
		if (device_old != NULL && g_date_time_equal(g_usb_device_get_created(device_old),
							    g_usb_device_get_created(device_tmp))) {
			g_autoptr(GPtrArray) events = g_usb_device_get_events(device_tmp);
			g_usb_device_clear_events(device_old);
			for (guint j = 0; j < events->len; j++) {
				GUsbDeviceEvent *event = g_ptr_array_index(events, j);
				_g_usb_device_add_event(device_old, event);
			}
			g_usb_context_emit_device_changed(self, device_old);
			g_ptr_array_remove(devices_remove, device_old);
			continue;
		}

		/* new to us! */
		g_ptr_array_add(devices_added, g_object_ref(device_tmp));
	}

	/* emit removes then adds */
	for (guint i = 0; i < devices_remove->len; i++) {
		GUsbDevice *device = g_ptr_array_index(devices_remove, i);
		g_usb_context_emit_device_remove(self, device);
		g_ptr_array_remove(priv->devices, device);
	}
	for (guint i = 0; i < devices_added->len; i++) {
		GUsbDevice *device = g_ptr_array_index(devices_added, i);
		g_ptr_array_add(priv->devices, g_object_ref(device));
		g_usb_context_emit_device_add(self, device);
	}

	/* success */
	return TRUE;
}

/**
 * g_usb_context_save:
 * @self: a #GUsbContext
 * @json_builder: a #JsonBuilder
 * @error: a #GError, or %NULL
 *
 * Saves the context to an existing JSON builder.
 *
 * Return value: %TRUE on success
 *
 * Since: 0.4.0
 **/
gboolean
g_usb_context_save(GUsbContext *self, JsonBuilder *json_builder, GError **error)
{
	return g_usb_context_save_with_tag(self, json_builder, NULL, error);
}

/**
 * g_usb_context_save_with_tag:
 * @self: a #GUsbContext
 * @json_builder: a #JsonBuilder
 * @tag: a string tag, e.g. `runtime-reload`, or %NULL
 * @error: a #GError, or %NULL
 *
 * Saves any devices with a specified tag into an existing JSON builder.
 *
 * Return value: %TRUE on success
 *
 * Since: 0.4.1
 **/
gboolean
g_usb_context_save_with_tag(GUsbContext *self,
			    JsonBuilder *json_builder,
			    const gchar *tag,
			    GError **error)
{
	GUsbContextPrivate *priv = GET_PRIVATE(self);

	g_return_val_if_fail(G_USB_IS_CONTEXT(self), FALSE);
	g_return_val_if_fail(json_builder != NULL, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	/* start */
	g_usb_context_enumerate(self);
	json_builder_begin_object(json_builder);

	/* array of devices */
	json_builder_set_member_name(json_builder, "UsbDevices");
	json_builder_begin_array(json_builder);
	if (priv->flags & G_USB_CONTEXT_FLAGS_SAVE_REMOVED_DEVICES) {
		for (guint i = 0; i < priv->devices_removed->len; i++) {
			GUsbDevice *device = g_ptr_array_index(priv->devices_removed, i);
			if (!_g_usb_device_save(device, json_builder, error))
				return FALSE;
		}
	}
	for (guint i = 0; i < priv->devices->len; i++) {
		GUsbDevice *device = g_ptr_array_index(priv->devices, i);
		if (tag != NULL && !g_usb_device_has_tag(device, tag))
			continue;
		if (!_g_usb_device_save(device, json_builder, error))
			return FALSE;
	}
	json_builder_end_array(json_builder);

	/* success */
	json_builder_end_object(json_builder);
	return TRUE;
}

typedef struct {
	GUsbContext *self;
	libusb_device *dev;
	libusb_hotplug_event event;
} GUsbContextIdleHelper;

static void
g_usb_context_idle_helper_free(GUsbContextIdleHelper *helper)
{
	g_object_unref(helper->self);
	libusb_unref_device(helper->dev);
	g_free(helper);
}

static gpointer
g_usb_context_idle_helper_copy(gconstpointer src, gpointer user_data)
{
	GUsbContextIdleHelper *helper_src = (GUsbContextIdleHelper *)src;
	GUsbContextIdleHelper *helper_dst = g_new0(GUsbContextIdleHelper, 1);
	helper_dst->self = g_object_ref(helper_src->self);
	helper_dst->dev = libusb_ref_device(helper_src->dev);
	helper_dst->event = helper_src->event;
	return helper_dst;
}

/* always in the main thread */
static gboolean
g_usb_context_idle_hotplug_cb(gpointer user_data)
{
	GUsbContext *self = G_USB_CONTEXT(user_data);
	GUsbContextPrivate *priv = GET_PRIVATE(self);
	g_autoptr(GPtrArray) idle_events = NULL;

	/* drain the idle events with the lock held */
	g_mutex_lock(&priv->idle_events_mutex);
	idle_events = g_ptr_array_copy(priv->idle_events, g_usb_context_idle_helper_copy, NULL);
	g_ptr_array_set_size(priv->idle_events, 0);
	priv->idle_events_id = 0;
	g_mutex_unlock(&priv->idle_events_mutex);

	/* run the callbacks when not locked */
	for (guint i = 0; i < idle_events->len; i++) {
		GUsbContextIdleHelper *helper = g_ptr_array_index(idle_events, i);
		switch (helper->event) {
		case LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED:
			g_usb_context_add_device(helper->self, helper->dev);
			break;
		case LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT:
			g_usb_context_remove_device(helper->self, helper->dev);
			break;
		default:
			break;
		}
	}

	/* all done */
	return FALSE;
}

/* this is run in the libusb thread */
static int LIBUSB_CALL
g_usb_context_hotplug_cb(struct libusb_context *ctx,
			 struct libusb_device *dev,
			 libusb_hotplug_event event,
			 void *user_data)
{
	GUsbContext *self = G_USB_CONTEXT(user_data);
	GUsbContextIdleHelper *helper;
	GUsbContextPrivate *priv = GET_PRIVATE(self);
	g_autoptr(GMutexLocker) locker = g_mutex_locker_new(&priv->idle_events_mutex);

	g_assert(locker != NULL);

	/* libusb is returning devices but LIBUSB_HOTPLUG_ENUMERATE is not set! */
	if (!priv->done_enumerate)
		return 0;

	helper = g_new0(GUsbContextIdleHelper, 1);
	helper->self = g_object_ref(self);
	helper->dev = libusb_ref_device(dev);
	helper->event = event;

	g_ptr_array_add(priv->idle_events, helper);
	if (priv->idle_events_id == 0)
		priv->idle_events_id = g_idle_add(g_usb_context_idle_hotplug_cb, self);

	return 0;
}

static void
g_usb_context_rescan(GUsbContext *self)
{
	GUsbContextPrivate *priv = GET_PRIVATE(self);
	libusb_device **dev_list = NULL;
	g_autoptr(GList) existing_devices = NULL;

	/* copy to a context so we can remove from the array */
	for (guint i = 0; i < priv->devices->len; i++) {
		GUsbDevice *device = g_ptr_array_index(priv->devices, i);
		existing_devices = g_list_prepend(existing_devices, device);
	}

	/* look for any removed devices */
	libusb_get_device_list(priv->ctx, &dev_list);
	for (GList *l = existing_devices; l != NULL; l = l->next) {
		GUsbDevice *device = G_USB_DEVICE(l->data);
		gboolean found = FALSE;
		for (guint i = 0; dev_list != NULL && dev_list[i] != NULL; i++) {
			if (libusb_get_bus_number(dev_list[i]) == g_usb_device_get_bus(device) &&
			    libusb_get_device_address(dev_list[i]) ==
				g_usb_device_get_address(device)) {
				found = TRUE;
				break;
			}
		}
		if (!found) {
			g_usb_context_emit_device_remove(self, device);
			g_ptr_array_remove(priv->devices, device);
		}
	}

	/* add any devices not yet added (duplicates will be filtered */
	for (guint i = 0; dev_list != NULL && dev_list[i] != NULL; i++)
		g_usb_context_add_device(self, dev_list[i]);

	libusb_free_device_list(dev_list, 1);
}

static gboolean
g_usb_context_rescan_cb(gpointer user_data)
{
	GUsbContext *self = G_USB_CONTEXT(user_data);
	g_usb_context_rescan(self);
	return TRUE;
}

/**
 * g_usb_context_get_main_context:
 * @self: a #GUsbContext
 *
 * Gets the internal GMainContext to use for synchronous methods.
 * By default the value is set to the value of g_main_context_default()
 *
 * Return value: (transfer none): the #GMainContext
 *
 * Since: 0.2.5
 **/
GMainContext *
g_usb_context_get_main_context(GUsbContext *self)
{
	GUsbContextPrivate *priv = GET_PRIVATE(self);
	g_return_val_if_fail(G_USB_IS_CONTEXT(self), NULL);
	return priv->main_ctx;
}

/**
 * g_usb_context_set_main_context:
 * @self: a #GUsbContext
 *
 * Sets the internal GMainContext to use for synchronous methods.
 *
 * Since: 0.2.5
 **/
void
g_usb_context_set_main_context(GUsbContext *self, GMainContext *main_ctx)
{
	GUsbContextPrivate *priv = GET_PRIVATE(self);

	g_return_if_fail(G_USB_IS_CONTEXT(self));

	if (main_ctx != priv->main_ctx) {
		g_main_context_unref(priv->main_ctx);
		priv->main_ctx = g_main_context_ref(main_ctx);
	}
}

static void
g_usb_context_ensure_rescan_timeout(GUsbContext *self)
{
	GUsbContextPrivate *priv = GET_PRIVATE(self);

	if (priv->hotplug_poll_id > 0) {
		g_source_remove(priv->hotplug_poll_id);
		priv->hotplug_poll_id = 0;
	}
	if (priv->hotplug_poll_interval > 0) {
		priv->hotplug_poll_id =
		    g_timeout_add(priv->hotplug_poll_interval, g_usb_context_rescan_cb, self);
	}
}

/**
 * g_usb_context_get_hotplug_poll_interval:
 * @self: a #GUsbContext
 *
 * Gets the poll interval for platforms like Windows that do not support `LIBUSB_CAP_HAS_HOTPLUG`.
 *
 * Return value: interval in ms
 *
 * Since: 0.3.10
 **/
guint
g_usb_context_get_hotplug_poll_interval(GUsbContext *self)
{
	GUsbContextPrivate *priv = GET_PRIVATE(self);
	g_return_val_if_fail(G_USB_IS_CONTEXT(self), G_MAXUINT);
	return priv->hotplug_poll_id;
}

/**
 * g_usb_context_set_hotplug_poll_interval:
 * @self: a #GUsbContext
 * @hotplug_poll_interval: the interval in ms
 *
 * Sets the poll interval for platforms like Windows that do not support `LIBUSB_CAP_HAS_HOTPLUG`.
 * This defaults to 1000ms and can be changed before or after g_usb_context_enumerate() has been
 * called.
 *
 * Since: 0.3.10
 **/
void
g_usb_context_set_hotplug_poll_interval(GUsbContext *self, guint hotplug_poll_interval)
{
	GUsbContextPrivate *priv = GET_PRIVATE(self);

	g_return_if_fail(G_USB_IS_CONTEXT(self));

	/* same */
	if (priv->hotplug_poll_interval == hotplug_poll_interval)
		return;

	priv->hotplug_poll_interval = hotplug_poll_interval;

	/* if already running then change the existing timeout */
	if (priv->hotplug_poll_id > 0)
		g_usb_context_ensure_rescan_timeout(self);
}

/**
 * g_usb_context_enumerate:
 * @self: a #GUsbContext
 *
 * Enumerates all the USB devices and adds them to the context.
 *
 * You only need to call this function once, and any subsequent calls
 * are silently ignored.
 *
 * Since: 0.2.2
 **/
void
g_usb_context_enumerate(GUsbContext *self)
{
	GUsbContextPrivate *priv = GET_PRIVATE(self);

	/* only ever initially scan once, then rely on hotplug / poll */
	if (priv->done_enumerate)
		return;

	g_usb_context_rescan(self);
	if (!libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
		g_debug("platform does not do hotplug, using polling");
		g_usb_context_ensure_rescan_timeout(self);
	}
	priv->done_enumerate = TRUE;

	/* emit device-added signals before returning */
	for (guint i = 0; i < priv->devices->len; i++) {
		g_signal_emit(self,
			      signals[DEVICE_ADDED_SIGNAL],
			      0,
			      g_ptr_array_index(priv->devices, i));
	}

	/* any queued up hotplug events are queued as idle handlers */
}

/**
 * g_usb_context_set_flags:
 * @self: a #GUsbContext
 * @flags: some #GUsbContextFlags, e.g. %G_USB_CONTEXT_FLAGS_AUTO_OPEN_DEVICES
 *
 * Sets the flags to use for the context. These should be set before
 * g_usb_context_enumerate() is called.
 *
 * Since: 0.2.11
 **/
void
g_usb_context_set_flags(GUsbContext *self, GUsbContextFlags flags)
{
	GUsbContextPrivate *priv = GET_PRIVATE(self);
	priv->flags = flags;
}

/**
 * g_usb_context_get_flags:
 * @self: a #GUsbContext
 *
 * Sets the flags to use for the context.
 *
 * Return value: the #GUsbContextFlags, e.g. %G_USB_CONTEXT_FLAGS_AUTO_OPEN_DEVICES
 *
 * Since: 0.2.11
 **/
GUsbContextFlags
g_usb_context_get_flags(GUsbContext *self)
{
	GUsbContextPrivate *priv = GET_PRIVATE(self);
	return priv->flags;
}

gboolean
_g_usb_context_has_flag(GUsbContext *self, GUsbContextFlags flag)
{
	GUsbContextPrivate *priv = GET_PRIVATE(self);
	return (priv->flags & flag) > 0;
}

static gpointer
g_usb_context_event_thread_cb(gpointer data)
{
	GUsbContext *self = G_USB_CONTEXT(data);
	GUsbContextPrivate *priv = GET_PRIVATE(self);
	struct timeval tv = {
	    .tv_usec = 0,
	    .tv_sec = 2,
	};

	while (g_atomic_int_get(&priv->thread_event_run) > 0)
		libusb_handle_events_timeout_completed(priv->ctx, &tv, NULL);

	return NULL;
}

static void
g_usb_context_init(GUsbContext *self)
{
	GUsbContextPrivate *priv = GET_PRIVATE(self);

	priv->flags = G_USB_CONTEXT_FLAGS_NONE;
	priv->hotplug_poll_interval = G_USB_CONTEXT_HOTPLUG_POLL_INTERVAL_DEFAULT;
	priv->devices = g_ptr_array_new_with_free_func((GDestroyNotify)g_object_unref);
	priv->devices_removed = g_ptr_array_new_with_free_func((GDestroyNotify)g_object_unref);
	priv->dict_usb_ids = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	priv->dict_replug = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

	/* to escape the thread into the mainloop */
	g_mutex_init(&priv->idle_events_mutex);
	priv->idle_events =
	    g_ptr_array_new_with_free_func((GDestroyNotify)g_usb_context_idle_helper_free);
}

static gboolean
g_usb_context_initable_init(GInitable *initable, GCancellable *cancellable, GError **error)
{
	GUsbContext *self = G_USB_CONTEXT(initable);
	GUsbContextPrivate *priv = GET_PRIVATE(self);
	gint rc;
	libusb_context *ctx;

	rc = libusb_init(&ctx);
	if (rc < 0) {
		g_set_error(error,
			    G_USB_CONTEXT_ERROR,
			    G_USB_CONTEXT_ERROR_INTERNAL,
			    "failed to init libusb: %s [%i]",
			    g_usb_strerror(rc),
			    rc);
		return FALSE;
	}

	priv->main_ctx = g_main_context_ref(g_main_context_default());
	priv->ctx = ctx;
	priv->thread_event_run = 1;
	priv->thread_event = g_thread_new("GUsbEventThread", g_usb_context_event_thread_cb, self);

	/* watch for add/remove */
	if (libusb_has_capability(LIBUSB_CAP_HAS_HOTPLUG)) {
		rc = libusb_hotplug_register_callback(priv->ctx,
						      LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED |
							  LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT,
						      0,
						      LIBUSB_HOTPLUG_MATCH_ANY,
						      LIBUSB_HOTPLUG_MATCH_ANY,
						      LIBUSB_HOTPLUG_MATCH_ANY,
						      g_usb_context_hotplug_cb,
						      self,
						      &priv->hotplug_id);
		if (rc != LIBUSB_SUCCESS) {
			g_warning("Error creating a hotplug callback: %s", g_usb_strerror(rc));
		}
	}

	return TRUE;
}

static void
g_usb_context_initable_iface_init(GInitableIface *iface)
{
	iface->init = g_usb_context_initable_init;
}

/**
 * _g_usb_context_get_context:
 * @self: a #GUsbContext
 *
 * Gets the internal libusb_context.
 *
 * Return value: (transfer none): the libusb_context
 *
 * Since: 0.1.0
 **/
libusb_context *
_g_usb_context_get_context(GUsbContext *self)
{
	GUsbContextPrivate *priv = GET_PRIVATE(self);
	return priv->ctx;
}

/**
 * g_usb_context_get_source:
 * @self: a #GUsbContext
 * @main_ctx: a #GMainContext, or %NULL
 *
 * This function does nothing.
 *
 * Return value: (transfer none): the #GUsbSource.
 *
 * Since: 0.1.0
 **/
GUsbSource *
g_usb_context_get_source(GUsbContext *self, GMainContext *main_ctx)
{
	return NULL;
}

/**
 * g_usb_context_set_debug:
 * @self: a #GUsbContext
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
g_usb_context_set_debug(GUsbContext *self, GLogLevelFlags flags)
{
	GUsbContextPrivate *priv = GET_PRIVATE(self);
	int debug_level;

	g_return_if_fail(G_USB_IS_CONTEXT(self));

	if (flags & (G_LOG_LEVEL_DEBUG | G_LOG_LEVEL_INFO))
		debug_level = 3;
	else if (flags & (G_LOG_LEVEL_MESSAGE | G_LOG_LEVEL_WARNING))
		debug_level = 2;
	else if (flags & (G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_ERROR))
		debug_level = 1;
	else
		debug_level = 0;

	if (debug_level != priv->debug_level) {
		priv->debug_level = debug_level;
#ifdef HAVE_LIBUSB_SET_OPTION
		libusb_set_option(priv->ctx, LIBUSB_OPTION_LOG_LEVEL, debug_level);
#else
		libusb_set_debug(priv->ctx, debug_level);
#endif

		g_object_notify_by_pspec(G_OBJECT(self), pspecs[PROP_DEBUG_LEVEL]);
	}
}

/**
 * g_usb_context_find_by_bus_address:
 * @self: a #GUsbContext
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
g_usb_context_find_by_bus_address(GUsbContext *self, guint8 bus, guint8 address, GError **error)
{
	GUsbContextPrivate *priv = GET_PRIVATE(self);

	g_return_val_if_fail(G_USB_IS_CONTEXT(self), NULL);
	g_return_val_if_fail(error == NULL || *error == NULL, NULL);

	g_usb_context_enumerate(self);
	for (guint i = 0; i < priv->devices->len; i++) {
		GUsbDevice *device = g_ptr_array_index(priv->devices, i);
		if (g_usb_device_get_bus(device) == bus &&
		    g_usb_device_get_address(device) == address) {
			return g_object_ref(device);
		}
	}
	g_set_error(error,
		    G_USB_DEVICE_ERROR,
		    G_USB_DEVICE_ERROR_NO_DEVICE,
		    "Failed to find device %04x:%04x",
		    bus,
		    address);
	return NULL;
}

/**
 * g_usb_context_find_by_platform_id:
 * @self: a #GUsbContext
 * @platform_id: a platform id, e.g. "usb:00:03:03:02"
 * @error: A #GError or %NULL
 *
 * Finds a device based on its platform id value.
 *
 * Return value: (transfer full): a new #GUsbDevice, or %NULL if not found.
 *
 * Since: 0.2.4
 **/
GUsbDevice *
g_usb_context_find_by_platform_id(GUsbContext *self, const gchar *platform_id, GError **error)
{
	GUsbContextPrivate *priv = GET_PRIVATE(self);

	g_return_val_if_fail(G_USB_IS_CONTEXT(self), NULL);
	g_return_val_if_fail(error == NULL || *error == NULL, NULL);

	g_usb_context_enumerate(self);
	for (guint i = 0; i < priv->devices->len; i++) {
		GUsbDevice *device = g_ptr_array_index(priv->devices, i);
		if (g_strcmp0(g_usb_device_get_platform_id(device), platform_id) == 0)
			return g_object_ref(device);
	}
	g_set_error(error,
		    G_USB_DEVICE_ERROR,
		    G_USB_DEVICE_ERROR_NO_DEVICE,
		    "Failed to find device %s",
		    platform_id);
	return NULL;
}

/**
 * g_usb_context_find_by_vid_pid:
 * @self: a #GUsbContext
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
g_usb_context_find_by_vid_pid(GUsbContext *self, guint16 vid, guint16 pid, GError **error)
{
	GUsbContextPrivate *priv = GET_PRIVATE(self);

	g_return_val_if_fail(G_USB_IS_CONTEXT(self), NULL);
	g_return_val_if_fail(error == NULL || *error == NULL, NULL);

	g_usb_context_enumerate(self);
	for (guint i = 0; i < priv->devices->len; i++) {
		GUsbDevice *device = g_ptr_array_index(priv->devices, i);
		if (g_usb_device_get_vid(device) == vid && g_usb_device_get_pid(device) == pid) {
			return g_object_ref(device);
		}
	}
	g_set_error(error,
		    G_USB_DEVICE_ERROR,
		    G_USB_DEVICE_ERROR_NO_DEVICE,
		    "Failed to find device %04x:%04x",
		    vid,
		    pid);
	return NULL;
}

static gboolean
g_usb_context_load_usb_ids(GUsbContext *self, GError **error)
{
	GUsbContextPrivate *priv = GET_PRIVATE(self);
	guint16 pid;
	guint16 vid = 0x0000;
	g_autofree gchar *data = NULL;
	g_auto(GStrv) lines = NULL;

	/* already loaded */
	if (g_hash_table_size(priv->dict_usb_ids) > 0)
		return TRUE;

	/* parse */
	if (!g_file_get_contents(USB_IDS, &data, NULL, error))
		return FALSE;

	lines = g_strsplit(data, "\n", -1);
	for (guint i = 0; lines[i] != NULL; i++) {
		if (lines[i][0] == '#')
			continue;

		if (lines[i][0] == '\0')
			continue;

		/* the list of known device classes, subclasses and protocols appeared */
		if (g_str_has_prefix(lines[i], "C 00"))
			break;

		if (lines[i][0] != '\t') {
			if (strlen(lines[i]) < 7)
				continue;
			lines[i][4] = '\0';
			vid = g_ascii_strtoull(lines[i], NULL, 16);
			if (vid == 0)
				break;

			g_hash_table_insert(priv->dict_usb_ids,
					    g_strdup(lines[i]),
					    g_strdup(lines[i] + 6));
		} else {
			if (vid == 0x0000)
				break;

			if (strlen(lines[i]) < 8)
				continue;
			lines[i][5] = '\0';
			pid = g_ascii_strtoull(lines[i] + 1, NULL, 16);
			g_hash_table_insert(priv->dict_usb_ids,
					    g_strdup_printf("%04x:%04x", vid, pid),
					    g_strdup(lines[i] + 7));
		}
	}

	return TRUE;
}

/**
 * _g_usb_context_lookup_vendor:
 * @self: a #GUsbContext
 * @vid: a USB vendor ID
 * @error: a #GError, or %NULL
 *
 * Returns the vendor name using usb.ids.
 *
 * Return value: the description, or %NULL
 *
 * Since: 0.1.5
 **/
const gchar *
_g_usb_context_lookup_vendor(GUsbContext *self, guint16 vid, GError **error)
{
	GUsbContextPrivate *priv = GET_PRIVATE(self);
	const gchar *tmp;
	g_autofree gchar *key = NULL;

	g_return_val_if_fail(G_USB_IS_CONTEXT(self), NULL);
	g_return_val_if_fail(error == NULL || *error == NULL, NULL);

	/* load */
	if (!g_usb_context_load_usb_ids(self, error))
		return NULL;

	/* find */
	key = g_strdup_printf("%04x", vid);
	tmp = g_hash_table_lookup(priv->dict_usb_ids, key);
	if (tmp == NULL) {
		g_set_error(error,
			    G_USB_CONTEXT_ERROR,
			    G_USB_CONTEXT_ERROR_INTERNAL,
			    "failed to find vid %s",
			    key);
		return NULL;
	}

	return tmp;
}

/**
 * _g_usb_context_lookup_product:
 * @self: a #GUsbContext
 * @vid: a USB vendor ID
 * @pid: a USB product ID
 * @error: a #GError, or %NULL
 *
 * Returns the product name using usb.ids.
 *
 * Return value: the description, or %NULL
 *
 * Since: 0.1.5
 **/
const gchar *
_g_usb_context_lookup_product(GUsbContext *self, guint16 vid, guint16 pid, GError **error)
{
	GUsbContextPrivate *priv = GET_PRIVATE(self);
	const gchar *tmp;
	g_autofree gchar *key = NULL;

	g_return_val_if_fail(G_USB_IS_CONTEXT(self), NULL);
	g_return_val_if_fail(error == NULL || *error == NULL, NULL);

	/* load */
	if (!g_usb_context_load_usb_ids(self, error))
		return NULL;

	/* find */
	key = g_strdup_printf("%04x:%04x", vid, pid);
	tmp = g_hash_table_lookup(priv->dict_usb_ids, key);
	if (tmp == NULL) {
		g_set_error(error,
			    G_USB_CONTEXT_ERROR,
			    G_USB_CONTEXT_ERROR_INTERNAL,
			    "failed to find vid %s",
			    key);
		return NULL;
	}

	return tmp;
}

/**
 * g_usb_context_get_devices:
 * @self: a #GUsbContext
 *
 * Return value: (transfer full) (element-type GUsbDevice): a new #GPtrArray of #GUsbDevice's.
 *
 * Since: 0.2.2
 **/
GPtrArray *
g_usb_context_get_devices(GUsbContext *self)
{
	GUsbContextPrivate *priv = GET_PRIVATE(self);

	g_return_val_if_fail(G_USB_IS_CONTEXT(self), NULL);

	g_usb_context_enumerate(self);

	return g_ptr_array_ref(priv->devices);
}

static gboolean
g_usb_context_replug_timeout_cb(gpointer user_data)
{
	GUsbContextReplugHelper *replug_helper = (GUsbContextReplugHelper *)user_data;
	replug_helper->timeout_id = 0;
	g_main_loop_quit(replug_helper->loop);
	return FALSE;
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(GUsbContextReplugHelper, g_usb_context_replug_helper_free)

/**
 * g_usb_context_wait_for_replug:
 * @self: a #GUsbContext
 * @device: a #GUsbDevice
 * @timeout_ms: timeout to wait
 * @error: A #GError or %NULL
 *
 * Waits for the device to be replugged.
 * It may come back with a different VID:PID.
 *
 * Warning: This is synchronous and blocks until the device comes
 * back or the timeout triggers.
 *
 * Return value: (transfer full): a new #GUsbDevice, or %NULL for invalid
 *
 * Since: 0.2.9
 **/
GUsbDevice *
g_usb_context_wait_for_replug(GUsbContext *self,
			      GUsbDevice *device,
			      guint timeout_ms,
			      GError **error)
{
	GUsbContextPrivate *priv = GET_PRIVATE(self);
	const gchar *platform_id;
	g_autoptr(GUsbContextReplugHelper) replug_helper = NULL;

	g_return_val_if_fail(G_USB_IS_CONTEXT(self), NULL);

	/* create a helper */
	replug_helper = g_new0(GUsbContextReplugHelper, 1);
	replug_helper->device = g_object_ref(device);
	replug_helper->loop = g_main_loop_new(priv->main_ctx, FALSE);
	replug_helper->timeout_id =
	    g_timeout_add(timeout_ms, g_usb_context_replug_timeout_cb, replug_helper);

	/* register */
	platform_id = g_usb_device_get_platform_id(device);
	g_hash_table_insert(priv->dict_replug, g_strdup(platform_id), replug_helper);

	/* wait for timeout, or replug */
	g_main_loop_run(replug_helper->loop);

	/* unregister */
	g_hash_table_remove(priv->dict_replug, platform_id);

	/* so we timed out; emit the removal now */
	if (replug_helper->timeout_id == 0) {
		g_usb_context_emit_device_remove(self, replug_helper->device);
		g_set_error_literal(error,
				    G_USB_CONTEXT_ERROR,
				    G_USB_CONTEXT_ERROR_INTERNAL,
				    "request timed out");
		return NULL;
	}
	return g_object_ref(replug_helper->device);
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
g_usb_context_new(GError **error)
{
	return g_initable_new(G_USB_TYPE_CONTEXT, NULL, error, NULL);
}
