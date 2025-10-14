/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2011 Hans de Goede <hdegoede@redhat.com>
 * Copyright (C) 2011 Debarshi Ray <debarshir@src.gnome.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

/**
 * SECTION:gusb-device
 * @short_description: GLib device integration for libusb
 *
 * This object is a thin glib wrapper around a libusb_device
 */

#include "config.h"

#include <libusb.h>
#include <string.h>

#include "gusb-bos-descriptor-private.h"
#include "gusb-context-private.h"
#include "gusb-device-event-private.h"
#include "gusb-device-private.h"
#include "gusb-interface-private.h"
#include "gusb-json-common.h"
#include "gusb-util.h"

/**
 * GUsbDevicePrivate:
 *
 * Private #GUsbDevice data
 **/
typedef struct {
	gchar *platform_id;
	GUsbContext *context;
	libusb_device *device;
	libusb_device_handle *handle;
	struct libusb_device_descriptor desc;
	gboolean interfaces_valid;
	gboolean bos_descriptors_valid;
	gboolean hid_descriptors_valid;
	GPtrArray *interfaces;	    /* of GUsbInterface */
	GPtrArray *bos_descriptors; /* of GUsbBosDescriptor */
	GPtrArray *hid_descriptors; /* of GBytes */
	GPtrArray *events;	    /* of GUsbDeviceEvent */
	GPtrArray *tags;	    /* of utf-8 */
	guint event_idx;
	GDateTime *created;
} GUsbDevicePrivate;

enum { PROP_0, PROP_LIBUSB_DEVICE, PROP_CONTEXT, PROP_PLATFORM_ID, N_PROPERTIES };

static GParamSpec *pspecs[N_PROPERTIES] = {
    NULL,
};

static void
g_usb_device_initable_iface_init(GInitableIface *iface);

G_DEFINE_TYPE_EXTENDED(GUsbDevice,
		       g_usb_device,
		       G_TYPE_OBJECT,
		       0,
		       G_ADD_PRIVATE(GUsbDevice)
			   G_IMPLEMENT_INTERFACE(G_TYPE_INITABLE,
						 g_usb_device_initable_iface_init));

#define GET_PRIVATE(o) (g_usb_device_get_instance_private(o))

/* clang-format off */
/**
 * g_usb_device_error_quark:
 *
 * Return value: Our personal error quark.
 *
 * Since: 0.1.0
 **/
G_DEFINE_QUARK (g-usb-device-error-quark, g_usb_device_error)
/* clang-format on */

static void
g_usb_device_finalize(GObject *object)
{
	GUsbDevice *self = G_USB_DEVICE(object);
	GUsbDevicePrivate *priv = GET_PRIVATE(self);

	g_free(priv->platform_id);
	g_date_time_unref(priv->created);
	g_ptr_array_unref(priv->interfaces);
	g_ptr_array_unref(priv->bos_descriptors);
	g_ptr_array_unref(priv->hid_descriptors);
	g_ptr_array_unref(priv->events);
	g_ptr_array_unref(priv->tags);

	G_OBJECT_CLASS(g_usb_device_parent_class)->finalize(object);
}

static void
g_usb_device_dispose(GObject *object)
{
	GUsbDevice *self = G_USB_DEVICE(object);
	GUsbDevicePrivate *priv = GET_PRIVATE(self);

	g_clear_pointer(&priv->handle, libusb_close);
	g_clear_pointer(&priv->device, libusb_unref_device);
	g_clear_object(&priv->context);

	G_OBJECT_CLASS(g_usb_device_parent_class)->dispose(object);
}

static void
g_usb_device_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GUsbDevice *self = G_USB_DEVICE(object);
	GUsbDevicePrivate *priv = GET_PRIVATE(self);

	switch (prop_id) {
	case PROP_LIBUSB_DEVICE:
		g_value_set_pointer(value, priv->device);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
set_libusb_device(GUsbDevice *self, struct libusb_device *dev)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);

	g_clear_pointer(&priv->device, libusb_unref_device);

	if (dev != NULL)
		priv->device = libusb_ref_device(dev);
}

static void
g_usb_device_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GUsbDevice *self = G_USB_DEVICE(object);
	GUsbDevicePrivate *priv = GET_PRIVATE(self);

	switch (prop_id) {
	case PROP_LIBUSB_DEVICE:
		set_libusb_device(self, g_value_get_pointer(value));
		break;
	case PROP_CONTEXT:
		priv->context = g_value_dup_object(value);
		break;
	case PROP_PLATFORM_ID:
		priv->platform_id = g_value_dup_string(value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void
g_usb_device_constructed(GObject *object)
{
	GUsbDevice *self = G_USB_DEVICE(object);
	GUsbDevicePrivate *priv = GET_PRIVATE(self);

	if (priv->device != NULL) {
		gint rc = libusb_get_device_descriptor(priv->device, &priv->desc);
		if (rc != LIBUSB_SUCCESS)
			g_warning("Failed to get USB descriptor for device: %s",
				  g_usb_strerror(rc));
	}

	G_OBJECT_CLASS(g_usb_device_parent_class)->constructed(object);
}

static void
g_usb_device_class_init(GUsbDeviceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

	object_class->finalize = g_usb_device_finalize;
	object_class->dispose = g_usb_device_dispose;
	object_class->get_property = g_usb_device_get_property;
	object_class->set_property = g_usb_device_set_property;
	object_class->constructed = g_usb_device_constructed;

	/**
	 * GUsbDevice:libusb_device:
	 */
	pspecs[PROP_LIBUSB_DEVICE] =
	    g_param_spec_pointer("libusb-device",
				 NULL,
				 NULL,
				 G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);

	/**
	 * GUsbDevice:context:
	 */
	pspecs[PROP_CONTEXT] = g_param_spec_object("context",
						   NULL,
						   NULL,
						   G_USB_TYPE_CONTEXT,
						   G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE);

	/**
	 * GUsbDevice:platform-id:
	 */
	pspecs[PROP_PLATFORM_ID] = g_param_spec_string("platform-id",
						       NULL,
						       NULL,
						       NULL,
						       G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE);

	g_object_class_install_properties(object_class, N_PROPERTIES, pspecs);
}

static void
g_usb_device_init(GUsbDevice *self)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	priv->created = g_date_time_new_now_utc();
	priv->interfaces = g_ptr_array_new_with_free_func((GDestroyNotify)g_object_unref);
	priv->bos_descriptors = g_ptr_array_new_with_free_func((GDestroyNotify)g_object_unref);
	priv->hid_descriptors = g_ptr_array_new_with_free_func((GDestroyNotify)g_bytes_unref);
	priv->events = g_ptr_array_new_with_free_func((GDestroyNotify)g_object_unref);
	priv->tags = g_ptr_array_new_with_free_func(g_free);
}

/* private */
void
_g_usb_device_add_event(GUsbDevice *self, GUsbDeviceEvent *event)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	g_return_if_fail(G_USB_IS_DEVICE(self));
	g_return_if_fail(G_USB_IS_DEVICE_EVENT(event));
	g_ptr_array_add(priv->events, g_object_ref(event));
}

gboolean
_g_usb_device_load(GUsbDevice *self, JsonObject *json_object, GError **error)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	const gchar *tmp;

	g_return_val_if_fail(G_USB_IS_DEVICE(self), FALSE);
	g_return_val_if_fail(json_object != NULL, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	/* optional properties */
	tmp = json_object_get_string_member_with_default(json_object, "PlatformId", NULL);
	if (tmp != NULL) {
		g_free(priv->platform_id);
		priv->platform_id = g_strdup(tmp);
	}
	tmp = json_object_get_string_member_with_default(json_object, "Created", NULL);
	if (tmp != NULL) {
		g_autoptr(GDateTime) created_new = g_date_time_new_from_iso8601(tmp, NULL);
		if (created_new == NULL) {
			g_set_error(error,
				    G_IO_ERROR,
				    G_IO_ERROR_INVALID_DATA,
				    "Cannot parse ISO8601 date: %s",
				    tmp);
			return FALSE;
		}
		if (!g_date_time_equal(priv->created, created_new)) {
			g_date_time_unref(priv->created);
			priv->created = g_steal_pointer(&created_new);
		}
	}
	priv->desc.idVendor = json_object_get_int_member_with_default(json_object, "IdVendor", 0x0);
	priv->desc.idProduct =
	    json_object_get_int_member_with_default(json_object, "IdProduct", 0x0);
	priv->desc.bcdDevice = json_object_get_int_member_with_default(json_object, "Device", 0x0);
	priv->desc.bcdUSB = json_object_get_int_member_with_default(json_object, "USB", 0x0);
	priv->desc.iManufacturer =
	    json_object_get_int_member_with_default(json_object, "Manufacturer", 0x0);
	priv->desc.bDeviceClass =
	    json_object_get_int_member_with_default(json_object, "DeviceClass", 0x0);
	priv->desc.bDeviceSubClass =
	    json_object_get_int_member_with_default(json_object, "DeviceSubClass", 0x0);
	priv->desc.bDeviceProtocol =
	    json_object_get_int_member_with_default(json_object, "DeviceProtocol", 0x0);
	priv->desc.iProduct = json_object_get_int_member_with_default(json_object, "Product", 0x0);
	priv->desc.iSerialNumber =
	    json_object_get_int_member_with_default(json_object, "SerialNumber", 0x0);

	/* array of BOS descriptors */
	if (json_object_has_member(json_object, "UsbBosDescriptors")) {
		JsonArray *json_array =
		    json_object_get_array_member(json_object, "UsbBosDescriptors");
		for (guint i = 0; i < json_array_get_length(json_array); i++) {
			JsonNode *node_tmp = json_array_get_element(json_array, i);
			JsonObject *obj_tmp = json_node_get_object(node_tmp);
			g_autoptr(GUsbBosDescriptor) bos_descriptor =
			    g_object_new(G_USB_TYPE_BOS_DESCRIPTOR, NULL);
			if (!_g_usb_bos_descriptor_load(bos_descriptor, obj_tmp, error))
				return FALSE;
			g_ptr_array_add(priv->bos_descriptors, g_object_ref(bos_descriptor));
		}
	}

	/* array of HID descriptors */
	if (json_object_has_member(json_object, "UsbHidDescriptors")) {
		JsonArray *json_array =
		    json_object_get_array_member(json_object, "UsbHidDescriptors");
		for (guint i = 0; i < json_array_get_length(json_array); i++) {
			JsonNode *node_tmp = json_array_get_element(json_array, i);
			tmp = json_node_get_string(node_tmp);
			if (tmp != NULL) {
				gsize bufsz = 0;
				g_autofree guchar *buf = g_base64_decode(tmp, &bufsz);
				g_ptr_array_add(priv->hid_descriptors,
						g_bytes_new_take(g_steal_pointer(&buf), bufsz));
			}
		}
	}

	/* array of interfaces */
	if (json_object_has_member(json_object, "UsbInterfaces")) {
		JsonArray *json_array = json_object_get_array_member(json_object, "UsbInterfaces");
		for (guint i = 0; i < json_array_get_length(json_array); i++) {
			JsonNode *node_tmp = json_array_get_element(json_array, i);
			JsonObject *obj_tmp = json_node_get_object(node_tmp);
			g_autoptr(GUsbInterface) iface =
			    g_object_new(G_USB_TYPE_INTERFACE, NULL);
			if (!_g_usb_interface_load(iface, obj_tmp, error))
				return FALSE;
			g_ptr_array_add(priv->interfaces, g_object_ref(iface));
		}
	}

	/* array of events */
	if (json_object_has_member(json_object, "UsbEvents")) {
		JsonArray *json_array = json_object_get_array_member(json_object, "UsbEvents");
		for (guint i = 0; i < json_array_get_length(json_array); i++) {
			JsonNode *node_tmp = json_array_get_element(json_array, i);
			JsonObject *obj_tmp = json_node_get_object(node_tmp);
			g_autoptr(GUsbDeviceEvent) event = _g_usb_device_event_new(NULL);
			if (!_g_usb_device_event_load(event, obj_tmp, error))
				return FALSE;
			g_ptr_array_add(priv->events, g_steal_pointer(&event));
		}
	}

	/* array of tags */
	if (json_object_has_member(json_object, "Tags")) {
		JsonArray *json_array = json_object_get_array_member(json_object, "Tags");
		for (guint i = 0; i < json_array_get_length(json_array); i++) {
			JsonNode *node_tmp = json_array_get_element(json_array, i);
			const gchar *str = json_node_get_string(node_tmp);
			if (str != NULL && str[0] != '\0')
				g_ptr_array_add(priv->tags, g_strdup(str));
		}
	}

	/* success */
	priv->interfaces_valid = TRUE;
	priv->bos_descriptors_valid = TRUE;
	priv->hid_descriptors_valid = TRUE;
	priv->event_idx = 0;
	return TRUE;
}

