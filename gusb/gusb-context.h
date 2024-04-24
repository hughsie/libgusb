/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2011 Hans de Goede <hdegoede@redhat.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <gusb/gusb-device.h>
#include <gusb/gusb-source.h>

G_BEGIN_DECLS

#define G_USB_TYPE_CONTEXT  (g_usb_context_get_type())
#define G_USB_CONTEXT_ERROR (g_usb_context_error_quark())

G_DECLARE_DERIVABLE_TYPE(GUsbContext, g_usb_context, G_USB, CONTEXT, GObject)

struct _GUsbContextClass {
	GObjectClass parent_class;
	void (*device_added)(GUsbContext *self, GUsbDevice *device);
	void (*device_removed)(GUsbContext *self, GUsbDevice *device);
	void (*device_changed)(GUsbContext *self, GUsbDevice *device);
	/*< private >*/
	/*
	 * If adding fields to this struct, remove corresponding
	 * amount of padding to avoid changing overall struct size
	 */
	gchar _gusb_reserved[61];
};

typedef enum { G_USB_CONTEXT_ERROR_INTERNAL } GUsbContextError;

/**
 * GUsbContextFlags:
 *
 * The flags to use for the context.
 **/
typedef enum {
	G_USB_CONTEXT_FLAGS_NONE = 0,
	G_USB_CONTEXT_FLAGS_AUTO_OPEN_DEVICES = 1 << 0,
	G_USB_CONTEXT_FLAGS_SAVE_EVENTS = 1 << 1,
	G_USB_CONTEXT_FLAGS_SAVE_REMOVED_DEVICES = 1 << 2,
	G_USB_CONTEXT_FLAGS_DEBUG = 1 << 3,
	/*< private >*/
	G_USB_CONTEXT_FLAGS_LAST
} GUsbContextFlags;

GQuark
g_usb_context_error_quark(void);

GUsbContext *
g_usb_context_new(GError **error);

void
g_usb_context_set_flags(GUsbContext *self, GUsbContextFlags flags);
GUsbContextFlags
g_usb_context_get_flags(GUsbContext *self);

G_DEPRECATED
GUsbSource *
g_usb_context_get_source(GUsbContext *self, GMainContext *main_ctx);
GMainContext *
g_usb_context_get_main_context(GUsbContext *self);
void
g_usb_context_set_main_context(GUsbContext *self, GMainContext *main_ctx);
guint
g_usb_context_get_hotplug_poll_interval(GUsbContext *self);
void
g_usb_context_set_hotplug_poll_interval(GUsbContext *self, guint hotplug_poll_interval);

void
g_usb_context_enumerate(GUsbContext *self);

gboolean
g_usb_context_load(GUsbContext *self, JsonObject *json_object, GError **error);
gboolean
g_usb_context_load_with_tag(GUsbContext *self,
			    JsonObject *json_object,
			    const gchar *tag,
			    GError **error);
gboolean
g_usb_context_save(GUsbContext *self, JsonBuilder *json_builder, GError **error);
gboolean
g_usb_context_save_with_tag(GUsbContext *self,
			    JsonBuilder *json_builder,
			    const gchar *tag,
			    GError **error);

void
g_usb_context_set_debug(GUsbContext *self, GLogLevelFlags flags);
GPtrArray *
g_usb_context_get_devices(GUsbContext *self);

GUsbDevice *
g_usb_context_find_by_bus_address(GUsbContext *self, guint8 bus, guint8 address, GError **error);

GUsbDevice *
g_usb_context_find_by_vid_pid(GUsbContext *self, guint16 vid, guint16 pid, GError **error);
GUsbDevice *
g_usb_context_find_by_platform_id(GUsbContext *self, const gchar *platform_id, GError **error);

GUsbDevice *
g_usb_context_wait_for_replug(GUsbContext *self,
			      GUsbDevice *device,
			      guint timeout_ms,
			      GError **error);

G_END_DECLS
