/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2011 Hans de Goede <hdegoede@redhat.com>
 * Copyright (C) 2011 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <gusb/gusb-context.h>

G_BEGIN_DECLS

#define G_USB_TYPE_DEVICE_LIST (g_usb_device_list_get_type())

G_DECLARE_DERIVABLE_TYPE(GUsbDeviceList, g_usb_device_list, G_USB, DEVICE_LIST, GObject)

struct _GUsbDeviceListClass {
	GObjectClass parent_class;
	/* Signals */
	void (*device_added)(GUsbDeviceList *self, GUsbDevice *device);
	void (*device_removed)(GUsbDeviceList *self, GUsbDevice *device);
	/*< private >*/
	/*
	 * If adding fields to this struct, remove corresponding
	 * amount of padding to avoid changing overall struct size
	 */
	gchar _gusb_reserved[64];
};

G_DEPRECATED_FOR(g_usb_context_new)
GUsbDeviceList *
g_usb_device_list_new(GUsbContext *context);

G_DEPRECATED
void
g_usb_device_list_coldplug(GUsbDeviceList *self);

G_DEPRECATED_FOR(g_usb_context_get_devices)
GPtrArray *
g_usb_device_list_get_devices(GUsbDeviceList *self);

G_DEPRECATED_FOR(g_usb_context_find_by_bus_address)
GUsbDevice *
g_usb_device_list_find_by_bus_address(GUsbDeviceList *self,
				      guint8 bus,
				      guint8 address,
				      GError **error);

G_DEPRECATED_FOR(g_usb_context_find_by_vid_pid)
GUsbDevice *
g_usb_device_list_find_by_vid_pid(GUsbDeviceList *self, guint16 vid, guint16 pid, GError **error);

G_END_DECLS