gboolean
_g_usb_device_save(GUsbDevice *self, JsonBuilder *json_builder, GError **error)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	g_autoptr(GPtrArray) bos_descriptors = NULL;
	g_autoptr(GPtrArray) hid_descriptors = NULL;
	g_autoptr(GPtrArray) interfaces = NULL;
	g_autoptr(GError) error_bos = NULL;
	g_autoptr(GError) error_hid = NULL;
	g_autoptr(GError) error_interfaces = NULL;

	g_return_val_if_fail(G_USB_IS_DEVICE(self), FALSE);
	g_return_val_if_fail(json_builder != NULL, FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	/* start */
	json_builder_begin_object(json_builder);

	/* optional properties */
	if (priv->platform_id != NULL) {
		json_builder_set_member_name(json_builder, "PlatformId");
		json_builder_add_string_value(json_builder, priv->platform_id);
	}
#if GLIB_CHECK_VERSION(2, 62, 0)
	if (priv->created != NULL) {
		g_autofree gchar *str = g_date_time_format_iso8601(priv->created);
		json_builder_set_member_name(json_builder, "Created");
		json_builder_add_string_value(json_builder, str);
	}
#endif
	if (priv->tags->len > 0) {
		json_builder_set_member_name(json_builder, "Tags");
		json_builder_begin_array(json_builder);
		for (guint i = 0; i < priv->tags->len; i++) {
			const gchar *tag = g_ptr_array_index(priv->tags, i);
			json_builder_add_string_value(json_builder, tag);
		}
		json_builder_end_array(json_builder);
	}
	if (priv->desc.idVendor != 0) {
		json_builder_set_member_name(json_builder, "IdVendor");
		json_builder_add_int_value(json_builder, priv->desc.idVendor);
	}
	if (priv->desc.idProduct != 0) {
		json_builder_set_member_name(json_builder, "IdProduct");
		json_builder_add_int_value(json_builder, priv->desc.idProduct);
	}
	if (priv->desc.bcdDevice != 0) {
		json_builder_set_member_name(json_builder, "Device");
		json_builder_add_int_value(json_builder, priv->desc.bcdDevice);
	}
	if (priv->desc.bcdUSB != 0) {
		json_builder_set_member_name(json_builder, "USB");
		json_builder_add_int_value(json_builder, priv->desc.bcdUSB);
	}
	if (priv->desc.iManufacturer != 0) {
		json_builder_set_member_name(json_builder, "Manufacturer");
		json_builder_add_int_value(json_builder, priv->desc.iManufacturer);
	}
	if (priv->desc.bDeviceClass != 0) {
		json_builder_set_member_name(json_builder, "DeviceClass");
		json_builder_add_int_value(json_builder, priv->desc.bDeviceClass);
	}
	if (priv->desc.bDeviceSubClass != 0) {
		json_builder_set_member_name(json_builder, "DeviceSubClass");
		json_builder_add_int_value(json_builder, priv->desc.bDeviceSubClass);
	}
	if (priv->desc.bDeviceProtocol != 0) {
		json_builder_set_member_name(json_builder, "DeviceProtocol");
		json_builder_add_int_value(json_builder, priv->desc.bDeviceProtocol);
	}
	if (priv->desc.iProduct != 0) {
		json_builder_set_member_name(json_builder, "Product");
		json_builder_add_int_value(json_builder, priv->desc.iProduct);
	}
	if (priv->desc.iSerialNumber != 0) {
		json_builder_set_member_name(json_builder, "SerialNumber");
		json_builder_add_int_value(json_builder, priv->desc.iSerialNumber);
	}

	/* array of BOS descriptors */
	bos_descriptors = g_usb_device_get_bos_descriptors(self, &error_bos);
	if (bos_descriptors == NULL) {
		if (_g_usb_context_has_flag(priv->context, G_USB_CONTEXT_FLAGS_DEBUG))
			g_debug("%s", error_bos->message);
	} else if (bos_descriptors->len > 0) {
		json_builder_set_member_name(json_builder, "UsbBosDescriptors");
		json_builder_begin_array(json_builder);
		for (guint i = 0; i < bos_descriptors->len; i++) {
			GUsbBosDescriptor *bos_descriptor = g_ptr_array_index(bos_descriptors, i);
			if (!_g_usb_bos_descriptor_save(bos_descriptor, json_builder, error))
				return FALSE;
		}
		json_builder_end_array(json_builder);
	}

	/* array of HID descriptors */
	hid_descriptors = g_usb_device_get_hid_descriptors(self, &error_hid);
	if (hid_descriptors == NULL) {
		if (_g_usb_context_has_flag(priv->context, G_USB_CONTEXT_FLAGS_DEBUG))
			g_debug("%s", error_hid->message);
	} else if (hid_descriptors->len > 0) {
		json_builder_set_member_name(json_builder, "UsbHidDescriptors");
		json_builder_begin_array(json_builder);
		for (guint i = 0; i < hid_descriptors->len; i++) {
			GBytes *bytes = g_ptr_array_index(hid_descriptors, i);
			g_autofree gchar *str =
			    g_base64_encode(g_bytes_get_data(bytes, NULL), g_bytes_get_size(bytes));
			json_builder_add_string_value(json_builder, str);
		}
		json_builder_end_array(json_builder);
	}

	/* array of interfaces */
	interfaces = g_usb_device_get_interfaces(self, &error_interfaces);
	if (interfaces == NULL) {
		if (_g_usb_context_has_flag(priv->context, G_USB_CONTEXT_FLAGS_DEBUG))
			g_debug("%s", error_interfaces->message);
	} else if (interfaces->len > 0) {
		json_builder_set_member_name(json_builder, "UsbInterfaces");
		json_builder_begin_array(json_builder);
		for (guint i = 0; i < interfaces->len; i++) {
			GUsbInterface *iface = g_ptr_array_index(interfaces, i);
			if (!_g_usb_interface_save(iface, json_builder, error))
				return FALSE;
		}
		json_builder_end_array(json_builder);
	}

	/* events */
	if (priv->events->len > 0) {
		json_builder_set_member_name(json_builder, "UsbEvents");
		json_builder_begin_array(json_builder);
		for (guint i = 0; i < priv->events->len; i++) {
			GUsbDeviceEvent *event = g_ptr_array_index(priv->events, i);
			if (!_g_usb_device_event_save(event, json_builder, error))
				return FALSE;
		}
		json_builder_end_array(json_builder);
	}

	/* success */
	json_builder_end_object(json_builder);
	return TRUE;
}

/**
 * g_usb_device_get_created:
 * @self: a #GUsbDevice
 *
 * Gets the date and time that the #GUsbDevice was created.
 *
 * This can be used as an indicator if the device replugged, as the vendor and product IDs may not
 * change for some devices. Use `g_date_time_equal()` to verify equality.
 *
 * Returns: (transfer none): a #GDateTime
 *
 * Since: 0.4.5
 **/
GDateTime *
g_usb_device_get_created(GUsbDevice *self)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	g_return_val_if_fail(G_USB_IS_DEVICE(self), NULL);
	return priv->created;
}

/**
 * g_usb_device_get_tags:
 * @self: a #GUsbDevice
 *
 * Gets all the tags.
 *
 * Returns: (transfer container) (element-type utf8): string tags
 *
 * Since: 0.4.4
 **/
GPtrArray *
g_usb_device_get_tags(GUsbDevice *self)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	g_return_val_if_fail(G_USB_IS_DEVICE(self), NULL);
	return g_ptr_array_ref(priv->tags);
}

/**
 * g_usb_device_has_tag:
 * @self: a #GUsbDevice
 * @tag: a tag, for example `bootloader` or `runtime-reload`
 *
 * Checks if a tag has been used to identify the specific device.
 *
 * Return value: %TRUE on success
 *
 * Since: 0.4.3
 **/
gboolean
g_usb_device_has_tag(GUsbDevice *self, const gchar *tag)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);

	g_return_val_if_fail(G_USB_IS_DEVICE(self), FALSE);
	g_return_val_if_fail(tag != NULL, FALSE);

	for (guint i = 0; i < priv->tags->len; i++) {
		const gchar *tag_tmp = g_ptr_array_index(priv->tags, i);
		if (g_strcmp0(tag_tmp, tag) == 0)
			return TRUE;
	}
	return FALSE;
}

/**
 * g_usb_device_add_tag:
 * @self: a #GUsbDevice
 * @tag: a tag, for example `bootloader` or `runtime-reload`
 *
 * Adds a tag, which is included in the JSON log to identify the specific device.
 *
 * For instance, there might be a pre-update runtime, a bootloader and a post-update runtime
 * and allowing tags to be saved to the backend object allows us to identify each version of
 * the same physical device.
 *
 * Since: 0.4.1
 **/
void
g_usb_device_add_tag(GUsbDevice *self, const gchar *tag)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);

	g_return_if_fail(G_USB_IS_DEVICE(self));
	g_return_if_fail(tag != NULL);

	if (g_usb_device_has_tag(self, tag))
		return;
	g_ptr_array_add(priv->tags, g_strdup(tag));
}

/**
 * g_usb_device_remove_tag:
 * @self: a #GUsbDevice
 * @tag: a tag, for example `bootloader` or `runtime-reload`
 *
 * Removes a tag, which is included in the JSON log to identify the specific device.
 *
 * Since: 0.4.4
 **/
void
g_usb_device_remove_tag(GUsbDevice *self, const gchar *tag)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);

	g_return_if_fail(G_USB_IS_DEVICE(self));
	g_return_if_fail(tag != NULL);

	for (guint i = 0; i < priv->tags->len; i++) {
		const gchar *tag_tmp = g_ptr_array_index(priv->tags, i);
		if (g_strcmp0(tag_tmp, tag) == 0) {
			g_ptr_array_remove_index(priv->tags, i);
			return;
		}
	}
}

/* not defined in FreeBSD */
#ifndef HAVE_LIBUSB_GET_PARENT
static libusb_device *
libusb_get_parent(libusb_device *dev)
{
	return NULL;
}
#endif

/* not defined in DragonFlyBSD */
#ifndef HAVE_LIBUSB_GET_PORT_NUMBER
static guint8
libusb_get_port_number(libusb_device *dev)
{
	return 0xff;
}
#endif

static void
g_usb_device_build_parent_port_number(GString *str, libusb_device *dev)
{
	libusb_device *parent = libusb_get_parent(dev);
	if (parent != NULL)
		g_usb_device_build_parent_port_number(str, parent);
	g_string_append_printf(str, "%02x:", libusb_get_port_number(dev));
}

static gchar *
g_usb_device_build_platform_id(struct libusb_device *dev)
{
	GString *platform_id;

	/* build a topology of the device */
	platform_id = g_string_new("usb:");
	g_string_append_printf(platform_id, "%02x:", libusb_get_bus_number(dev));
	g_usb_device_build_parent_port_number(platform_id, dev);
	g_string_truncate(platform_id, platform_id->len - 1);
	return g_string_free(platform_id, FALSE);
}

