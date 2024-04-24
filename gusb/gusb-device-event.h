/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2022 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define G_USB_TYPE_DEVICE_EVENT (g_usb_device_event_get_type())
G_DECLARE_FINAL_TYPE(GUsbDeviceEvent, g_usb_device_event, G_USB, DEVICE_EVENT, GObject)

const gchar *
g_usb_device_event_get_id(GUsbDeviceEvent *self);
GBytes *
g_usb_device_event_get_bytes(GUsbDeviceEvent *self);
gint
g_usb_device_event_get_status(GUsbDeviceEvent *self);
gint
g_usb_device_event_get_rc(GUsbDeviceEvent *self);
void
g_usb_device_event_set_bytes(GUsbDeviceEvent *self, GBytes *bytes);

G_END_DECLS
