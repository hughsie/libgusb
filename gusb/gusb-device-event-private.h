/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2022 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <gusb/gusb-device-event.h>
#include <json-glib/json-glib.h>
#include <libusb.h>

G_BEGIN_DECLS

GUsbDeviceEvent *
_g_usb_device_event_new(const gchar *id);
void
_g_usb_device_event_set_bytes_raw(GUsbDeviceEvent *self, gconstpointer buf, gsize bufsz);
void
_g_usb_device_event_set_status(GUsbDeviceEvent *self, gint status);
void
_g_usb_device_event_set_rc(GUsbDeviceEvent *self, gint rc);

gboolean
_g_usb_device_event_load(GUsbDeviceEvent *self, JsonObject *json_object, GError **error);
gboolean
_g_usb_device_event_save(GUsbDeviceEvent *self, JsonBuilder *json_builder, GError **error);

G_END_DECLS