static gboolean
g_usb_device_initable_init(GInitable *initable, GCancellable *cancellable, GError **error)
{
	GUsbDevice *self = G_USB_DEVICE(initable);
	GUsbDevicePrivate *priv;
	gint rc;

	priv = GET_PRIVATE(self);

	if (priv->device == NULL) {
		g_set_error_literal(error,
				    G_USB_DEVICE_ERROR,
				    G_USB_DEVICE_ERROR_INTERNAL,
				    "Constructed without a libusb_device");
		return FALSE;
	}

	rc = libusb_get_device_descriptor(priv->device, &priv->desc);
	if (rc != LIBUSB_SUCCESS) {
		g_set_error(error,
			    G_USB_DEVICE_ERROR,
			    G_USB_DEVICE_ERROR_INTERNAL,
			    "Failed to get USB descriptor for device: %s",
			    g_usb_strerror(rc));
		return FALSE;
	}

	/* this does not change on plug->unplug->plug */
	priv->platform_id = g_usb_device_build_platform_id(priv->device);

	return TRUE;
}

static void
g_usb_device_initable_iface_init(GInitableIface *iface)
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
_g_usb_device_new(GUsbContext *context, libusb_device *device, GError **error)
{
	return g_initable_new(G_USB_TYPE_DEVICE,
			      NULL,
			      error,
			      "context",
			      context,
			      "libusb-device",
			      device,
			      NULL);
}

/**
 * _g_usb_device_get_device:
 * @self: a #GUsbDevice instance
 *
 * Gets the low-level libusb_device
 *
 * Return value: The #libusb_device or %NULL. Do not unref this value.
 **/
libusb_device *
_g_usb_device_get_device(GUsbDevice *self)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	return priv->device;
}

/**
 * g_usb_device_is_emulated:
 * @self: a #GUsbDevice instance
 *
 * Gets if the device is emulated.
 *
 * Return value: %TRUE if the device is emulated and not backed by a physical device.
 *
 * Since: 0.4.4
 **/
gboolean
g_usb_device_is_emulated(GUsbDevice *self)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	return priv->device == NULL;
}

static gboolean
g_usb_device_libusb_error_to_gerror(GUsbDevice *self, gint rc, GError **error)
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
		error_code = G_USB_DEVICE_ERROR_NO_DEVICE;
		break;
	case LIBUSB_ERROR_BUSY:
		error_code = G_USB_DEVICE_ERROR_BUSY;
		break;
	default:
		break;
	}

	g_set_error(error,
		    G_USB_DEVICE_ERROR,
		    error_code,
		    "USB error on device %04x:%04x : %s [%i]",
		    g_usb_device_get_vid(self),
		    g_usb_device_get_pid(self),
		    g_usb_strerror(rc),
		    rc);

	return FALSE;
}

static gboolean
g_usb_device_not_open_error(GUsbDevice *self, GError **error)
{
	g_set_error(error,
		    G_USB_DEVICE_ERROR,
		    G_USB_DEVICE_ERROR_NOT_OPEN,
		    "Device %04x:%04x has not been opened",
		    g_usb_device_get_vid(self),
		    g_usb_device_get_pid(self));
	return FALSE;
}

static void
g_usb_device_async_not_open_error(GUsbDevice *self,
				  GAsyncReadyCallback callback,
				  gpointer user_data,
				  gpointer source_tag)
{
	g_task_report_new_error(self,
				callback,
				user_data,
				source_tag,
				G_USB_DEVICE_ERROR,
				G_USB_DEVICE_ERROR_NOT_OPEN,
				"Device %04x:%04x has not been opened",
				g_usb_device_get_vid(self),
				g_usb_device_get_pid(self));
}

gboolean
_g_usb_device_open_internal(GUsbDevice *self, GError **error)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	gint rc;

	/* sanity check */
	if (priv->device == NULL)
		return TRUE;

	if (priv->handle != NULL) {
		g_set_error(error,
			    G_USB_DEVICE_ERROR,
			    G_USB_DEVICE_ERROR_ALREADY_OPEN,
			    "Device %04x:%04x is already open",
			    g_usb_device_get_vid(self),
			    g_usb_device_get_pid(self));
		return FALSE;
	}

	/* open device */
	rc = libusb_open(priv->device, &priv->handle);
	if (!g_usb_device_libusb_error_to_gerror(self, rc, error)) {
		if (priv->handle != NULL)
			libusb_close(priv->handle);
		priv->handle = NULL;
		return FALSE;
	}

	/* success */
	return TRUE;
}

/**
 * g_usb_device_open:
 * @self: a #GUsbDevice
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
g_usb_device_open(GUsbDevice *self, GError **error)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);

	g_return_val_if_fail(G_USB_IS_DEVICE(self), FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	/* emulated */
	if (priv->device == NULL)
		return TRUE;

	/* ignore */
	if (g_usb_context_get_flags(priv->context) & G_USB_CONTEXT_FLAGS_AUTO_OPEN_DEVICES)
		return TRUE;

	/* open */
	return _g_usb_device_open_internal(self, error);
}

/* transfer none */
static GUsbDeviceEvent *
g_usb_device_load_event(GUsbDevice *self, const gchar *id)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);

	/* reset back to the beginning */
	if (priv->event_idx >= priv->events->len) {
		if (_g_usb_context_has_flag(priv->context, G_USB_CONTEXT_FLAGS_DEBUG))
			g_debug("resetting event index");
		priv->event_idx = 0;
	}

	/* look for the next event in the sequence */
	for (guint i = priv->event_idx; i < priv->events->len; i++) {
		GUsbDeviceEvent *event = g_ptr_array_index(priv->events, i);
		if (g_strcmp0(g_usb_device_event_get_id(event), id) == 0) {
			if (_g_usb_context_has_flag(priv->context, G_USB_CONTEXT_FLAGS_DEBUG))
				g_debug("found in-order %s at position %u", id, i);
			priv->event_idx = i + 1;
			return event;
		}
	}

	/* look for *any* event that matches */
	for (guint i = 0; i < priv->events->len; i++) {
		GUsbDeviceEvent *event = g_ptr_array_index(priv->events, i);
		if (g_strcmp0(g_usb_device_event_get_id(event), id) == 0) {
			if (_g_usb_context_has_flag(priv->context, G_USB_CONTEXT_FLAGS_DEBUG))
				g_debug("found out-of-order %s at position %u", id, i);
			priv->event_idx = i + 1;
			return event;
		}
	}

	/* nothing found */
	return NULL;
}

/* transfer none */
static GUsbDeviceEvent *
g_usb_device_save_event(GUsbDevice *self, const gchar *id)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	GUsbDeviceEvent *event;

	g_return_val_if_fail(G_USB_IS_DEVICE(self), NULL);
	g_return_val_if_fail(id != NULL, NULL);

	/* success */
	event = _g_usb_device_event_new(id);
	g_ptr_array_add(priv->events, event);
	return event;
}

/**
 * g_usb_device_get_custom_index:
 * @self: a #GUsbDevice
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
g_usb_device_get_custom_index(GUsbDevice *self,
			      guint8 class_id,
			      guint8 subclass_id,
			      guint8 protocol_id,
			      GError **error)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	GUsbDeviceEvent *event;
	const struct libusb_interface_descriptor *ifp;
	gint rc;
	guint8 idx = 0x00;
	struct libusb_config_descriptor *config;
	g_autofree gchar *event_id = NULL;

	/* build event key either for load or save */
	if (priv->device == NULL ||
	    g_usb_context_get_flags(priv->context) & G_USB_CONTEXT_FLAGS_SAVE_EVENTS) {
		event_id = g_strdup_printf(
		    "GetCustomIndex:ClassId=0x%02x,SubclassId=0x%02x,ProtocolId=0x%02x",
		    class_id,
		    subclass_id,
		    protocol_id);
	}

	/* emulated */
	if (priv->device == NULL) {
		GBytes *bytes;
		event = g_usb_device_load_event(self, event_id);
		if (event == NULL) {
			g_set_error(error,
				    G_IO_ERROR,
				    G_IO_ERROR_INVALID_DATA,
				    "no matching event for %s",
				    event_id);
			return 0x00;
		}
		if (!g_usb_device_libusb_error_to_gerror(self,
							 g_usb_device_event_get_rc(event),
							 error))
			return 0x00;
		bytes = g_usb_device_event_get_bytes(event);
		if (bytes == NULL || g_bytes_get_size(bytes) != 1) {
			g_set_error(error,
				    G_IO_ERROR,
				    G_IO_ERROR_INVALID_DATA,
				    "no matching event data for %s",
				    event_id);
			return 0x00;
		}
		return ((const guint8 *)g_bytes_get_data(bytes, NULL))[0];
	}

	rc = libusb_get_active_config_descriptor(priv->device, &config);
	if (!g_usb_device_libusb_error_to_gerror(self, rc, error))
		return 0x00;

	/* find the right data */
	for (guint i = 0; i < config->bNumInterfaces; i++) {
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
		g_set_error(error,
			    G_USB_DEVICE_ERROR,
			    G_USB_DEVICE_ERROR_NOT_SUPPORTED,
			    "no vendor descriptor for class 0x%02x, "
			    "subclass 0x%02x and protocol 0x%02x",
			    class_id,
			    subclass_id,
			    protocol_id);

	} else if (g_usb_context_get_flags(priv->context) & G_USB_CONTEXT_FLAGS_SAVE_EVENTS) {
		/* save */
		event = g_usb_device_save_event(self, event_id);
		_g_usb_device_event_set_bytes_raw(event, &idx, sizeof(idx));
	}

	libusb_free_config_descriptor(config);
	return idx;
}

/**
 * g_usb_device_get_interface:
 * @self: a #GUsbDevice
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
g_usb_device_get_interface(GUsbDevice *self,
			   guint8 class_id,
			   guint8 subclass_id,
			   guint8 protocol_id,
			   GError **error)
{
	g_autoptr(GPtrArray) interfaces = NULL;

	g_return_val_if_fail(G_USB_IS_DEVICE(self), NULL);
	g_return_val_if_fail(error == NULL || *error == NULL, NULL);

	/* find the right data */
	interfaces = g_usb_device_get_interfaces(self, error);
	if (interfaces == NULL)
		return NULL;
	for (guint i = 0; i < interfaces->len; i++) {
		GUsbInterface *iface = g_ptr_array_index(interfaces, i);
		if (g_usb_interface_get_class(iface) != class_id)
			continue;
		if (g_usb_interface_get_subclass(iface) != subclass_id)
			continue;
		if (g_usb_interface_get_protocol(iface) != protocol_id)
			continue;
		return g_object_ref(iface);
	}

	/* nothing matched */
	g_set_error(error,
		    G_USB_DEVICE_ERROR,
		    G_USB_DEVICE_ERROR_NOT_SUPPORTED,
		    "no interface for class 0x%02x, "
		    "subclass 0x%02x and protocol 0x%02x",
		    class_id,
		    subclass_id,
		    protocol_id);
	return NULL;
}

/**
 * g_usb_device_get_interfaces:
 * @self: a #GUsbDevice
 * @error: a #GError, or %NULL
 *
 * Gets all the interfaces exported by the device.
 *
 * The first time this method is used the hardware is queried and then after that cached results
 * are returned. To invalidate the caches use g_usb_device_invalidate().
 *
 * Return value: (transfer container) (element-type GUsbInterface): an array of interfaces or %NULL
 *for error
 *
 * Since: 0.2.8
 **/
GPtrArray *
g_usb_device_get_interfaces(GUsbDevice *self, GError **error)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);

	g_return_val_if_fail(G_USB_IS_DEVICE(self), NULL);
	g_return_val_if_fail(error == NULL || *error == NULL, NULL);

	/* get all interfaces */
	if (!priv->interfaces_valid) {
		gint rc;
		struct libusb_config_descriptor *config;

		/* sanity check */
		if (priv->device == NULL) {
			g_set_error_literal(error,
					    G_IO_ERROR,
					    G_IO_ERROR_NOT_SUPPORTED,
					    "not supported for emulated device");
			return NULL;
		}

		rc = libusb_get_active_config_descriptor(priv->device, &config);
		if (!g_usb_device_libusb_error_to_gerror(self, rc, error))
			return NULL;

		for (guint i = 0; i < config->bNumInterfaces; i++) {
			for (guint j = 0; j < (guint)config->interface[i].num_altsetting; j++) {
				const struct libusb_interface_descriptor *ifp =
				    &config->interface[i].altsetting[j];
				GUsbInterface *iface = _g_usb_interface_new(ifp);
				g_ptr_array_add(priv->interfaces, iface);
			}
		}
		libusb_free_config_descriptor(config);
		priv->interfaces_valid = TRUE;
	}

	/* success */
	return g_ptr_array_ref(priv->interfaces);
}

/**
 * g_usb_device_get_events:
 * @self: a #GUsbDevice
 *
 * Gets all the events saved by the device.
 *
 * Events are only collected when the `G_USB_CONTEXT_FLAGS_SAVE_EVENTS` flag is used before
 * enumerating the context. Events can be used to replay device transactions.
 *
 * Return value: (transfer container) (element-type GUsbDeviceEvent): an array of events
 *
 * Since: 0.4.0
 **/
GPtrArray *
g_usb_device_get_events(GUsbDevice *self)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	g_return_val_if_fail(G_USB_IS_DEVICE(self), NULL);
	return g_ptr_array_ref(priv->events);
}

/**
 * g_usb_device_clear_events:
 * @self: a #GUsbDevice
 *
 * Clear all the events saved by the device.
 *
 * Since: 0.4.4
 **/
void
g_usb_device_clear_events(GUsbDevice *self)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	g_return_if_fail(G_USB_IS_DEVICE(self));
	priv->event_idx = 0;
	g_ptr_array_set_size(priv->events, 0);
}

/**
 * g_usb_device_invalidate:
 * @self: a #GUsbDevice
 *
 * Invalidates the caches used in g_usb_device_get_interfaces().
 *
 * Since: 0.4.0
 **/
void
g_usb_device_invalidate(GUsbDevice *self)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	g_return_if_fail(G_USB_IS_DEVICE(self));
	priv->interfaces_valid = FALSE;
	priv->bos_descriptors_valid = FALSE;
	g_ptr_array_set_size(priv->interfaces, 0);
	g_ptr_array_set_size(priv->bos_descriptors, 0);
	g_ptr_array_set_size(priv->hid_descriptors, 0);
}

/**
 * g_usb_device_get_bos_descriptor:
 * @self: a #GUsbDevice
 * @capability: a BOS capability type
 * @error: a #GError, or %NULL
 *
 * Gets the first bos_descriptor that matches the descriptor capability.
 * If you want to find all the BOS descriptors that match (there may be other matching BOS
 * descriptors you have to use `g_usb_device_get_bos_descriptors()` and check each one manually.
 *
 * Return value: (transfer full): a #GUsbBosDescriptor or %NULL for not found
 *
 * Since: 0.4.0
 **/
GUsbBosDescriptor *
g_usb_device_get_bos_descriptor(GUsbDevice *self, guint8 capability, GError **error)
{
	g_autoptr(GPtrArray) bos_descriptors = NULL;

	g_return_val_if_fail(G_USB_IS_DEVICE(self), NULL);
	g_return_val_if_fail(error == NULL || *error == NULL, NULL);

	/* find the right data */
	bos_descriptors = g_usb_device_get_bos_descriptors(self, error);
	if (bos_descriptors == NULL)
		return NULL;
	for (guint i = 0; i < bos_descriptors->len; i++) {
		GUsbBosDescriptor *bos_descriptor = g_ptr_array_index(bos_descriptors, i);
		if (g_usb_bos_descriptor_get_capability(bos_descriptor) == capability)
			return g_object_ref(bos_descriptor);
	}

	/* nothing matched */
	g_set_error(error,
		    G_USB_DEVICE_ERROR,
		    G_USB_DEVICE_ERROR_NOT_SUPPORTED,
		    "no BOS descriptor for capability 0x%02x",
		    capability);
	return NULL;
}

/**
 * g_usb_device_get_bos_descriptors:
 * @self: a #GUsbDevice
 * @error: a #GError, or %NULL
 *
 * Gets all the BOS descriptors exported by the device.
 *
 * The first time this method is used the hardware is queried and then after that cached results
 * are returned. To invalidate the caches use g_usb_device_invalidate().
 *
 * Return value: (transfer container) (element-type GUsbBosDescriptor): an array of BOS descriptors
 *
 * Since: 0.4.0
 **/
GPtrArray *
g_usb_device_get_bos_descriptors(GUsbDevice *self, GError **error)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);

	g_return_val_if_fail(G_USB_IS_DEVICE(self), NULL);
	g_return_val_if_fail(error == NULL || *error == NULL, NULL);

	/* get all BOS descriptors */
	if (!priv->bos_descriptors_valid) {
		gint rc;
		guint8 num_device_caps;
		struct libusb_bos_descriptor *bos = NULL;

		/* sanity check */
		if (priv->device == NULL) {
			g_set_error_literal(error,
					    G_IO_ERROR,
					    G_IO_ERROR_NOT_SUPPORTED,
					    "not supported for emulated device");
			return NULL;
		}
		if (priv->handle == NULL) {
			g_usb_device_not_open_error(self, error);
			return NULL;
		}
		if (g_usb_device_get_spec(self) <= 0x0200) {
			g_set_error(error,
				    G_IO_ERROR,
				    G_IO_ERROR_NOT_SUPPORTED,
				    "not available as bcdUSB 0x%04x <= 0x0200",
				    g_usb_device_get_spec(self));
			return NULL;
		}

		rc = libusb_get_bos_descriptor(priv->handle, &bos);
		if (!g_usb_device_libusb_error_to_gerror(self, rc, error))
			return NULL;
#ifdef __FreeBSD__
		num_device_caps = bos->bNumDeviceCapabilities;
#else
		num_device_caps = bos->bNumDeviceCaps;
#endif
		for (guint i = 0; i < num_device_caps; i++) {
			GUsbBosDescriptor *bos_descriptor = NULL;
			struct libusb_bos_dev_capability_descriptor *bos_cap =
			    bos->dev_capability[i];
			bos_descriptor = _g_usb_bos_descriptor_new(bos_cap);
			g_ptr_array_add(priv->bos_descriptors, bos_descriptor);
		}
		libusb_free_bos_descriptor(bos);
		priv->bos_descriptors_valid = TRUE;
	}

	/* success */
	return g_ptr_array_ref(priv->bos_descriptors);
}

static GBytes *
g_usb_device_get_hid_descriptor_for_interface(GUsbDevice *self, GUsbInterface *intf, GError **error)
{
	GBytes *extra;
	gsize bufsz = 0;
	const guint8 *buf;
	gsize actual_length = 0;
	gsize buf2sz;
	guint16 buf2szle = 0;
	g_autofree guint8 *buf2 = NULL;

	extra = g_usb_interface_get_extra(intf);
	if (extra == NULL) {
		g_set_error(error,
			    G_IO_ERROR,
			    G_IO_ERROR_NOT_FOUND,
			    "no data found on HID interface 0x%x",
			    g_usb_interface_get_number(intf));
		return NULL;
	}
	buf = g_bytes_get_data(extra, &bufsz);
	if (bufsz < 9) {
		g_set_error(error,
			    G_IO_ERROR,
			    G_IO_ERROR_NOT_FOUND,
			    "invalid data on HID interface 0x%x",
			    g_usb_interface_get_number(intf));
		return NULL;
	}
	if (buf[1] != LIBUSB_DT_HID) {
		g_set_error(error,
			    G_IO_ERROR,
			    G_IO_ERROR_NOT_FOUND,
			    "invalid data on HID interface 0x%x, got 0x%x and expected 0x%x",
			    g_usb_interface_get_number(intf),
			    buf[1],
			    (guint)LIBUSB_DT_HID);
		return NULL;
	}
	memcpy(&buf2szle, buf + 7, sizeof(buf2szle));
	buf2sz = GUINT16_FROM_LE(buf2szle);
	if (buf2sz == 0) {
		g_set_error(error,
			    G_IO_ERROR,
			    G_IO_ERROR_NOT_FOUND,
			    "missing data on HID interface 0x%x",
			    g_usb_interface_get_number(intf));
		return NULL;
	}
	g_debug("get 0x%x bytes of HID descriptor on iface 0x%x",
		(guint)buf2sz,
		g_usb_interface_get_number(intf));

	/* get HID descriptor */
	buf2 = g_malloc0(buf2sz);
	if (!g_usb_device_control_transfer(self,
					   G_USB_DEVICE_DIRECTION_DEVICE_TO_HOST,
					   G_USB_DEVICE_REQUEST_TYPE_STANDARD,
					   G_USB_DEVICE_RECIPIENT_INTERFACE,
					   LIBUSB_REQUEST_GET_DESCRIPTOR,
					   LIBUSB_DT_REPORT << 8,
					   g_usb_interface_get_number(intf),
					   buf2,
					   buf2sz,
					   &actual_length,
					   5000,
					   NULL,
					   error)) {
		g_prefix_error(error, "failed to get HID report descriptor: ");
		return NULL;
	}
	if (actual_length < buf2sz) {
		g_set_error(error,
			    G_IO_ERROR,
			    G_IO_ERROR_NOT_FOUND,
			    "invalid data on HID interface 0x%x, got 0x%x and expected 0x%x",
			    g_usb_interface_get_number(intf),
			    (guint)actual_length,
			    (guint)buf2sz);
		return NULL;
	}

	/* success */
	return g_bytes_new_take(g_steal_pointer(&buf2), actual_length);
}

/**
 * g_usb_device_get_hid_descriptors:
 * @self: a #GUsbDevice
 * @error: a #GError, or %NULL
 *
 * Gets all the HID descriptors exported by the device.
 *
 * The first time this method is used the hardware is queried and then after that cached results
 * are returned. To invalidate the caches use g_usb_device_invalidate().
 *
 * Return value: (transfer container) (element-type GBytes): an array of HID descriptors
 *
 * Since: 0.4.7
 **/
GPtrArray *
g_usb_device_get_hid_descriptors(GUsbDevice *self, GError **error)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	g_autoptr(GPtrArray) interfaces = NULL;

	g_return_val_if_fail(G_USB_IS_DEVICE(self), NULL);
	g_return_val_if_fail(error == NULL || *error == NULL, NULL);

	/* sanity check */
	if (!priv->hid_descriptors_valid) {
		if (priv->device == NULL) {
			g_set_error_literal(error,
					    G_IO_ERROR,
					    G_IO_ERROR_NOT_SUPPORTED,
					    "not supported for emulated device");
			return NULL;
		}
		if (priv->handle == NULL) {
			g_usb_device_not_open_error(self, error);
			return NULL;
		}

		interfaces = g_usb_device_get_interfaces(self, error);
		if (interfaces == NULL)
			return NULL;
		for (guint i = 0; i < interfaces->len; i++) {
			GUsbInterface *intf = g_ptr_array_index(interfaces, i);
			g_autoptr(GBytes) blob = NULL;
			if (g_usb_interface_get_class(intf) != G_USB_DEVICE_CLASS_HID)
				continue;
			blob = g_usb_device_get_hid_descriptor_for_interface(self, intf, error);
			if (blob == NULL)
				return NULL;
			g_ptr_array_add(priv->hid_descriptors, g_steal_pointer(&blob));
		}
		priv->hid_descriptors_valid = TRUE;
	}

	/* success */
	return g_ptr_array_ref(priv->hid_descriptors);
}

/**
 * g_usb_device_get_hid_descriptor_default:
 * @self: a #GUsbDevice
 * @error: a #GError, or %NULL
 *
 * Gets the default HID descriptors exported by the device.
 *
 * If more than one interface exports a HID descriptor, use g_usb_device_get_hid_descriptors()
 * instead.
 *
 * Return value: (transfer full): a HID descriptor, or %NULL
 *
 * Since: 0.4.7
 **/
GBytes *
g_usb_device_get_hid_descriptor_default(GUsbDevice *self, GError **error)
{
	g_autoptr(GPtrArray) hid_descriptors = NULL;

	g_return_val_if_fail(G_USB_IS_DEVICE(self), NULL);
	g_return_val_if_fail(error == NULL || *error == NULL, NULL);

	hid_descriptors = g_usb_device_get_hid_descriptors(self, error);
	if (hid_descriptors == NULL)
		return NULL;
	if (hid_descriptors->len != 1) {
		g_set_error(error,
			    G_IO_ERROR,
			    G_IO_ERROR_NOT_SUPPORTED,
			    "no default descriptor, got %u",
			    hid_descriptors->len);
		return NULL;
	}
	return g_bytes_ref(g_ptr_array_index(hid_descriptors, 0));
}

/**
 * g_usb_device_close:
 * @self: a #GUsbDevice
 * @error: a #GError, or %NULL
 *
 * Closes the device when it is no longer required.
 *
 * Return value: %TRUE on success
 *
 * Since: 0.1.0
 **/
gboolean
g_usb_device_close(GUsbDevice *self, GError **error)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);

	g_return_val_if_fail(G_USB_IS_DEVICE(self), FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	/* emulated */
	if (priv->device == NULL)
		return TRUE;

	/* ignore */
	if (g_usb_context_get_flags(priv->context) & G_USB_CONTEXT_FLAGS_AUTO_OPEN_DEVICES)
		return TRUE;

	if (priv->handle == NULL)
		return g_usb_device_not_open_error(self, error);

	libusb_close(priv->handle);
	priv->handle = NULL;
	return TRUE;
}

/**
 * g_usb_device_reset:
 * @self: a #GUsbDevice
 * @error: a #GError, or %NULL
 *
 * Perform a USB port reset to reinitialize a device.
 *
 * If the reset succeeds, the device will appear to disconnected and reconnected.
 * This means the @self will no longer be valid and should be closed and
 * rediscovered.
 *
 * This is a blocking function which usually incurs a noticeable delay.
 *
 * Return value: %TRUE on success
 *
 * Since: 0.1.0
 **/
gboolean
g_usb_device_reset(GUsbDevice *self, GError **error)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	gint rc;
	g_return_val_if_fail(G_USB_IS_DEVICE(self), FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	/* emulated */
	if (priv->device == NULL)
		return TRUE;

	if (priv->handle == NULL)
		return g_usb_device_not_open_error(self, error);

	rc = libusb_reset_device(priv->handle);
	if (rc == LIBUSB_ERROR_NOT_FOUND)
		return TRUE;
	return g_usb_device_libusb_error_to_gerror(self, rc, error);
}

/**
 * g_usb_device_get_configuration:
 * @self: a #GUsbDevice
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
g_usb_device_get_configuration(GUsbDevice *self, GError **error)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	gint rc;
	int config;

	g_return_val_if_fail(G_USB_IS_DEVICE(self), -1);
	g_return_val_if_fail(error == NULL || *error == NULL, -1);

	/* emulated */
	if (priv->device == NULL)
		return 0x0;

	if (priv->handle == NULL) {
		g_usb_device_not_open_error(self, error);
		return -1;
	}

	rc = libusb_get_configuration(priv->handle, &config);
	if (rc != LIBUSB_SUCCESS) {
		g_usb_device_libusb_error_to_gerror(self, rc, error);
		return -1;
	}

	return config;
}

/**
 * g_usb_device_set_configuration:
 * @self: a #GUsbDevice
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
g_usb_device_set_configuration(GUsbDevice *self, gint configuration, GError **error)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	gint rc;
	gint config_tmp = 0;

	g_return_val_if_fail(G_USB_IS_DEVICE(self), FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	/* emulated */
	if (priv->device == NULL)
		return TRUE;

	if (priv->handle == NULL)
		return g_usb_device_not_open_error(self, error);

	/* verify we've not already set the same configuration */
	rc = libusb_get_configuration(priv->handle, &config_tmp);
	if (rc != LIBUSB_SUCCESS) {
		return g_usb_device_libusb_error_to_gerror(self, rc, error);
	}
	if (config_tmp == configuration)
		return TRUE;

	/* different, so change */
	rc = libusb_set_configuration(priv->handle, configuration);
	return g_usb_device_libusb_error_to_gerror(self, rc, error);
}

/**
 * g_usb_device_claim_interface:
 * @self: a #GUsbDevice
 * @iface: bInterfaceNumber of the interface you wish to claim
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
g_usb_device_claim_interface(GUsbDevice *self,
			     gint iface,
			     GUsbDeviceClaimInterfaceFlags flags,
			     GError **error)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	gint rc;

	g_return_val_if_fail(G_USB_IS_DEVICE(self), FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	/* emulated */
	if (priv->device == NULL)
		return TRUE;

	if (priv->handle == NULL)
		return g_usb_device_not_open_error(self, error);

	if (flags & G_USB_DEVICE_CLAIM_INTERFACE_BIND_KERNEL_DRIVER) {
		rc = libusb_detach_kernel_driver(priv->handle, iface);
		if (rc != LIBUSB_SUCCESS && rc != LIBUSB_ERROR_NOT_FOUND && /* No driver attached */
		    rc != LIBUSB_ERROR_NOT_SUPPORTED &&			    /* win32 */
		    rc != LIBUSB_ERROR_BUSY /* driver rebound already */)
			return g_usb_device_libusb_error_to_gerror(self, rc, error);
	}

	rc = libusb_claim_interface(priv->handle, iface);
	return g_usb_device_libusb_error_to_gerror(self, rc, error);
}

/**
 * g_usb_device_release_interface:
 * @self: a #GUsbDevice
 * @iface: bInterfaceNumber of the interface you wish to release
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
g_usb_device_release_interface(GUsbDevice *self,
			       gint iface,
			       GUsbDeviceClaimInterfaceFlags flags,
			       GError **error)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	gint rc;

	g_return_val_if_fail(G_USB_IS_DEVICE(self), FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	/* emulated */
	if (priv->device == NULL)
		return TRUE;

	if (priv->handle == NULL)
		return g_usb_device_not_open_error(self, error);

	rc = libusb_release_interface(priv->handle, iface);
	if (rc != LIBUSB_SUCCESS)
		return g_usb_device_libusb_error_to_gerror(self, rc, error);

	if (flags & G_USB_DEVICE_CLAIM_INTERFACE_BIND_KERNEL_DRIVER) {
		rc = libusb_attach_kernel_driver(priv->handle, iface);
		if (rc != LIBUSB_SUCCESS && rc != LIBUSB_ERROR_NOT_FOUND && /* No driver attached */
		    rc != LIBUSB_ERROR_NOT_SUPPORTED &&			    /* win32 */
		    rc != LIBUSB_ERROR_BUSY /* driver rebound already */)
			return g_usb_device_libusb_error_to_gerror(self, rc, error);
	}

	return TRUE;
}

/**
 * g_usb_device_set_interface_alt:
 * @self: a #GUsbDevice
 * @iface: bInterfaceNumber of the interface you wish to release
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
g_usb_device_set_interface_alt(GUsbDevice *self, gint iface, guint8 alt, GError **error)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	gint rc;

	g_return_val_if_fail(G_USB_IS_DEVICE(self), FALSE);
	g_return_val_if_fail(error == NULL || *error == NULL, FALSE);

	/* emulated */
	if (priv->device == NULL)
		return TRUE;

	if (priv->handle == NULL)
		return g_usb_device_not_open_error(self, error);

	rc = libusb_set_interface_alt_setting(priv->handle, iface, (gint)alt);
	if (rc != LIBUSB_SUCCESS)
		return g_usb_device_libusb_error_to_gerror(self, rc, error);

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
g_usb_device_get_string_descriptor(GUsbDevice *self, guint8 desc_index, GError **error)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	GUsbDeviceEvent *event;
	gint rc;
	/* libusb_get_string_descriptor_ascii returns max 128 bytes */
	unsigned char buf[128];
	g_autofree gchar *event_id = NULL;

	g_return_val_if_fail(G_USB_IS_DEVICE(self), NULL);
	g_return_val_if_fail(error == NULL || *error == NULL, NULL);

	/* build event key either for load or save */
	if (priv->device == NULL ||
	    g_usb_context_get_flags(priv->context) & G_USB_CONTEXT_FLAGS_SAVE_EVENTS) {
		event_id = g_strdup_printf("GetStringDescriptor:DescIndex=0x%02x", desc_index);
	}

	/* emulated */
	if (priv->device == NULL) {
		GBytes *bytes;
		event = g_usb_device_load_event(self, event_id);
		if (event == NULL) {
			g_set_error(error,
				    G_IO_ERROR,
				    G_IO_ERROR_INVALID_DATA,
				    "no matching event data for %s",
				    event_id);
			return NULL;
		}
		if (!g_usb_device_libusb_error_to_gerror(self,
							 g_usb_device_event_get_rc(event),
							 error))
			return NULL;
		bytes = g_usb_device_event_get_bytes(event);
		if (bytes == NULL) {
			g_set_error(error,
				    G_IO_ERROR,
				    G_IO_ERROR_INVALID_DATA,
				    "no matching event data for %s",
				    event_id);
			return NULL;
		}
		return g_strndup(g_bytes_get_data(bytes, NULL), g_bytes_get_size(bytes));
	}
	if (priv->handle == NULL) {
		g_usb_device_not_open_error(self, error);
		return NULL;
	}

	rc = libusb_get_string_descriptor_ascii(priv->handle, desc_index, buf, sizeof(buf));
	if (rc < 0) {
		g_usb_device_libusb_error_to_gerror(self, rc, error);
		return NULL;
	}

	/* save */
	if (g_usb_context_get_flags(priv->context) & G_USB_CONTEXT_FLAGS_SAVE_EVENTS) {
		event = g_usb_device_save_event(self, event_id);
		_g_usb_device_event_set_bytes_raw(event, buf, sizeof(buf));
	}

	return g_strdup((const gchar *)buf);
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
g_usb_device_get_string_descriptor_bytes_full(GUsbDevice *self,
					      guint8 desc_index,
					      guint16 langid,
					      gsize length,
					      GError **error)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	GUsbDeviceEvent *event;
	gint rc;
	g_autofree guint8 *buf = g_malloc0(length);
	g_autofree gchar *event_id = NULL;

	g_return_val_if_fail(G_USB_IS_DEVICE(self), NULL);
	g_return_val_if_fail(error == NULL || *error == NULL, NULL);

	/* build event key either for load or save */
	if (priv->device == NULL ||
	    g_usb_context_get_flags(priv->context) & G_USB_CONTEXT_FLAGS_SAVE_EVENTS) {
		event_id = g_strdup_printf(
		    "GetStringDescriptorBytes:DescIndex=0x%02x,Langid=0x%04x,Length=0x%x",
		    desc_index,
		    langid,
		    (guint)length);
	}

	/* emulated */
	if (priv->device == NULL) {
		GBytes *bytes;
		event = g_usb_device_load_event(self, event_id);
		if (event == NULL) {
			g_set_error(error,
				    G_IO_ERROR,
				    G_IO_ERROR_INVALID_DATA,
				    "no matching event for %s",
				    event_id);
			return NULL;
		}
		if (!g_usb_device_libusb_error_to_gerror(self,
							 g_usb_device_event_get_rc(event),
							 error))
			return 0x00;
		bytes = g_usb_device_event_get_bytes(event);
		if (bytes == NULL) {
			g_set_error(error,
				    G_IO_ERROR,
				    G_IO_ERROR_INVALID_DATA,
				    "no matching event data for %s",
				    event_id);
			return NULL;
		}
		return g_bytes_ref(bytes);
	}

	if (priv->handle == NULL) {
		g_usb_device_not_open_error(self, error);
		return NULL;
	}

	rc = libusb_get_string_descriptor(priv->handle, desc_index, langid, buf, length);
	if (rc < 0) {
		g_usb_device_libusb_error_to_gerror(self, rc, error);
		return NULL;
	}

	/* save */
	if (g_usb_context_get_flags(priv->context) & G_USB_CONTEXT_FLAGS_SAVE_EVENTS) {
		event = g_usb_device_save_event(self, event_id);
		_g_usb_device_event_set_bytes_raw(event, buf, rc);
	}

	return g_bytes_new(buf, rc);
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
g_usb_device_get_string_descriptor_bytes(GUsbDevice *self,
					 guint8 desc_index,
					 guint16 langid,
					 GError **error)
{
	return g_usb_device_get_string_descriptor_bytes_full(self, desc_index, langid, 128, error);
}

typedef gssize(GUsbDeviceTransferFinishFunc)(GUsbDevice *self, GAsyncResult *res, GError **error);

typedef struct {
	GError **error;
	GMainContext *context;
	GMainLoop *loop;
	GUsbDeviceTransferFinishFunc *finish_func;
	gssize ret;
} GUsbSyncHelper;

static void
g_usb_device_sync_transfer_cb(GUsbDevice *self, GAsyncResult *res, GUsbSyncHelper *helper)
{
	helper->ret = (*helper->finish_func)(self, res, helper->error);
	g_main_loop_quit(helper->loop);
}

/**
 * g_usb_device_control_transfer:
 * @self: a #GUsbDevice
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
g_usb_device_control_transfer(GUsbDevice *self,
			      GUsbDeviceDirection direction,
			      GUsbDeviceRequestType request_type,
			      GUsbDeviceRecipient recipient,
			      guint8 request,
			      guint16 value,
			      guint16 idx,
			      guint8 *data,
			      gsize length,
			      gsize *actual_length,
			      guint timeout,
			      GCancellable *cancellable,
			      GError **error)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	GUsbSyncHelper helper;

	helper.ret = -1;
	helper.context = g_usb_context_get_main_context(priv->context);
	helper.loop = g_main_loop_new(helper.context, FALSE);
	helper.error = error;
	helper.finish_func = g_usb_device_control_transfer_finish;

	g_usb_device_control_transfer_async(self,
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
					    (GAsyncReadyCallback)g_usb_device_sync_transfer_cb,
					    &helper);
	g_main_loop_run(helper.loop);
	g_main_loop_unref(helper.loop);

	if (actual_length != NULL)
		*actual_length = (gsize)helper.ret;

	return helper.ret != -1;
}

/**
 * g_usb_device_bulk_transfer:
 * @self: a #GUsbDevice
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
g_usb_device_bulk_transfer(GUsbDevice *self,
			   guint8 endpoint,
			   guint8 *data,
			   gsize length,
			   gsize *actual_length,
			   guint timeout,
			   GCancellable *cancellable,
			   GError **error)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	GUsbSyncHelper helper;

	helper.ret = -1;
	helper.context = g_usb_context_get_main_context(priv->context);
	helper.loop = g_main_loop_new(helper.context, FALSE);
	helper.error = error;
	helper.finish_func = g_usb_device_bulk_transfer_finish;

	g_usb_device_bulk_transfer_async(self,
					 endpoint,
					 data,
					 length,
					 timeout,
					 cancellable,
					 (GAsyncReadyCallback)g_usb_device_sync_transfer_cb,
					 &helper);
	g_main_loop_run(helper.loop);
	g_main_loop_unref(helper.loop);

	if (actual_length != NULL)
		*actual_length = (gsize)helper.ret;

	return helper.ret != -1;
}

/**
 * g_usb_device_interrupt_transfer:
 * @self: a #GUsbDevice
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
g_usb_device_interrupt_transfer(GUsbDevice *self,
				guint8 endpoint,
				guint8 *data,
				gsize length,
				gsize *actual_length,
				guint timeout,
				GCancellable *cancellable,
				GError **error)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	GUsbSyncHelper helper;

	helper.ret = -1;
	helper.context = g_usb_context_get_main_context(priv->context);
	helper.loop = g_main_loop_new(helper.context, FALSE);
	helper.error = error;
	helper.finish_func = g_usb_device_interrupt_transfer_finish;

	g_usb_device_interrupt_transfer_async(self,
					      endpoint,
					      data,
					      length,
					      timeout,
					      cancellable,
					      (GAsyncReadyCallback)g_usb_device_sync_transfer_cb,
					      &helper);
	g_main_loop_run(helper.loop);
	g_main_loop_unref(helper.loop);

	if (actual_length != NULL)
		*actual_length = helper.ret;

	return helper.ret != -1;
}

typedef struct {
	GCancellable *cancellable;
	gulong cancellable_id;
	struct libusb_transfer *transfer;
	guint8 *data;		/* owned by the user */
	guint8 *data_raw;	/* owned by the task */
	GUsbDeviceEvent *event; /* no-ref */
} GcmDeviceReq;

static void
g_usb_device_req_free(GcmDeviceReq *req)
{
	g_free(req->data_raw);
	if (req->cancellable_id > 0) {
		g_cancellable_disconnect(req->cancellable, req->cancellable_id);
		g_object_unref(req->cancellable);
	}

	libusb_free_transfer(req->transfer);
	g_slice_free(GcmDeviceReq, req);
}

static gboolean
g_usb_device_libusb_status_to_gerror(gint status, GError **error)
{
	gboolean ret = FALSE;

	switch (status) {
	case LIBUSB_TRANSFER_COMPLETED:
		ret = TRUE;
		break;
	case LIBUSB_TRANSFER_ERROR:
		g_set_error_literal(error,
				    G_USB_DEVICE_ERROR,
				    G_USB_DEVICE_ERROR_FAILED,
				    "transfer failed");
		break;
	case LIBUSB_TRANSFER_TIMED_OUT:
		g_set_error_literal(error,
				    G_USB_DEVICE_ERROR,
				    G_USB_DEVICE_ERROR_TIMED_OUT,
				    "transfer timed out");
		break;
	case LIBUSB_TRANSFER_CANCELLED:
		g_set_error_literal(error,
				    G_USB_DEVICE_ERROR,
				    G_USB_DEVICE_ERROR_CANCELLED,
				    "transfer cancelled");
		break;
	case LIBUSB_TRANSFER_STALL:
		g_set_error_literal(error,
				    G_USB_DEVICE_ERROR,
				    G_USB_DEVICE_ERROR_NOT_SUPPORTED,
				    "endpoint stalled or request not supported");
		break;
	case LIBUSB_TRANSFER_NO_DEVICE:
		g_set_error_literal(error,
				    G_USB_DEVICE_ERROR,
				    G_USB_DEVICE_ERROR_NO_DEVICE,
				    "device was disconnected");
		break;
	case LIBUSB_TRANSFER_OVERFLOW:
		g_set_error_literal(error,
				    G_USB_DEVICE_ERROR,
				    G_USB_DEVICE_ERROR_INTERNAL,
				    "device sent more data than requested");
		break;
	default:
		g_set_error(error,
			    G_USB_DEVICE_ERROR,
			    G_USB_DEVICE_ERROR_INTERNAL,
			    "unknown status [%i]",
			    status);
	}
	return ret;
}

static void LIBUSB_CALL
g_usb_device_async_transfer_cb(struct libusb_transfer *transfer)
{
	GTask *task = transfer->user_data;
	GcmDeviceReq *req = g_task_get_task_data(task);
	gboolean ret;
	GError *error = NULL;

	/* did request fail? */
	ret = g_usb_device_libusb_status_to_gerror(transfer->status, &error);
	if (!ret) {
		if (req->event != NULL)
			_g_usb_device_event_set_status(req->event, transfer->status);
		g_task_return_error(task, error);
	} else {
		if (req->event != NULL) {
			_g_usb_device_event_set_bytes_raw(req->event,
							  transfer->buffer,
							  (gsize)transfer->actual_length);
		}
		g_task_return_int(task, transfer->actual_length);
	}

	g_object_unref(task);
}

static void
g_usb_device_cancelled_cb(GCancellable *cancellable, GcmDeviceReq *req)
{
	libusb_cancel_transfer(req->transfer);
}

static void LIBUSB_CALL
g_usb_device_control_transfer_cb(struct libusb_transfer *transfer)
{
	GTask *task = transfer->user_data;
	GcmDeviceReq *req = g_task_get_task_data(task);
	gboolean ret;
	GError *error = NULL;

	/* did request fail? */
	ret = g_usb_device_libusb_status_to_gerror(transfer->status, &error);
	if (!ret) {
		if (req->event != NULL)
			_g_usb_device_event_set_status(req->event, transfer->status);
		g_task_return_error(task, error);
	} else {
		memmove(req->data,
			transfer->buffer + LIBUSB_CONTROL_SETUP_SIZE,
			(gsize)transfer->actual_length);
		if (req->event != NULL) {
			_g_usb_device_event_set_bytes_raw(req->event,
							  transfer->buffer +
							      LIBUSB_CONTROL_SETUP_SIZE,
							  (gsize)transfer->actual_length);
		}
		g_task_return_int(task, transfer->actual_length);
	}

	g_object_unref(task);
}

/* copy @dstsz bytes of @bytes into @dst */
static gboolean
gusb_memcpy_bytes_safe(guint8 *dst, gsize dstsz, GBytes *bytes, GError **error)
{
	/* sanity check */
	if (dstsz < g_bytes_get_size(bytes)) {
		g_set_error(
		    error,
		    G_IO_ERROR,
		    G_IO_ERROR_INVALID_DATA,
		    "cannot copy source buffer of size 0x%x into destination buffer of size 0x%x",
		    (guint)g_bytes_get_size(bytes),
		    (guint)dstsz);
		return FALSE;
	}

	/* data is the same */
	if (memcmp(dst, g_bytes_get_data(bytes, NULL), g_bytes_get_size(bytes)) == 0)
		return TRUE;

	/* if this explodes it's because the caller has cast an immutable buffer to a guint8* */
	memcpy(dst, g_bytes_get_data(bytes, NULL), g_bytes_get_size(bytes));
	return TRUE;
}

/**
 * g_usb_device_control_transfer_async:
 * @self: a #GUsbDevice
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
g_usb_device_control_transfer_async(GUsbDevice *self,
				    GUsbDeviceDirection direction,
				    GUsbDeviceRequestType request_type,
				    GUsbDeviceRecipient recipient,
				    guint8 request,
				    guint16 value,
				    guint16 idx,
				    guint8 *data,
				    gsize length,
				    guint timeout,
				    GCancellable *cancellable,
				    GAsyncReadyCallback callback,
				    gpointer user_data)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	GTask *task;
	GcmDeviceReq *req;
	gint rc;
	guint8 request_type_raw = 0;
	GError *error = NULL;
	GUsbDeviceEvent *event = NULL;
	g_autofree gchar *event_id = NULL;

	g_return_if_fail(G_USB_IS_DEVICE(self));

	/* build event key either for load or save */
	if (priv->device == NULL ||
	    g_usb_context_get_flags(priv->context) & G_USB_CONTEXT_FLAGS_SAVE_EVENTS) {
		g_autofree gchar *data_base64 = g_base64_encode(data, length);
		event_id = g_strdup_printf("ControlTransfer:"
					   "Direction=0x%02x,"
					   "RequestType=0x%02x,"
					   "Recipient=0x%02x,"
					   "Request=0x%02x,"
					   "Value=0x%04x,"
					   "Idx=0x%04x,"
					   "Data=%s,"
					   "Length=0x%x",
					   direction,
					   request_type,
					   recipient,
					   request,
					   value,
					   idx,
					   data_base64,
					   (guint)length);
	}

	/* emulated */
	if (priv->device == NULL) {
		GBytes *bytes;
		event = g_usb_device_load_event(self, event_id);
		if (event == NULL) {
			g_task_report_new_error(self,
						callback,
						user_data,
						g_usb_device_control_transfer_async,
						G_IO_ERROR,
						G_IO_ERROR_INVALID_DATA,
						"no matching event for %s",
						event_id);
			return;
		}
		if (!g_usb_device_libusb_error_to_gerror(self,
							 g_usb_device_event_get_rc(event),
							 &error) ||
		    !g_usb_device_libusb_status_to_gerror(g_usb_device_event_get_status(event),
							  &error)) {
			g_task_report_error(self,
					    callback,
					    user_data,
					    g_usb_device_control_transfer_async,
					    error);
			return;
		}
		bytes = g_usb_device_event_get_bytes(event);
		if (bytes == NULL) {
			g_task_report_new_error(self,
						callback,
						user_data,
						g_usb_device_control_transfer_async,
						G_IO_ERROR,
						G_IO_ERROR_INVALID_DATA,
						"no matching event data for %s",
						event_id);
			return;
		}
		if (!gusb_memcpy_bytes_safe(data, length, bytes, &error)) {
			g_task_report_error(self,
					    callback,
					    user_data,
					    g_usb_device_control_transfer_async,
					    error);
			return;
		}
		task = g_task_new(self, cancellable, callback, user_data);
		g_task_return_int(task, g_bytes_get_size(bytes));
		g_object_unref(task);
		return;
	}

	if (priv->handle == NULL) {
		g_usb_device_async_not_open_error(self,
						  callback,
						  user_data,
						  g_usb_device_control_transfer_async);
		return;
	}

	/* save */
	if (g_usb_context_get_flags(priv->context) & G_USB_CONTEXT_FLAGS_SAVE_EVENTS)
		event = g_usb_device_save_event(self, event_id);

	req = g_slice_new0(GcmDeviceReq);
	req->transfer = libusb_alloc_transfer(0);
	req->data = data;
	req->event = event;

	task = g_task_new(self, cancellable, callback, user_data);
	g_task_set_task_data(task, req, (GDestroyNotify)g_usb_device_req_free);

	if (g_task_return_error_if_cancelled(task)) {
		g_object_unref(task);
		return;
	}

	/* munge back to flags */
	if (direction == G_USB_DEVICE_DIRECTION_DEVICE_TO_HOST)
		request_type_raw |= 0x80;
	request_type_raw |= (request_type << 5);
	request_type_raw |= recipient;

	req->data_raw = g_malloc0(length + LIBUSB_CONTROL_SETUP_SIZE);
	memmove(req->data_raw + LIBUSB_CONTROL_SETUP_SIZE, data, length);

	/* fill in setup packet */
	libusb_fill_control_setup(req->data_raw, request_type_raw, request, value, idx, length);

	/* fill in transfer details */
	libusb_fill_control_transfer(req->transfer,
				     priv->handle,
				     req->data_raw,
				     g_usb_device_control_transfer_cb,
				     task,
				     timeout);

	/* submit transfer */
	rc = libusb_submit_transfer(req->transfer);
	if (rc < 0) {
		if (event != NULL)
			_g_usb_device_event_set_rc(event, rc);
		g_usb_device_libusb_error_to_gerror(self, rc, &error);
		g_task_return_error(task, error);
		g_object_unref(task);
	}

	/* setup cancellation after submission */
	if (cancellable != NULL) {
		req->cancellable = g_object_ref(cancellable);
		req->cancellable_id = g_cancellable_connect(req->cancellable,
							    G_CALLBACK(g_usb_device_cancelled_cb),
							    req,
							    NULL);
	}
}

/**
 * g_usb_device_control_transfer_finish:
 * @self: a #GUsbDevice instance.
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
g_usb_device_control_transfer_finish(GUsbDevice *self, GAsyncResult *res, GError **error)
{
	g_return_val_if_fail(G_USB_IS_DEVICE(self), -1);
	g_return_val_if_fail(g_task_is_valid(res, self), -1);
	g_return_val_if_fail(error == NULL || *error == NULL, -1);

	return g_task_propagate_int(G_TASK(res), error);
}

/**
 * g_usb_device_bulk_transfer_async:
 * @self: a #GUsbDevice instance.
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
g_usb_device_bulk_transfer_async(GUsbDevice *self,
				 guint8 endpoint,
				 guint8 *data,
				 gsize length,
				 guint timeout,
				 GCancellable *cancellable,
				 GAsyncReadyCallback callback,
				 gpointer user_data)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	GTask *task;
	GcmDeviceReq *req;
	gint rc;
	GError *error = NULL;
	GUsbDeviceEvent *event = NULL;
	g_autofree gchar *event_id = NULL;

	g_return_if_fail(G_USB_IS_DEVICE(self));

	/* build event key either for load or save */
	if (priv->device == NULL ||
	    g_usb_context_get_flags(priv->context) & G_USB_CONTEXT_FLAGS_SAVE_EVENTS) {
		g_autofree gchar *data_base64 = g_base64_encode(data, length);
		event_id = g_strdup_printf("BulkTransfer:"
					   "Endpoint=0x%02x,"
					   "Data=%s,"
					   "Length=0x%x",
					   endpoint,
					   data_base64,
					   (guint)length);
	}

	/* emulated */
	if (priv->device == NULL) {
		GBytes *bytes;
		event = g_usb_device_load_event(self, event_id);
		if (event == NULL) {
			g_task_report_new_error(self,
						callback,
						user_data,
						g_usb_device_control_transfer_async,
						G_IO_ERROR,
						G_IO_ERROR_INVALID_DATA,
						"no matching event for %s",
						event_id);
			return;
		}
		if (!g_usb_device_libusb_error_to_gerror(self,
							 g_usb_device_event_get_rc(event),
							 &error) ||
		    !g_usb_device_libusb_status_to_gerror(g_usb_device_event_get_status(event),
							  &error)) {
			g_task_report_error(self,
					    callback,
					    user_data,
					    g_usb_device_control_transfer_async,
					    error);
			return;
		}
		bytes = g_usb_device_event_get_bytes(event);
		if (bytes == NULL) {
			g_task_report_new_error(self,
						callback,
						user_data,
						g_usb_device_control_transfer_async,
						G_IO_ERROR,
						G_IO_ERROR_INVALID_DATA,
						"no matching event data for %s",
						event_id);
			return;
		}
		if (!gusb_memcpy_bytes_safe(data, length, bytes, &error)) {
			g_task_report_error(self,
					    callback,
					    user_data,
					    g_usb_device_control_transfer_async,
					    error);
			return;
		}
		task = g_task_new(self, cancellable, callback, user_data);
		g_task_return_int(task, g_bytes_get_size(bytes));
		g_object_unref(task);
		return;
	}

	if (priv->handle == NULL) {
		g_usb_device_async_not_open_error(self,
						  callback,
						  user_data,
						  g_usb_device_bulk_transfer_async);
		return;
	}

	/* save */
	if (g_usb_context_get_flags(priv->context) & G_USB_CONTEXT_FLAGS_SAVE_EVENTS)
		event = g_usb_device_save_event(self, event_id);

	req = g_slice_new0(GcmDeviceReq);
	req->transfer = libusb_alloc_transfer(0);
	req->event = event;

	task = g_task_new(self, cancellable, callback, user_data);
	g_task_set_task_data(task, req, (GDestroyNotify)g_usb_device_req_free);

	if (g_task_return_error_if_cancelled(task)) {
		g_object_unref(task);
		return;
	}

	/* fill in transfer details */
	libusb_fill_bulk_transfer(req->transfer,
				  priv->handle,
				  endpoint,
				  data,
				  length,
				  g_usb_device_async_transfer_cb,
				  task,
				  timeout);

	/* submit transfer */
	rc = libusb_submit_transfer(req->transfer);
	if (rc < 0) {
		if (event != NULL)
			_g_usb_device_event_set_rc(event, rc);
		g_usb_device_libusb_error_to_gerror(self, rc, &error);
		g_task_return_error(task, error);
		g_object_unref(task);
	}

	/* setup cancellation after submission */
	if (cancellable != NULL) {
		req->cancellable = g_object_ref(cancellable);
		req->cancellable_id = g_cancellable_connect(req->cancellable,
							    G_CALLBACK(g_usb_device_cancelled_cb),
							    req,
							    NULL);
	}
}

/**
 * g_usb_device_bulk_transfer_finish:
 * @self: a #GUsbDevice instance.
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
g_usb_device_bulk_transfer_finish(GUsbDevice *self, GAsyncResult *res, GError **error)
{
	g_return_val_if_fail(G_USB_IS_DEVICE(self), -1);
	g_return_val_if_fail(g_task_is_valid(res, self), -1);
	g_return_val_if_fail(error == NULL || *error == NULL, -1);

	return g_task_propagate_int(G_TASK(res), error);
}

/**
 * g_usb_device_interrupt_transfer_async:
 * @self: a #GUsbDevice instance.
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
g_usb_device_interrupt_transfer_async(GUsbDevice *self,
				      guint8 endpoint,
				      guint8 *data,
				      gsize length,
				      guint timeout,
				      GCancellable *cancellable,
				      GAsyncReadyCallback callback,
				      gpointer user_data)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	GTask *task;
	GcmDeviceReq *req;
	GError *error = NULL;
	gint rc;
	GUsbDeviceEvent *event = NULL;
	g_autofree gchar *event_id = NULL;

	g_return_if_fail(G_USB_IS_DEVICE(self));

	/* build event key either for load or save */
	if (priv->device == NULL ||
	    g_usb_context_get_flags(priv->context) & G_USB_CONTEXT_FLAGS_SAVE_EVENTS) {
		g_autofree gchar *data_base64 = g_base64_encode(data, length);
		event_id = g_strdup_printf("InterruptTransfer:"
					   "Endpoint=0x%02x,"
					   "Data=%s,"
					   "Length=0x%x",
					   endpoint,
					   data_base64,
					   (guint)length);
	}

	/* emulated */
	if (priv->device == NULL) {
		GBytes *bytes;
		event = g_usb_device_load_event(self, event_id);
		if (event == NULL) {
			g_task_report_new_error(self,
						callback,
						user_data,
						g_usb_device_interrupt_transfer_async,
						G_IO_ERROR,
						G_IO_ERROR_INVALID_DATA,
						"no matching event for %s",
						event_id);
			return;
		}
		if (!g_usb_device_libusb_error_to_gerror(self,
							 g_usb_device_event_get_rc(event),
							 &error) ||
		    !g_usb_device_libusb_status_to_gerror(g_usb_device_event_get_status(event),
							  &error)) {
			g_task_report_error(self,
					    callback,
					    user_data,
					    g_usb_device_control_transfer_async,
					    error);
			return;
		}
		bytes = g_usb_device_event_get_bytes(event);
		if (bytes == NULL) {
			g_task_report_new_error(self,
						callback,
						user_data,
						g_usb_device_interrupt_transfer_async,
						G_IO_ERROR,
						G_IO_ERROR_INVALID_DATA,
						"no matching event data for %s",
						event_id);
			return;
		}
		if (!gusb_memcpy_bytes_safe(data, length, bytes, &error)) {
			g_task_report_error(self,
					    callback,
					    user_data,
					    g_usb_device_control_transfer_async,
					    error);
			return;
		}
		task = g_task_new(self, cancellable, callback, user_data);
		g_task_return_int(task, g_bytes_get_size(bytes));
		g_object_unref(task);
		return;
	}

	if (priv->handle == NULL) {
		g_usb_device_async_not_open_error(self,
						  callback,
						  user_data,
						  g_usb_device_interrupt_transfer_async);
		return;
	}

	/* save */
	if (g_usb_context_get_flags(priv->context) & G_USB_CONTEXT_FLAGS_SAVE_EVENTS)
		event = g_usb_device_save_event(self, event_id);

	req = g_slice_new0(GcmDeviceReq);
	req->transfer = libusb_alloc_transfer(0);
	req->event = event;

	task = g_task_new(self, cancellable, callback, user_data);
	g_task_set_task_data(task, req, (GDestroyNotify)g_usb_device_req_free);

	if (g_task_return_error_if_cancelled(task)) {
		g_object_unref(task);
		return;
	}

	/* fill in transfer details */
	libusb_fill_interrupt_transfer(req->transfer,
				       priv->handle,
				       endpoint,
				       data,
				       length,
				       g_usb_device_async_transfer_cb,
				       task,
				       timeout);

	/* submit transfer */
	rc = libusb_submit_transfer(req->transfer);
	if (rc < 0) {
		if (event != NULL)
			_g_usb_device_event_set_rc(event, rc);
		g_usb_device_libusb_error_to_gerror(self, rc, &error);
		g_task_return_error(task, error);
		g_object_unref(task);
	}

	/* setup cancellation after submission */
	if (cancellable != NULL) {
		req->cancellable = g_object_ref(cancellable);
		req->cancellable_id = g_cancellable_connect(req->cancellable,
							    G_CALLBACK(g_usb_device_cancelled_cb),
							    req,
							    NULL);
	}
}

/**
 * g_usb_device_interrupt_transfer_finish:
 * @self: a #GUsbDevice instance.
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
g_usb_device_interrupt_transfer_finish(GUsbDevice *self, GAsyncResult *res, GError **error)
{
	g_return_val_if_fail(G_USB_IS_DEVICE(self), -1);
	g_return_val_if_fail(g_task_is_valid(res, self), -1);
	g_return_val_if_fail(error == NULL || *error == NULL, -1);

	return g_task_propagate_int(G_TASK(res), error);
}

/**
 * g_usb_device_get_platform_id:
 * @self: a #GUsbDevice
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
g_usb_device_get_platform_id(GUsbDevice *self)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	g_return_val_if_fail(G_USB_IS_DEVICE(self), NULL);
	return priv->platform_id;
}

/**
 * g_usb_device_get_parent:
 * @self: a #GUsbDevice instance
 *
 * Gets the device parent if one exists.
 *
 * Return value: (transfer full): #GUsbDevice or %NULL
 *
 * Since: 0.2.4
 **/
GUsbDevice *
g_usb_device_get_parent(GUsbDevice *self)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	libusb_device *parent;

	/* sanity check */
	if (priv->device == NULL)
		return NULL;

	parent = libusb_get_parent(priv->device);
	if (parent == NULL)
		return NULL;

	return g_usb_context_find_by_bus_address(priv->context,
						 libusb_get_bus_number(parent),
						 libusb_get_device_address(parent),
						 NULL);
}

/**
 * g_usb_device_get_children:
 * @self: a #GUsbDevice instance
 *
 * Gets the device children if any exist.
 *
 * Return value: (transfer full) (element-type GUsbDevice): an array of #GUsbDevice
 *
 * Since: 0.2.4
 **/
GPtrArray *
g_usb_device_get_children(GUsbDevice *self)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	GPtrArray *children;
	g_autoptr(GPtrArray) devices = NULL;

	/* find any devices that have @self as a parent */
	children = g_ptr_array_new_with_free_func((GDestroyNotify)g_object_unref);
	devices = g_usb_context_get_devices(priv->context);
	for (guint i = 0; i < devices->len; i++) {
		GUsbDevice *device_tmp = g_ptr_array_index(devices, i);
		GUsbDevicePrivate *priv_tmp = GET_PRIVATE(device_tmp);
		if (priv->device == NULL)
			continue;
		if (priv->device == libusb_get_parent(priv_tmp->device))
			g_ptr_array_add(children, g_object_ref(device_tmp));
	}

	return children;
}

/**
 * g_usb_device_get_bus:
 * @self: a #GUsbDevice
 *
 * Gets the USB bus number for the device.
 *
 * Return value: The 8-bit bus number
 *
 * Since: 0.1.0
 **/
guint8
g_usb_device_get_bus(GUsbDevice *self)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	g_return_val_if_fail(G_USB_IS_DEVICE(self), 0);
	/* sanity check */
	if (priv->device == NULL)
		return 0x0;
	return libusb_get_bus_number(priv->device);
}

/**
 * g_usb_device_get_address:
 * @self: a #GUsbDevice
 *
 * Gets the USB address for the device.
 *
 * Return value: The 8-bit address
 *
 * Since: 0.1.0
 **/
guint8
g_usb_device_get_address(GUsbDevice *self)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	g_return_val_if_fail(G_USB_IS_DEVICE(self), 0);
	/* sanity check */
	if (priv->device == NULL)
		return 0x0;
	return libusb_get_device_address(priv->device);
}

/**
 * g_usb_device_get_port_number:
 * @self: a #GUsbDevice
 *
 * Gets the USB port number for the device.
 *
 * Return value: The 8-bit port number
 *
 * Since: 0.2.4
 **/
guint8
g_usb_device_get_port_number(GUsbDevice *self)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	g_return_val_if_fail(G_USB_IS_DEVICE(self), 0);
	/* sanity check */
	if (priv->device == NULL)
		return 0x0;
	return libusb_get_port_number(priv->device);
}

/**
 * g_usb_device_get_vid:
 * @self: a #GUsbDevice
 *
 * Gets the vendor ID for the device.
 *
 * Return value: an ID.
 *
 * Since: 0.1.0
 **/
guint16
g_usb_device_get_vid(GUsbDevice *self)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	g_return_val_if_fail(G_USB_IS_DEVICE(self), 0);
	return priv->desc.idVendor;
}

/**
 * g_usb_device_get_pid:
 * @self: a #GUsbDevice
 *
 * Gets the product ID for the device.
 *
 * Return value: an ID.
 *
 * Since: 0.1.0
 **/
guint16
g_usb_device_get_pid(GUsbDevice *self)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	g_return_val_if_fail(G_USB_IS_DEVICE(self), 0);
	return priv->desc.idProduct;
}

/**
 * g_usb_device_get_release:
 * @self: a #GUsbDevice
 *
 * Gets the BCD firmware version number for the device.
 *
 * Return value: a version number in BCD format.
 *
 * Since: 0.2.8
 **/
guint16
g_usb_device_get_release(GUsbDevice *self)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	g_return_val_if_fail(G_USB_IS_DEVICE(self), 0);
	return priv->desc.bcdDevice;
}

/**
 * g_usb_device_get_spec:
 * @self: a #GUsbDevice
 *
 * Gets the BCD specification revision for the device. For example,
 * `0x0110` indicates USB 1.1 and 0x0320 indicates USB 3.2
 *
 * Return value: a specification revision in BCD format.
 *
 * Since: 0.3.1
 **/
guint16
g_usb_device_get_spec(GUsbDevice *self)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	g_return_val_if_fail(G_USB_IS_DEVICE(self), 0);
	return priv->desc.bcdUSB;
}

/**
 * g_usb_device_get_vid_as_str:
 * @self: a #GUsbDevice
 *
 * Gets the vendor ID for the device as a string.
 *
 * Return value: an string ID, or %NULL if not available.
 *
 * Since: 0.2.4
 **/
const gchar *
g_usb_device_get_vid_as_str(GUsbDevice *self)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	g_return_val_if_fail(G_USB_IS_DEVICE(self), NULL);
	return _g_usb_context_lookup_vendor(priv->context, priv->desc.idVendor, NULL);
}

/**
 * g_usb_device_get_pid_as_str:
 * @self: a #GUsbDevice
 *
 * Gets the product ID for the device as a string.
 *
 * Return value: an string ID, or %NULL if not available.
 *
 * Since: 0.2.4
 **/
const gchar *
g_usb_device_get_pid_as_str(GUsbDevice *self)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	g_return_val_if_fail(G_USB_IS_DEVICE(self), NULL);
	return _g_usb_context_lookup_product(priv->context,
					     priv->desc.idVendor,
					     priv->desc.idProduct,
					     NULL);
}

/**
 * g_usb_device_get_configuration_index
 * @self: a #GUsbDevice
 *
 * Get the index for the active Configuration string descriptor
 * ie, iConfiguration.
 *
 * Return value: a string descriptor index.
 *
 * Since: 0.3.5
 **/
guint8
g_usb_device_get_configuration_index(GUsbDevice *self)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	GUsbDeviceEvent *event = NULL;
	struct libusb_config_descriptor *config;
	gint rc;
	guint8 index;
	g_autofree gchar *event_id = NULL;

	g_return_val_if_fail(G_USB_IS_DEVICE(self), 0);

	/* build event key either for load or save */
	if (priv->device == NULL ||
	    g_usb_context_get_flags(priv->context) & G_USB_CONTEXT_FLAGS_SAVE_EVENTS)
		event_id = g_strdup_printf("GetConfigurationIndex");

	/* emulated */
	if (priv->device == NULL) {
		GBytes *bytes;
		event = g_usb_device_load_event(self, event_id);
		if (event == NULL)
			return 0x0;
		bytes = g_usb_device_event_get_bytes(event);
		if (bytes == NULL && g_bytes_get_size(bytes) != 1)
			return 0x0;
		return ((const guint8 *)g_bytes_get_data(bytes, NULL))[0];
	}

	rc = libusb_get_active_config_descriptor(priv->device, &config);
	g_return_val_if_fail(rc == 0, 0);

	index = config->iConfiguration;

	/* save */
	if (g_usb_context_get_flags(priv->context) & G_USB_CONTEXT_FLAGS_SAVE_EVENTS) {
		event = g_usb_device_save_event(self, event_id);
		_g_usb_device_event_set_bytes_raw(event, &index, sizeof(index));
	}

	libusb_free_config_descriptor(config);
	return index;
}

/**
 * g_usb_device_get_manufacturer_index:
 * @self: a #GUsbDevice
 *
 * Gets the index for the Manufacturer string descriptor.
 *
 * Return value: a string descriptor index.
 *
 * Since: 0.1.0
 **/
guint8
g_usb_device_get_manufacturer_index(GUsbDevice *self)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	g_return_val_if_fail(G_USB_IS_DEVICE(self), 0);
	return priv->desc.iManufacturer;
}

/**
 * g_usb_device_get_device_class:
 * @self: a #GUsbDevice
 *
 * Gets the device class, typically a #GUsbDeviceClassCode.
 *
 * Return value: a device class number, e.g. 0x09 is a USB hub.
 *
 * Since: 0.1.7
 **/
guint8
g_usb_device_get_device_class(GUsbDevice *self)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	g_return_val_if_fail(G_USB_IS_DEVICE(self), 0);
	return priv->desc.bDeviceClass;
}

/**
 * g_usb_device_get_device_subclass:
 * @self: a #GUsbDevice
 *
 * Gets the device subclass qualified by the class number.
 * See g_usb_device_get_device_class().
 *
 * Return value: a device subclass number.
 *
 * Since: 0.2.4
 **/
guint8
g_usb_device_get_device_subclass(GUsbDevice *self)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	g_return_val_if_fail(G_USB_IS_DEVICE(self), 0);
	return priv->desc.bDeviceSubClass;
}

/**
 * g_usb_device_get_device_protocol:
 * @self: a #GUsbDevice
 *
 * Gets the device protocol qualified by the class and subclass numbers.
 * See g_usb_device_get_device_class() and g_usb_device_get_device_subclass().
 *
 * Return value: a device protocol number.
 *
 * Since: 0.2.4
 **/
guint8
g_usb_device_get_device_protocol(GUsbDevice *self)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	g_return_val_if_fail(G_USB_IS_DEVICE(self), 0);
	return priv->desc.bDeviceProtocol;
}

/**
 * g_usb_device_get_product_index:
 * @self: a #GUsbDevice
 *
 * Gets the index for the Product string descriptor.
 *
 * Return value: a string descriptor index.
 *
 * Since: 0.1.0
 **/
guint8
g_usb_device_get_product_index(GUsbDevice *self)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	g_return_val_if_fail(G_USB_IS_DEVICE(self), 0);
	return priv->desc.iProduct;
}

/**
 * g_usb_device_get_serial_number_index:
 * @self: a #GUsbDevice
 *
 * Gets the index for the Serial Number string descriptor.
 *
 * Return value: a string descriptor index.
 *
 * Since: 0.1.0
 **/
guint8
g_usb_device_get_serial_number_index(GUsbDevice *self)
{
	GUsbDevicePrivate *priv = GET_PRIVATE(self);
	g_return_val_if_fail(G_USB_IS_DEVICE(self), 0);
	return priv->desc.iSerialNumber;
}
